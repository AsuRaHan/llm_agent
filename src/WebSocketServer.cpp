#include "WebSocketServer.h"
#include "Logger.h"
#include "AssistantRole.h" // Для AssistantResponse
#include "Config.h"
#include <functional>

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

    // Используем RAII для гарантированной очистки указателя на ws
    auto cleanup = detail::scope_exit([&] {
        SPDLOG_INFO("Завершение обработки WebSocket соединения.");
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
        } else if (type == "clear_history") {
            handleClearHistory(msg, ws_handle);
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

void WebSocketServer::processAgentLogic(std::shared_ptr<UserSession> session, const std::string& queryText, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = session->id;
    try {
        auto send_thought = [this, ws_handle](const std::string& thought) {
            sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", thought}}}});
        };

        // Для нового запроса ищем контекст. Для подтверждения или продолжения - нет.
        auto context = queryText.empty() ? std::vector<SearchResult>() : indexer.findTopK(queryText, config.top_k_results);

        AssistantResponse response = assistant.processQuery(queryText, context, indexer, session->history, send_thought);

        session->history = response.conversation_history;

        if (response.requires_confirmation) {
            session->status = AgentStatus::AWAITING_CONFIRMATION;
            session->pending_tool_call = response.pending_tool_call;
            sendMessage(ws_handle, {{"type", "action_required"}, {"data", {{"message", response.text}, {"tool_call", response.pending_tool_call}}}});
        } else {
            session->status = AgentStatus::IDLE;
            sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", response.text}}}});
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Ошибка при обработке логики агента для сессии {}: {}", sessionId, e.what());
        session->status = AgentStatus::IDLE;
        sendMessage(ws_handle, {
            {"type", "error"},
            {"data", {{"message", "Внутренняя ошибка сервера в фоновом потоке: " + std::string(e.what())}}}
        });
    }
}

void WebSocketServer::handleQuery(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = msg.value("session_id", "");
    std::string queryText = msg.value("data", nlohmann::json::object()).value("text", "");

    if (sessionId.empty() || queryText.empty()) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "session_id and text are required for query."}}}});
        return;
    }

    auto session = sessionManager.getSession(sessionId);
    
    if (session->status != AgentStatus::IDLE) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "Агент занят. Пожалуйста, подождите."}}}});
        return;
    }

    session->status = AgentStatus::THINKING;
    session->history.push_back({{"role", "user"}, {"content", queryText}});

    // Отправляем тяжелую задачу в пул потоков, не блокируя основной поток.
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

    session->status = AgentStatus::THINKING;
    auto pending_call = session->pending_tool_call;
    session->pending_tool_call = nullptr;

    if (!confirmed) {
        SPDLOG_INFO("Пользователь отклонил действие для сессии {}", sessionId);
        session->history.push_back({{"role", "tool"}, {"tool_call_id", pending_call["id"]}, {"content", "{\"result\": \"User denied execution.\"}"}});
        session->status = AgentStatus::IDLE;
        sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "Хорошо, я отменил действие."}}}});
        return;
    }

    SPDLOG_INFO("Пользователь подтвердил действие для сессии {}", sessionId);
    // Отправляем продолжение диалога в пул потоков.
    threadPool.enqueue([this, session, ws_handle] {
        processAgentLogic(session, "", ws_handle); // Пустой queryText означает продолжение
    });
}

void WebSocketServer::handleClearHistory(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    (void)ws_handle; // Not used in this handler
    std::string sessionId = msg.value("session_id", "");
    if (sessionId.empty()) return;
    SPDLOG_INFO("Очистка истории для сессии: {}", sessionId);
    sessionManager.clearSession(sessionId);
}

void WebSocketServer::sendMessage(std::shared_ptr<SafeWsHandle> ws_handle, const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(ws_handle->mtx);
    if (!ws_handle->ws || !ws_handle->ws->is_open()) {
        SPDLOG_WARN("Попытка отправить сообщение на невалидный или закрытый WebSocket.");
        return;
    }
    try {
        std::string payload_str = payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        SPDLOG_TRACE("Отправка сообщения: {}", payload_str);
        ws_handle->ws->send(payload_str);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Не удалось отправить сообщение: {}", e.what());
    }
}