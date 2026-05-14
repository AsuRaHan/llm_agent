#include "WebSocketServer.h"
#include "ContextIndexer.h"
#include "AssistantRole.h"
#include "Logger.h"
#include <chrono>
#include <fstream>
#include <thread>

using json = nlohmann::json;

void to_json(nlohmann::json& j, const WSResponse& r) {
    j = nlohmann::json{{"type", r.type}, {"data", r.data}};
}

WebSocketServer::WebSocketServer(const Config& config) : config(config) {}

WebSocketServer::~WebSocketServer() = default;

bool WebSocketServer::initialize(std::shared_ptr<ContextIndexer> indexer, std::shared_ptr<AssistantRole> assistant) {
    this->indexer = indexer;
    this->assistant = assistant;
    if (!this->indexer || !this->assistant) {
        SPDLOG_ERROR("WebSocketServer requires a valid indexer and assistant.");
        return false;
    }
    loadSessions();
    return true;
}

void WebSocketServer::handleConnection(httplib::ws::WebSocket& ws) {
    // Лямбда-функция для отправки ответов обратно клиенту
    auto send_func = [&ws](const std::string& data) {
        if (ws.is_open()) { // Проверяем, открыт ли сокет прямо сейчас
            ws.send(data);
        }
    };

    SPDLOG_INFO("Новое WebSocket соединение открыто.");

    // Буферная строка, куда cpp-httplib будет записывать входящие данные
    std::string msg;

    // Цикл работает автоматически, пока соединение активно.
    // ws.read(msg) блокирует поток до прихода нового сообщения.
    // Если клиент отключится, read() вернет false и цикл завершится.
    while (ws.read(msg)) {
        if (!msg.empty()) {
            handleMessage(msg, send_func);
        }
    }
    SPDLOG_INFO("WebSocket соединение закрыто.");
}

void WebSocketServer::handleMessage(const std::string& message, const std::function<void(const std::string&)>& send_back) {
    try {
        auto json_msg = nlohmann::json::parse(message);
        std::string msg_type = json_msg.value("type", "");
        std::string session_id = json_msg.value("session_id", "default_session");
        nlohmann::json msg_data = json_msg.value("data", nlohmann::json::object());

        SPDLOG_DEBUG("Received WS message: type={}, session={}", msg_type, session_id);

        if (msg_type == "query") {
            handleQuery(session_id, msg_data, send_back);
        } else if (msg_type == "confirm_action") {
            handleConfirmation(session_id, msg_data, send_back);
        } else if (msg_type == "get_stats") {
            handleGetStats(send_back);
        } else if (msg_type == "clear_history") {
            handleClearHistory(session_id);
        } else if (msg_type == "get_project_info") {
            handleGetProjectInfo(send_back);
        } else if (msg_type == "ping") {
            WSResponse pong;
            pong.type = "pong";
            pong.data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            sendResponse(pong, send_back);
        } else {
            sendError("Unknown message type: " + msg_type, send_back);
        }

    } catch (const nlohmann::json::exception& e) {
        sendError("Invalid JSON message: " + std::string(e.what()), send_back);
    }
}

void WebSocketServer::handleQuery(const std::string& session_id, const nlohmann::json& data, const std::function<void(const std::string&)>& send_back) {
    std::string query = data.value("text", "");
    if (query.empty()) {
        sendError("Query text is empty", send_back);
        return;
    }

    UserSession session = getOrCreateSession(session_id);
    if (session.is_waiting_confirmation) {
        sendError("Ожидается подтверждение или отмена предыдущего действия!", send_back);
        return;
    }

    auto send_thought = [this, &send_back](const std::string& thought_text) {
        WSResponse thought_msg;
        thought_msg.type = "agent_thought";
        thought_msg.data["message"] = thought_text;
        sendResponse(thought_msg, send_back);
    };

    try {
        send_thought("Ищу релевантный контекст в репозитории...");
        auto topResults = indexer->findTopK(query, config.top_k_results);
        
        send_thought("Контекст собран. Анализирую задачу через LLM...");

        AssistantResponse response = assistant->processQuery(query, topResults, *indexer, session.chat_history, send_thought);

        if (response.requires_confirmation) {
            session.chat_history = response.conversation_history;
            session.is_waiting_confirmation = true;
            session.pending_tool_call = response.pending_tool_call;
            updateSession(session);

            WSResponse ws_resp;
            ws_resp.type = "action_required";
            ws_resp.data["message"] = response.text;
            ws_resp.data["tool_call"] = response.pending_tool_call;
            sendResponse(ws_resp, send_back);
        } else {
            session.chat_history = response.conversation_history;
            updateSession(session);

            WSResponse ws_resp;
            ws_resp.type = "query_response";
            ws_resp.data["answer"] = response.text;
            ws_resp.data["is_final"] = response.is_final;
            sendResponse(ws_resp, send_back);
        }

    } catch (const std::exception& e) {
        sendError("Error processing query: " + std::string(e.what()), send_back);
    }
}

void WebSocketServer::handleConfirmation(const std::string& session_id, const nlohmann::json& data, const std::function<void(const std::string&)>& send_back) {
    UserSession session = getOrCreateSession(session_id);

    if (!session.is_waiting_confirmation || session.pending_tool_call.is_null()) {
        sendError("Нет активных действий для подтверждения", send_back);
        return;
    }

    bool confirmed = data.value("confirmed", false);

    auto send_thought = [this, &send_back](const std::string& thought_text) {
        WSResponse thought_msg;
        thought_msg.type = "agent_thought";
        thought_msg.data["message"] = thought_text;
        sendResponse(thought_msg, send_back);
    };

    try {
        if (confirmed) {
            send_thought("Действие подтверждено. Выполняю инструмент...");

            const auto& call = session.pending_tool_call;
            const auto& function_call = call["function"];
            std::string tool_name = function_call["name"];
            nlohmann::json tool_args = function_call["arguments"];
            if (tool_args.is_string()) {
                tool_args = json::parse(tool_args.get<std::string>());
            }
            std::string tool_id = call["id"];

            std::string tool_result = assistant->toolManager->executeTool(tool_name, tool_args, indexer.get());

            session.chat_history.push_back({
                {"role", "tool"},
                {"tool_call_id", tool_id},
                {"content", tool_result}
            });

            // Re-enter the main processing loop to get the next step from the agent.
            // We pass an empty query because the user hasn't typed anything new.
            // The agent will decide what to do next based on the tool's result in the history.
            AssistantResponse final_response = assistant->processQuery(
                /*userQuery=*/"", 
                /*initialContext=*/{}, // No new RAG context needed
                *indexer, 
                session.chat_history, 
                send_thought
            );
            session.chat_history = final_response.conversation_history;
            
            // The response from processQuery could be another confirmation request or a final answer.
            // We must handle both cases to support chained tool calls.
            if (final_response.requires_confirmation) {
                session.is_waiting_confirmation = true;
                session.pending_tool_call = final_response.pending_tool_call;
                updateSession(session);

                WSResponse ws_resp;
                ws_resp.type = "action_required";
                ws_resp.data["message"] = final_response.text;
                ws_resp.data["tool_call"] = final_response.pending_tool_call;
                sendResponse(ws_resp, send_back);
            } else {
                session.is_waiting_confirmation = false;
                session.pending_tool_call = nullptr;
                updateSession(session);

                WSResponse ws_resp;
                ws_resp.type = "query_response";
                ws_resp.data["answer"] = final_response.text;
                ws_resp.data["is_final"] = final_response.is_final;
                sendResponse(ws_resp, send_back);
            }

        } else {
            send_thought("Действие отменено пользователем.");
            session.chat_history.push_back({
                {"role", "user"}, 
                {"content", "Я отменил это действие. Не выполняй его."}
            });
            
            // Ask the agent what to do now that the action was cancelled.
            AssistantResponse alt_response = assistant->processQuery(
                /*userQuery=*/"", 
                /*initialContext=*/{},
                *indexer, 
                session.chat_history, 
                send_thought
            );
            session.chat_history = alt_response.conversation_history;

            if (alt_response.requires_confirmation) {
                session.is_waiting_confirmation = true;
                session.pending_tool_call = alt_response.pending_tool_call;
                updateSession(session);

                WSResponse ws_resp;
                ws_resp.type = "action_required";
                ws_resp.data["message"] = alt_response.text;
                ws_resp.data["tool_call"] = alt_response.pending_tool_call;
                sendResponse(ws_resp, send_back);
            } else {
                session.is_waiting_confirmation = false;
                session.pending_tool_call = nullptr;
                updateSession(session);

                WSResponse ws_resp;
                ws_resp.type = "query_response";
                ws_resp.data["answer"] = alt_response.text;
                ws_resp.data["is_final"] = alt_response.is_final;
                sendResponse(ws_resp, send_back);
            }
        }

        // Session state is now updated inside the if/else blocks, so no need to reset it here.

    } catch (const std::exception& e) {
        sendError("Ошибка при обработке подтверждения: " + std::string(e.what()), send_back);
        // Also reset state on error
        session.is_waiting_confirmation = false;
        session.pending_tool_call = nullptr;
        updateSession(session);
    }
}

void WebSocketServer::handleGetStats(const std::function<void(const std::string&)>& send_back) {
    WSResponse resp;
    resp.type = "stats_response";
    resp.data["indexed_files"] = indexer->getFileCount();
    resp.data["embeddings"] = indexer->getEmbeddingsCount();
    sendResponse(resp, send_back);
}

void WebSocketServer::handleGetProjectInfo(const std::function<void(const std::string&)>& send_back) {
    std::string greeting = assistant->generateProjectSummaryGreeting(indexer->getFileCount(), indexer->getEmbeddingsCount());
    WSResponse resp;
    resp.type = "project_info_response";
    resp.data["greeting"] = greeting;
    resp.data["file_count"] = indexer->getFileCount();
    resp.data["embedding_count"] = indexer->getEmbeddingsCount();
    sendResponse(resp, send_back);
}

void WebSocketServer::handleClearHistory(const std::string& session_id) {
    SPDLOG_INFO("Получен запрос на очистку истории для сессии: {}", session_id);
    std::lock_guard<std::mutex> lock(session_mutex);
    if (sessions.find(session_id) != sessions.end()) {
        // Clear the chat history array for the specific session
        sessions[session_id].chat_history = nlohmann::json::array();
        // Also reset confirmation state just in case
        sessions[session_id].is_waiting_confirmation = false;
        sessions[session_id].pending_tool_call = nullptr;
        
        // Persist the changes
        saveSessions();
        SPDLOG_INFO("История для сессии {} успешно очищена.", session_id);
    } else {
        SPDLOG_WARN("Попытка очистить историю для несуществующей сессии: {}", session_id);
    }
}

void WebSocketServer::sendResponse(const WSResponse& response, const std::function<void(const std::string&)>& send_back) {
    nlohmann::json j = response;
    send_back(j.dump());
}

void WebSocketServer::sendError(const std::string& message, const std::function<void(const std::string&)>& send_back) {
    SPDLOG_ERROR("WS Error: {}", message);
    WSResponse err_resp;
    err_resp.type = "error";
    err_resp.data["message"] = message;
    sendResponse(err_resp, send_back);
}

UserSession WebSocketServer::getOrCreateSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(session_mutex);
    if (sessions.find(session_id) == sessions.end()) {
        SPDLOG_INFO("Creating new WebSocket session: {}", session_id);
        sessions[session_id] = UserSession{session_id};
    }
    return sessions[session_id];
}

void WebSocketServer::updateSession(const UserSession& session) {
    std::lock_guard<std::mutex> lock(session_mutex);
    sessions[session.id] = session;
    SPDLOG_DEBUG("Обновление сессии {} и сохранение в файл...", session.id);
    saveSessions(); // Persist session state on every update
}

void WebSocketServer::saveSessions() {
    // ВНИМАНИЕ: Этот метод предполагает, что `session_mutex` уже заблокирован вызывающей стороной.
    // Не добавляйте сюда блокировку мьютекса, чтобы избежать взаимной блокировки (deadlock).
    SPDLOG_TRACE("Выполняется сохранение всех сессий в файл: {}", sessions_db_path);

    json sessions_json;
    for (const auto& [id, session] : sessions) {
        sessions_json[id] = {
            {"id", session.id},
            {"chat_history", session.chat_history},
            {"is_waiting_confirmation", session.is_waiting_confirmation},
            {"pending_tool_call", session.pending_tool_call}
        };
    }

    std::ofstream file(sessions_db_path);
    if (file.is_open()) {
        file << sessions_json.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
    } else {
        SPDLOG_ERROR("Не удалось открыть файл для сохранения сессий: {}", sessions_db_path);
    }
}

void WebSocketServer::loadSessions() {
    std::lock_guard<std::mutex> lock(session_mutex);
    std::ifstream file(sessions_db_path);
    if (!file.is_open()) {
        SPDLOG_INFO("Файл сессий '{}' не найден. Начинаем с чистого листа.", sessions_db_path);
        return;
    }

    try {
        json sessions_json;
        file >> sessions_json;
        for (auto& [id, session_json] : sessions_json.items()) {
            sessions[id] = UserSession{
                session_json.value("id", id),
                session_json.value("chat_history", json::array()),
                session_json.value("is_waiting_confirmation", false),
                session_json.value("pending_tool_call", nullptr)
            };
        }
        SPDLOG_INFO("Загружено {} сессий из файла '{}'.", sessions.size(), sessions_db_path);
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Ошибка парсинга файла сессий '{}': {}", sessions_db_path, e.what());
    }
}