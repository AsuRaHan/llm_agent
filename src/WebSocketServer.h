#pragma once

#include "Config.h"
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace httplib {
    namespace ws {
        class WebSocket;
    }
}

// Forward declarations
class ContextIndexer;
class AssistantRole;

struct WSResponse {
    std::string type;
    nlohmann::json data;
};

void to_json(nlohmann::json& j, const WSResponse& r);

struct UserSession {
    std::string id;
    nlohmann::json chat_history = nlohmann::json::array();
    bool is_waiting_confirmation = false;
    nlohmann::json pending_tool_call = nullptr;
};

class WebSocketServer {
public:
    explicit WebSocketServer(const Config& config);
    ~WebSocketServer();

    bool initialize(std::shared_ptr<ContextIndexer> indexer, std::shared_ptr<AssistantRole> assistant);

    void handleConnection(httplib::ws::WebSocket& ws);

private:
    void handleMessage(const std::string& message, const std::function<void(const std::string&)>& send_back);

    // Message handlers
    void handleQuery(const std::string& session_id, const nlohmann::json& data, const std::function<void(const std::string&)>& send_back);
    void handleConfirmation(const std::string& session_id, const nlohmann::json& data, const std::function<void(const std::string&)>& send_back);
    void handleGetStats(const std::function<void(const std::string&)>& send_back);
    void handleGetProjectInfo(const std::function<void(const std::string&)>& send_back);
    void handleClearHistory(const std::string& session_id);

    // Utility methods
    void sendResponse(const WSResponse& response, const std::function<void(const std::string&)>& send_back);
    void sendError(const std::string& message, const std::function<void(const std::string&)>& send_back);

    // Session management
    UserSession getOrCreateSession(const std::string& session_id);
    void updateSession(const UserSession& session);
    
    // Session persistence
    void saveSessions();
    void loadSessions();

private:
    const Config& config;
    std::shared_ptr<ContextIndexer> indexer;
    std::shared_ptr<AssistantRole> assistant;

    // Session storage
    std::mutex session_mutex;
    std::unordered_map<std::string, UserSession> sessions;
    const std::string sessions_db_path = ".shdata/sessions.json";
};