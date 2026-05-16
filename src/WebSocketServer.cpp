#include "WebSocketServer.h"
#include "Logger.h"
#include "AssistantRole.h" // Для AssistantResponse
#include "UserSession.h"
#include "Config.h"
#include <functional>
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

std::string to_string(AgentStatus status) {
    switch (status) {
        case AgentStatus::IDLE: return "IDLE";
        case AgentStatus::THINKING: return "THINKING";
        case AgentStatus::AWAITING_CONFIRMATION: return "AWAITING_CONFIRMATION";
        case AgentStatus::GENERATING_PLAN: return "GENERATING_PLAN";
        case AgentStatus::AWAITING_PLAN_CONFIRMATION: return "AWAITING_PLAN_CONFIRMATION";
        case AgentStatus::EXECUTING_PLAN: return "EXECUTING_PLAN";
        case AgentStatus::AWAITING_ERROR_RECOVERY_DECISION: return "AWAITING_ERROR_RECOVERY_DECISION";
        default: return "UNKNOWN";
    }
}

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
        } else if (type == "confirm_plan") {
            handlePlanConfirmation(msg, ws_handle);
        } else if (type == "confirm_error_recovery") {
            handleErrorRecoveryConfirmation(msg, ws_handle);
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
    } else if (session->status == AgentStatus::AWAITING_PLAN_CONFIRMATION) {
        // This case is handled by the frontend receiving 'plan_generated'
        // But if a reconnect happens, we might need to resend the plan.
        // For now, we just indicate the status.
    }

    if (session->history.empty()) {
        response_data["greeting"] = "Привет! Я Smart Hammer, ваш AI-ассистент. Я проанализировал проект и готов к работе. Чем могу помочь?";
    }

    sendMessage(ws_handle, {
        {"type", "session_state"},
        {"data", response_data}
    });
}

void WebSocketServer::processPlanGeneration(std::shared_ptr<UserSession> session, const std::string& queryText, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = session->id;
    try {
        auto plan_steps = assistant.generatePlan(queryText);
        
        if (plan_steps.is_array() && !plan_steps.empty() && plan_steps[0].is_string() && plan_steps[0].get<std::string>().rfind("Ошибка:", 0) == 0) {
             SPDLOG_ERROR("Ошибка при генерации плана для сессии {}: {}", sessionId, plan_steps[0].get<std::string>());
             session->status = AgentStatus::IDLE;
             if (!session->history.empty() && session->history.back()["role"] == "user") {
                 session->history.erase(session->history.size() - 1);
             }
             sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", plan_steps[0].get<std::string>()}}}});
             return;
        }

        session->plan_steps = plan_steps;
        session->status = AgentStatus::AWAITING_PLAN_CONFIRMATION;

        sendMessage(ws_handle, {
            {"type", "plan_generated"},
            {"data", {{"steps", plan_steps}}}
        });

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Исключение при генерации плана для сессии {}: {}", sessionId, e.what());
        session->status = AgentStatus::IDLE;
        sendMessage(ws_handle, {
            {"type", "error"},
            {"data", {{"message", "Внутренняя ошибка сервера при генерации плана: " + std::string(e.what())}}}
        });
    }
}

void WebSocketServer::processAgentLogic(std::shared_ptr<UserSession> session, const std::string& queryText, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = session->id;
    try {
        auto send_thought = [this, ws_handle](const std::string& thought) {
            sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", thought}}}});
        };
        
        // --- ЛОГИКА ВЫПОЛНЕНИЯ ПЛАНА ---
        if (session->status == AgentStatus::EXECUTING_PLAN) {
            // Сбор RAG-контекста выполняется только один раз в самом начале плана

            // Итерируемся по шагам плана
            while (session->current_plan_step < (int)session->plan_steps.size()) {
                int current_step_idx = session->current_plan_step;
                std::string task = session->plan_steps[current_step_idx].get<std::string>();

                SPDLOG_INFO("Сессия {}: выполнение шага {}/{} плана: {}", sessionId, current_step_idx + 1, session->plan_steps.size(), task);
                sendMessage(ws_handle, {{"type", "plan_update"}, {"data", {{"current_step", current_step_idx}, {"steps", session->plan_steps}}}});

                // Для первого шага ищем RAG-контекст по оригинальному запросу. Для последующих - не ищем.
                auto context = (current_step_idx == 0) ? indexer.findTopK(session->original_user_query, config.top_k_results) : std::vector<SearchResult>();

                // ВАЖНО: Мы пушим ноту шага в историю ТОЛЬКО если мы не продолжаем работу после подтверждения опасного инструмента.
                // Если мы вернулись из handleConfirmation, вызов инструмента уже сидит на вершине истории, и пушить туда системную ноту нельзя.
                if (queryText != "CONTINUE_AFTER_TOOL") {
                    session->history.push_back({{"role", "user"}, {"content", "[SYSTEM_NOTE]: Текущий шаг плана: \"" + task + "\". Используй инструменты для его реализации. По окончании шага переходи к следующему."}});
                }

                // Вызываем ассистента. userQuery пустой, так как вся цепочка и инструкции сидят в session->history.
                // Контекст передаем только для первого шага.
                AssistantResponse response = assistant.processQuery("", context, indexer, session->history, send_thought);
                session->history = response.conversation_history;

                // Если шаг плана завершился с ошибкой, прерываем выполнение всего плана
                if (response.step_failed) {
                    SPDLOG_ERROR("Шаг плана для сессии {} завершился с ошибкой: {}", sessionId, response.error_message);
                    session->status = AgentStatus::AWAITING_ERROR_RECOVERY_DECISION;
                    sendMessage(ws_handle, {
                        {"type", "plan_error"}, 
                        {"data", {
                            {"error_message", response.error_message}, 
                            {"recovery_options", response.recovery_options}
                        }}
                    });
                    return; // Полностью выходим из потока пула, ждем решения.
                }

                // Перехват ручного подтверждения опасного инструмента
                if (response.requires_confirmation) {
                    SPDLOG_INFO("Выполнение плана для сессии {} приостановлено. Ожидание подтверждения операции.", sessionId);
                    session->status = AgentStatus::AWAITING_CONFIRMATION;
                    session->pending_tool_call = response.pending_tool_call;
                    sendMessage(ws_handle, {{"type", "action_required"}, {"data", {{"message", response.text}, {"tool_call", response.pending_tool_call}}}});
                    return; // Полностью выходим из потока пула. Ждем клика в UI.
                }

                // Шаг успешно закрыт моделью, инкрементируем счетчик плана
                session->current_plan_step++;
                // Сбрасываем флаг продолжения для следующих шагов в цикле while
                const_cast<std::string&>(queryText) = ""; 
            }

            // Все шаги плана успешно исчерпаны. Запрашиваем финальный аналитический отчет.
            SPDLOG_INFO("Все задачи плана для сессии {} выполнены. Сборка итогового отчета...", sessionId);
            session->history.push_back({{"role", "user"}, {"content", "Все пункты плана успешно реализованы. Подведи итог проделанной работы, перечисли измененные компоненты и предоставь финальный ответ пользователю."}});
            
            AssistantResponse final_response = assistant.processQuery("", {}, indexer, session->history, send_thought);
            
            // Глубокая очистка состояния сессии после закрытия кампании
            session->status = AgentStatus::IDLE;
            session->plan_steps = nlohmann::json::array();
            session->current_plan_step = -1;
            session->original_user_query = "";
            
            sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", final_response.text}}}});

        } else { 
            // --- СИНГЛ-ШОТ РЕЖИМ (ОБЫЧНЫЙ ДИАЛОГ БЕЗ ПЛАНА) ---
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

    // Лингвистическая эвристика на запуск автономного планирования
    const std::vector<std::string> plan_keywords = {"исправь", "реализуй", "перепиши", "добавь", "удали", "рефактор", "оптимизируй", "создай", "fix", "implement", "rewrite", "add", "refactor"};
    bool needs_plan = msg.value("data", nlohmann::json::object()).value("force_plan", false);
    
    if (!needs_plan) {
        std::string lower_query = queryText;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
            [](unsigned char c){ return std::tolower(c); });

        for (const auto& keyword : plan_keywords) {
            // Ищем ключевое слово в любой части строки (не только в начале)
            if (lower_query.find(keyword) != std::string::npos) {
                needs_plan = true;
                SPDLOG_DEBUG("Обнаружено ключевое слово '{}' в запросе. Запуск планирования.", keyword);
                break;
            }
        }
    } else {
        SPDLOG_INFO("Пользователь явно запросил планирование (force_plan=true).");
    }

    session->history.push_back({{"role", "user"}, {"content", queryText}});

    if (needs_plan) {
        SPDLOG_INFO("Запрос требует планирования. Запуск генерации плана для сессии {}.", sessionId);
        session->status = AgentStatus::GENERATING_PLAN;
        session->original_user_query = queryText;
        sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", "Запрос требует планирования. Составляю пошаговый план..."}}}});
        threadPool.enqueue([this, session, queryText, ws_handle] {
            processPlanGeneration(session, queryText, ws_handle);
        });
    } else {
        session->status = AgentStatus::THINKING;
        threadPool.enqueue([this, session, queryText, ws_handle] {
            processAgentLogic(session, queryText, ws_handle);
        });
    }
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
        
        // Полная очистка состояния плана, так как цепочка действий разорвана отказом оператора
        session->status = AgentStatus::IDLE;
        session->plan_steps = nlohmann::json::array();
        session->current_plan_step = -1;
        session->original_user_query = "";
        
        sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "Действие отклонено. Автономный план отменен по требованию пользователя."}}}});
        return;
    }

    // Сценарий 2: Человек ОДОБРИЛ вызов опасного инструмента
    SPDLOG_INFO("Пользователь подтвердил действие для сессии {}", sessionId);
    
    std::string pipeline_signal = "";
    if (!session->plan_steps.empty() && session->current_plan_step != -1) {
        session->status = AgentStatus::EXECUTING_PLAN;
        // Маркерный флаг, чтобы предохранить step_history от раздувания лишними системными нотами
        pipeline_signal = "CONTINUE_AFTER_TOOL"; 
    } else {
        session->status = AgentStatus::THINKING;
    }

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
        SPDLOG_INFO("Пользователь выбрал 'retry' для сессии {}. Возобновление выполнения плана.", sessionId);
        session->status = AgentStatus::EXECUTING_PLAN;
        // The current_plan_step is not incremented. We just re-run the logic for the same step.
        threadPool.enqueue([this, session, ws_handle] {
            processAgentLogic(session, "", ws_handle);
        });
    } else if (option == "skip") {
        SPDLOG_INFO("Пользователь выбрал 'skip' для сессии {}. Переход к следующему шагу.", sessionId);
        session->current_plan_step++; // Move to the next step
        session->status = AgentStatus::EXECUTING_PLAN;
        threadPool.enqueue([this, session, ws_handle] {
            processAgentLogic(session, "", ws_handle);
        });
    } else if (option == "re-plan") {
        SPDLOG_INFO("Пользователь выбрал 're-plan' для сессии {}. Запуск перепланирования.", sessionId);
        session->status = AgentStatus::GENERATING_PLAN;
        std::string new_query = session->original_user_query + "\n\n[SYSTEM_NOTE]: Предыдущая попытка выполнения плана провалилась на шаге " 
                              + std::to_string(session->current_plan_step + 1) + ". Учти это при создании нового плана.";
        
        sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", "План провалился. Запускаю перепланирование с учетом ошибки..."}}}});
        threadPool.enqueue([this, session, new_query, ws_handle] {
            processPlanGeneration(session, new_query, ws_handle);
        });
    } else { // "abort" or any other unknown option
        SPDLOG_INFO("Пользователь выбрал 'abort' для сессии {}. План отменен.", sessionId);
        session->status = AgentStatus::IDLE;
        session->plan_steps = nlohmann::json::array();
        session->current_plan_step = -1;
        session->original_user_query = "";
        sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "Хорошо, план отменен. Чем я могу помочь?"}}}});
    }
}

void WebSocketServer::handlePlanConfirmation(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = msg.value("session_id", "");
    const auto& data = msg.value("data", nlohmann::json::object());
    bool confirmed = data.value("confirmed", false);
    
    if (sessionId.empty()) return;

    auto session = sessionManager.getSession(sessionId);

    if (session->status != AgentStatus::AWAITING_PLAN_CONFIRMATION) {
        sendMessage(ws_handle, {{"type", "error"}, {"data", {{"message", "Нет плана, ожидающего подтверждения."}}}});
        return;
    }

    if (confirmed) {
        // Проверяем, прислал ли клиент отредактированный список шагов
        if (data.contains("steps") && data["steps"].is_array()) {
            session->plan_steps = data["steps"];
            SPDLOG_INFO("Пользователь утвердил и отредактировал план для сессии {}", sessionId);
        } else {
            SPDLOG_INFO("Пользователь утвердил план для сессии {}", sessionId);
        }

        // Проверяем, не оказался ли план пустым после редактирования
        if (session->plan_steps.empty()) {
            SPDLOG_WARN("Пользователь утвердил пустой план для сессии {}. План отменен.", sessionId);
            session->status = AgentStatus::IDLE;
            session->current_plan_step = -1;
            session->original_user_query = "";
            sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "План пуст. Задачи для выполнения отсутствуют."}}}});
            return;
        }

        session->status = AgentStatus::EXECUTING_PLAN;
        session->current_plan_step = 0;

        threadPool.enqueue([this, session, ws_handle] {
            processAgentLogic(session, "", ws_handle);
        });
    } else {
        SPDLOG_INFO("Пользователь отклонил план для сессии {}", sessionId);
        session->status = AgentStatus::IDLE;
        session->plan_steps = nlohmann::json::array();
        session->current_plan_step = -1;
        session->original_user_query = "";
        sendMessage(ws_handle, {{"type", "query_response"}, {"data", {{"answer", "Хорошо, план отменен. Чем я могу помочь?"}}}});
    }
}

void WebSocketServer::handleClearHistory(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle) {
    (void)ws_handle; 
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