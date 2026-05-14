#include "WebSocketServer.h"
#include "ContextIndexer.h"
#include "AssistantRole.h"
#include "Config.h"
#include "Logger.h"
#include <chrono>

WebSocketServer::WebSocketServer(const Config& config)
    : config(config), host(config.web_server_host), port(config.web_server_port) {
}

WebSocketServer::~WebSocketServer() {
    if (running) {
        stop();
    }
}

bool WebSocketServer::start(std::shared_ptr<ContextIndexer> indexer_ptr, 
                             std::shared_ptr<AssistantRole> assistant_ptr) {
    std::lock_guard<std::mutex> lock(mutex);

    if (running) {
        SPDLOG_WARN("WebSocket server is already running");
        return true; // It's already started, which is not an error
    }

    indexer = indexer_ptr;
    assistant = assistant_ptr;

    if (!indexer || !assistant) {
        SPDLOG_ERROR("WebSocketServer::start: indexer or assistant is null");
        return false;
    }

    running = true;
    SPDLOG_INFO("WebSocket message handler logic initialized.");
    return true;
}

void WebSocketServer::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    running = false;
    SPDLOG_INFO("WebSocket message handler logic stopped.");
}

void WebSocketServer::handleMessage(const std::string& message, 
                                     const std::function<void(const std::string&)>& send_back) {
    std::lock_guard<std::mutex> lock(mutex);
    try {
        auto json_msg = nlohmann::json::parse(message);
        WSMessage ws_msg;
        ws_msg.type = json_msg.value("type", "");
        ws_msg.data = json_msg.value("data", nlohmann::json::object());

        SPDLOG_DEBUG("Received WebSocket message of type: {}", ws_msg.type);

        if (ws_msg.type == "query") {
            handleQuery(ws_msg.data, send_back);
        } else if (ws_msg.type == "get_stats") {
            handleGetStats(send_back);
        } else if (ws_msg.type == "get_project_info") {
            handleGetProjectInfo(send_back);
        } else if (ws_msg.type == "ping") {
            WSResponse pong;
            pong.type = "pong";
            pong.data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            sendResponse(pong, send_back);
        } else {
            sendError("Unknown message type: " + ws_msg.type, send_back);
        }

    } catch (const nlohmann::json::exception& e) {
        sendError("Invalid JSON message: " + std::string(e.what()), send_back);
    }
}

void WebSocketServer::handleQuery(const nlohmann::json& data, 
                                   const std::function<void(const std::string&)>& send_back) {
    std::string query = data.value("text", "");
    if (query.empty()) {
        sendError("Query text is empty", send_back);
        return;
    }

    if (!indexer || !assistant) {
        sendError("Server components not initialized", send_back);
        return;
    }

    try {
        auto topResults = indexer->findTopK(query, config.top_k_results);
        AssistantResponse response = assistant->processQuery(query, topResults, *indexer);

        WSResponse ws_resp;
        ws_resp.type = "query_response";
        ws_resp.data["answer"] = response.text;
        ws_resp.data["is_final"] = response.is_final;
        sendResponse(ws_resp, send_back);

    } catch (const std::exception& e) {
        sendError("Error processing query: " + std::string(e.what()), send_back);
    }
}

void WebSocketServer::handleGetStats(const std::function<void(const std::string&)>& send_back) {
    if (!indexer) { sendError("Indexer not initialized", send_back); return; }
    WSResponse response;
    response.type = "stats";
    response.data["embeddings_count"] = indexer->getEmbeddingsCount();
    response.data["files_count"] = indexer->getFileCount();
    response.data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    sendResponse(response, send_back);
}

void WebSocketServer::handleReloadIndex(const std::function<void(const std::string&)>& send_back) {
    sendError("Reloading index via WebSocket is not yet implemented.", send_back);
}

void WebSocketServer::handleGetProjectInfo(const std::function<void(const std::string&)>& send_back) {
    if (!indexer) { sendError("Indexer not initialized", send_back); return; }
    WSResponse response;
    response.type = "project_info";
    response.data["files_count"] = indexer->getFileCount();
    response.data["embeddings_count"] = indexer->getEmbeddingsCount();
    sendResponse(response, send_back);
}

void WebSocketServer::sendResponse(const WSResponse& response, 
                                    const std::function<void(const std::string&)>& send_back) {
    send_back(response.serialize());
}

void WebSocketServer::sendError(const std::string& message, 
                                 const std::function<void(const std::string&)>& send_back) {
    WSResponse response;
    response.type = "error";
    response.data["message"] = message;
    sendResponse(response, send_back);
    SPDLOG_ERROR("WebSocket error: {}", message);
}
