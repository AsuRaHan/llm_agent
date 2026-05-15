#pragma once

#include "httplib.h"
#include "nlohmann/json.hpp"
#include "AssistantRole.h"
#include "ContextIndexer.h"
#include "SessionManager.h"
#include "ThreadPool.h"
#include <memory>
#include <mutex>

struct Config; // Forward declaration

// Потокобезопасный "хэндл" для WebSocket соединения, чтобы избежать use-after-free
struct SafeWsHandle {
    std::mutex mtx;
    httplib::ws::WebSocket* ws = nullptr;
};

class WebSocketServer {
public:
    WebSocketServer(const Config& config, AssistantRole& assistant, ContextIndexer& indexer, SessionManager& sessionManager);

    void handleConnection(const httplib::Request& req, httplib::ws::WebSocket& ws);

private:
    void handleMessage(const std::string& raw_message, std::shared_ptr<SafeWsHandle> ws_handle);
    void processAgentLogic(std::shared_ptr<UserSession> session, const std::string& queryText, std::shared_ptr<SafeWsHandle> ws_handle);

    void handleSyncSession(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle);
    void handleQuery(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle);
    void handleConfirmation(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle);
    void handleClearHistory(const nlohmann::json& msg, std::shared_ptr<SafeWsHandle> ws_handle);
    void sendMessage(std::shared_ptr<SafeWsHandle> ws_handle, const nlohmann::json& payload);

    const Config& config;
    AssistantRole& assistant;
    ContextIndexer& indexer;
    SessionManager& sessionManager;
    ThreadPool threadPool;
};