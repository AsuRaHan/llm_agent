#include "WebSocketServer.h"
#include "ContextIndexer.h"
#include "AssistantRole.h"
#include "Config.h"
#include "Logger.h"
#include <chrono>

WebSocketServer::WebSocketServer(const Config& config)
    : config(config), host("localhost"), port(9000) {
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
        return false;
    }

    indexer = indexer_ptr;
    assistant = assistant_ptr;

    if (!indexer || !assistant) {
        SPDLOG_ERROR("WebSocketServer::start: indexer or assistant is null");
        return false;
    }

    running = true;
    SPDLOG_INFO("WebSocket server stub initialized (REST API used instead on {}:{})", host, port);
    
    return true;
}

void WebSocketServer::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    running = false;
    SPDLOG_INFO("WebSocket server stopping");
}

void WebSocketServer::handleMessage(const std::string& message, 
                                     const std::function<void(const std::string&)>& send_back) {
    // Stub implementation - not used with REST API
    SPDLOG_DEBUG("WebSocket message stub: {}", message.substr(0, 100));
}

void WebSocketServer::handleQuery(const nlohmann::json& data, 
                                   const std::function<void(const std::string&)>& send_back) {
    // Stub implementation
    SPDLOG_DEBUG("WebSocket query handler stub");
}

void WebSocketServer::handleGetStats(const std::function<void(const std::string&)>& send_back) {
    // Stub implementation
    SPDLOG_DEBUG("WebSocket stats handler stub");
}

void WebSocketServer::handleReloadIndex(const std::function<void(const std::string&)>& send_back) {
    // Stub implementation
    SPDLOG_DEBUG("WebSocket reload handler stub");
}

void WebSocketServer::handleGetProjectInfo(const std::function<void(const std::string&)>& send_back) {
    // Stub implementation
    SPDLOG_DEBUG("WebSocket project info handler stub");
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
