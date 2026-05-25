#include "WebSocketServer.h"
#include "Logger.h"
#include "AssistantRole.h" // Для AssistantResponse
#include "UserSession.h"
#include "Config.h"
#include <functional>
#include <unordered_map>
#include <sstream>
#include <algorithm>

// Helper for RAII-style cleanup. This is to fix the missing detail::scope_exit.
// It seems the version of httplib.h you are using does not provide it.
namespace detail {
    template <typename F>
    struct scope_exit {
        scope_exit(F f) : f_(std::move(f)) {}
        ~scope_exit() { f_(); }
    private:
        F f_;
    };
} // namespace detail

WebSocketServer::WebSocketServer(const Config& config, AssistantRole& assistant, ContextIndexer& indexer, SessionManager& sessionManager)
    : config(config),
      assistant(assistant), 
      indexer(indexer), 
      sessionManager(sessionManager),
      threadPool(4) // Инициализируем пул с 4 рабочими потоками
{}

void WebSocketServer::handleConnection(const httplib::Request& req, httplib::ws::WebSocket& ws) {
    (void)req; // req is not used for now
    SPDLOG_INFO("Новое WebSocket соединение установлено.");

    auto ws_handle = std::make_shared<SafeWsHandle>();
    {
        std::lock_guard<std::mutex> lock(ws_handle->mtx);
        ws_handle->ws = &ws;
    }

    // Добавляем клиента в общий список для broadcast-рассылок
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients[&ws] = ws_handle;
    }

    // Используем RAII для гарантированной очистки указателя на ws
    auto cleanup = detail::scope_exit([&] {
        SPDLOG_INFO("Завершение обработки WebSocket соединения.");
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(&ws);
        }
        std::lock_guard<std::mutex> lock(ws_handle->mtx);
        ws_handle->ws = nullptr;
    });

    while (ws.is_open()) {
        std::string data;
        auto result = ws.read(data);

        if (result == httplib::ws::ReadResult::Fail) {
            SPDLOG_INFO("WebSocket соединение закрыто или произошла ошибка чтения.");
            break;
        }

        if (result == httplib::ws::ReadResult::Text) {
            handleMessage(data, ws_handle);
        }
    }
}

void WebSocketServer::handleMessage(const std::string& raw_message, std::shared_ptr<SafeWsHandle> ws_handle) {
    try {
        SPDLOG_TRACE("Получено сообщение: {}", raw_message);
        auto msg = nlohmann::json::parse(raw_message);
        std::string type = msg.value("type", "");

        if (type == "ping") {
            sendMessage(ws_handle, {{"type", "pong"}});
            return;
        }

        if (type == "sync_session") {
            handleSyncSession(msg, ws_handle);
        } else if (type == "query") {
            handleQuery(msg, ws_handle);
        } else if (type == "confirm_action") {
            handleConfirmation(msg, ws_handle);
        } else if (type == "confirm_error_recovery") {
            handleErrorRecoveryConfirmation(msg, ws_handle);
        } else if (type == "clear_history") {
            handleClearHistory(msg, ws_handle);
        } else if (type == "control_file_watcher") {
            handleControlFileWatcher(msg);        
        } else if (type == "interrupt_agent") { // <-- НАШ НОВЫЙ ОБРАБОТЧИК
            std::string sessionId = msg.value("session_id", "");
            if (!sessionId.empty()) {
                SPDLOG_INFO("Received interrupt request for session {}", sessionId);
                sessionManager.interruptSession(sessionId);
            }
        } else {
            SPDLOG_WARN("Получен неизвестный тип сообщения: {}", type);
            sendMessage(ws_handle, {
                {"type", "error"},
                {"data", {{"message", "Unknown message type: " + type}}}
            });
        }
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_ERROR("Не удалось разобрать JSON: {}", e.what());
        sendMessage(ws_handle, {
            {"type", "error"},
            {"data", {{"message", "Invalid JSON format."}}}
        });
    }
}

void WebSocketServer::handleSyncSession(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = msg.value("session_id", "");
    if (sessionId.empty()) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "session_id is required for sync."}}}});
        return;
    }

    SPDLOG_INFO("Синхронизация сессии: {}", sessionId);
    auto session = sessionManager.getSession(sessionId);

    nlohmann::json response_data;
    response_data["history"] = session->history;
    response_data["status"] = to_string(session->status);

    if (session->status == AgentStatus::AWAITING_CONFIRMATION) {
        if (session->pending_tool_call != nullptr) {
            response_data["confirmation_data"]["message"] = "Я собираюсь использовать инструмент '" + session->pending_tool_call["function"]["name"].get<std::string>() + "'. Разрешить выполнение?";
            response_data["confirmation_data"]["tool_call"] = session->pending_tool_call;
        }
    }

    if (session->history.empty()) {
        response_data["greeting"] = "Привет! Я Smart Hammer, ваш AI-ассистент. Я проанализировал проект и готов к работе. Чем могу помочь?";
    }

    sendMessage(ws_handle, {
        {"type", "session_state"},
        {"data", response_data}
    });
}

void WebSocketServer::setSessionIdle(std::shared_ptr<UserSession> session) {
    if (session->status == AgentStatus::IDLE) {
        // Если мы уже в состоянии IDLE, возможно, что-то пошло не так.
        // На всякий случай размораживаем FileWatcher.
        if (file_watcher_control_callback) {
            file_watcher_control_callback("unfreeze");
        }
        return;
    }

    SPDLOG_INFO("Сессия {} переходит в состояние IDLE. Снятие блокировок.", session->id);
    session->status = AgentStatus::IDLE;
    session->pending_tool_call = nullptr;

    if (file_watcher_control_callback) {
        file_watcher_control_callback("unfreeze");
    }
}

void WebSocketServer::processAgentLogic(std::shared_ptr<UserSession> session, const std::string& queryText, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = session->id;
    try {
        auto send_thought = [this, ws_handle](const std::string& thought) {
            sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", thought}}}});
        };
        auto send_stream_chunk = [this, ws_handle](const std::string& token) {
            sendMessage(ws_handle, {{"type", "llm_token"}, {"data", {{"token", token}}}});
        };
        
        // --- ЕДИНЫЙ ЦИКЛ ReAct ---
        std::vector<SearchResult> context;
        if (!queryText.empty()) {
            auto& searcher = indexer.getSearcher();
            auto& fileIndex = indexer.getFileIndexer().getFileIndex();
            context = searcher.findTopK(queryText, config.top_k_results, fileIndex);
        }

        AssistantResponse response = assistant.processQuery(queryText, context, indexer, session->history, send_thought, send_stream_chunk, session->is_interrupted);
        sendMessage(ws_handle, {{"type", "stream_end"}});

        session->history = response.conversation_history;
        sessionManager.saveSessions();

        if (response.step_failed) {
            SPDLOG_ERROR("Шаг для сессии {} завершился с ошибкой: {}", sessionId, response.error_message);
            session->status = AgentStatus::AWAITING_ERROR_RECOVERY_DECISION;

            sendMessage(ws_handle, {
                {"type", "step_error"},
                {"data", {
                    {"title", "❗️ Ошибка обработки запроса:"},
                    {"error_message", response.error_message}, 
                    {"recovery_options", response.recovery_options}
                }}
            });
            // Не переводим в IDLE, ждем решения пользователя
        } else if (response.requires_confirmation) {
            session->status = AgentStatus::AWAITING_CONFIRMATION;
            session->pending_tool_call = response.pending_tool_call;
            sendMessage(ws_handle, {{"type", "action_required"}, {"data", {{"message", response.text}, {"tool_call", response.pending_tool_call}}}});
        } else {
            setSessionIdle(session);
            sessionManager.saveSessions();
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Ошибка при обработке логики агента для сессии {}: {}", sessionId, e.what());
        setSessionIdle(session);
        sendMessage(ws_handle, {
            {"type", "error"},
            {"data", {{"message", "Внутренняя ошибка сервера в фоновом потоке: " + std::string(e.what())}}}
        });
    }
}

void WebSocketServer::handleQuery(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = msg.value("session_id", "");
    auto data = msg.value("data", nlohmann::json::object());
    std::string queryText = data.value("text", "");
    nlohmann::json images = data.value("images", nlohmann::json::array());

    if (sessionId.empty() || (queryText.empty() && images.empty())) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "Для запроса требуется session_id и текст или изображение."}}}});
        return;
    }

    auto session = sessionManager.getSession(sessionId);
    session->is_interrupted = false; // Сбрасываем флаг прерывания перед новой задачей
    
    if (session->status != AgentStatus::IDLE) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "Агент занят. Пожалуйста, подождите."}}}});
        return;
    }

    // "Замораживаем" FileWatcher перед запуском любой логики агента, чтобы избежать гонки с индексатором.
    if (file_watcher_control_callback) {
        SPDLOG_INFO("Запрос от пользователя, сессия {}. Активация заморозки FileWatcher.", sessionId);
        file_watcher_control_callback("freeze");
    }

    // Construct the user message
    if (images.empty()) {
        // Simple text message
        session->history.push_back({{"role", "user"}, {"content", queryText}});
    } else {
        // Multi-part message with text and images
        nlohmann::json user_message_content = nlohmann::json::array();
        if (!queryText.empty()) {
            user_message_content.push_back({{"type", "text"}, {"text", queryText}});
        }
        for (const auto& image_obj : images) {
            if (image_obj.is_object() && image_obj.contains("type") && image_obj.contains("data")) {
                std::string mime_type = image_obj.value("type", "image/jpeg");
                std::string base64_data = image_obj.value("data", "");
                if (!base64_data.empty()) {
                    user_message_content.push_back({
                        {"type", "image_url"},
                        {"image_url", {{"url", "data:" + mime_type + ";base64," + base64_data}}}
                    });
                }
            }
        }
        session->history.push_back({{"role", "user"}, {"content", user_message_content}});
    }

    // Сохраняем сессию сразу после добавления сообщения пользователя в историю
    SPDLOG_DEBUG("Сохранение сессии {} после получения нового запроса от пользователя.", sessionId);
    sessionManager.saveSessions();

    session->status = AgentStatus::THINKING;
    threadPool.enqueue([this, session, queryText, ws_handle] {
        processAgentLogic(session, queryText, ws_handle);
    });
}

void WebSocketServer::handleConfirmation(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = msg.value("session_id", "");
    bool confirmed = msg.value("data", nlohmann::json::object()).value("confirmed", false);

    if (sessionId.empty()) return;

    auto session = sessionManager.getSession(sessionId);

    if (session->status != AgentStatus::AWAITING_CONFIRMATION) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "Нет действия, ожидающего подтверждения."}}}});
        return;
    }

    auto pending_call = session->pending_tool_call;
    session->pending_tool_call = nullptr;

    // Сценарий 1: Человек ОТКЛОНИЛ вызов опасного инструмента
    if (!confirmed) {
        SPDLOG_INFO("Пользователь отклонил действие для сессии {}", sessionId);
        session->history.push_back({{"role", "tool"}, {"tool_call_id", pending_call["id"]}, {"content", "{\"result\": \"Error: User explicitly denied execution of this tool.\"}"}});
        // The agent needs to know the user denied it, so we continue the logic loop.
    }

    // Сценарий 2: Человек ОДОБРИЛ вызов опасного инструмента
    SPDLOG_INFO("Пользователь подтвердил действие для сессии {}", sessionId);
    
    // We continue the agent logic. The query text is empty, which signals a continuation.
    std::string pipeline_signal = ""; 
    session->status = AgentStatus::THINKING;

    // Отправляем выполнение обратно в пул потоков
    threadPool.enqueue([this, session, pipeline_signal, ws_handle] {
        processAgentLogic(session, pipeline_signal, ws_handle); 
    });
}

void WebSocketServer::handleErrorRecoveryConfirmation(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = msg.value("session_id", "");
    std::string option = msg.value("data", nlohmann::json::object()).value("option", "abort");

    if (sessionId.empty()) return;

    auto session = sessionManager.getSession(sessionId);

    if (session->status != AgentStatus::AWAITING_ERROR_RECOVERY_DECISION) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "Нет ошибки, ожидающей решения."}}}});
        return;
    }

    if (option == "retry") {
        SPDLOG_INFO("Пользователь выбрал 'retry' для сессии {}. Возобновление выполнения.", sessionId);
        session->status = AgentStatus::THINKING;
        threadPool.enqueue([this, session, ws_handle] {
            // Pass an empty query to signal a continuation of the previous failed step. processAgentLogic(session, "RETRY_STEP", ws_handle);
            // The agent logic will pick up the existing history and re-try the last LLM call.
            processAgentLogic(session, "", ws_handle);
        });
    } else if (option == "skip") {
        SPDLOG_INFO("Пользователь выбрал 'skip' для сессии {}. Отмена текущей задачи.", sessionId);
        setSessionIdle(session);
        sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "Хорошо, текущая задача отменена. Чем я могу помочь?"}}}});
    } else { // "abort" or any other unknown option
        SPDLOG_INFO("Пользователь выбрал 'abort' для сессии {}. Задача отменена.", sessionId);
        setSessionIdle(session);
        sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "Хорошо, задача отменена. Чем я могу помочь?"}}}});
    }
}

void WebSocketServer::handleControlFileWatcher(const nlohmann::json& msg) {
    if (file_watcher_control_callback) {
        std::string command = msg.value("data", nlohmann::json::object()).value("command", "");
        if (!command.empty()) {
            SPDLOG_INFO("Получена команда для FileWatcher: {}", command);
            file_watcher_control_callback(command);
        }
    }
}

void WebSocketServer::setFileWatcherControlCallback(std::function<void(const std::string&)> cb) {
    file_watcher_control_callback = std::move(cb);
}

void WebSocketServer::handleClearHistory(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    (void)ws_handle; 
    std::string sessionId = msg.value("session_id", "");
    if (sessionId.empty()) return;

    auto session = sessionManager.getSession(sessionId);
    // Clear history and reset state
    sessionManager.clearSession(sessionId);
    // Unfreeze the file watcher if it was frozen
    if (file_watcher_control_callback) {
        file_watcher_control_callback("unfreeze");
    }
}

void WebSocketServer::sendMessage(std::shared_ptr<SafeWsHandle> ws_handle, const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(ws_handle->mtx);
    if (!ws_handle->ws || !ws_handle->ws->is_open()) {
        SPDLOG_WARN("Попытка отправить сообщение на невалидный или закрытый WebSocket.");
        return;
    }
    try {
        std::string payload_str = payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        // SPDLOG_TRACE("Отправка сообщения: {}", payload_str);
        ws_handle->ws->send(payload_str);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Не удалось отправить сообщение: {}", e.what());
    }
}

void WebSocketServer::broadcast(const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    SPDLOG_DEBUG("Рассылка сообщения {} клиентам.", clients.size());
    for (auto const& [ws_ptr, ws_handle] : clients) {
        sendMessage(ws_handle, payload);
    }
}