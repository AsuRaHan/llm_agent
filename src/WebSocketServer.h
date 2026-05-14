#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

class ContextIndexer;
class AssistantRole;
struct Config;
struct SearchResult;

// WebSocket message types
struct WSMessage {
    std::string type;  // "query", "reload_index", "get_stats", etc.
    nlohmann::json data;
};

// Response sent back to client
struct WSResponse {
    std::string type;
    nlohmann::json data;
    
    std::string serialize() const {
        nlohmann::json obj;
        obj["type"] = type;
        obj["data"] = data;
        return obj.dump();
    }
};

class WebSocketServer {
public:
    explicit WebSocketServer(const Config& config);
    ~WebSocketServer();

    /**
     * @brief Initialize and start the WebSocket server
     * @param indexer Pointer to the ContextIndexer
     * @param assistant Pointer to the AssistantRole
     * @return true if server started successfully
     */
    bool start(std::shared_ptr<ContextIndexer> indexer, std::shared_ptr<AssistantRole> assistant);

    /**
     * @brief Stop the server gracefully
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return running; }

    /**
     * @brief Get the server port
     */
    int getPort() const { return port; }

    /**
     * @brief Get the server host
     */
    std::string getHost() const { return host; }

    // Обработчик входящих сообщений, вызываемый из ApiHandlers
    // Сделан публичным, чтобы ApiHandlers мог делегировать обработку сообщений.
    void handleMessage(const std::string& message, const std::function<void(const std::string&)>& send_back);

private:


private:
    // Configuration
    const Config& config;
    std::string host;
    int port;
    
    // State
    std::atomic<bool> running{false};
    std::shared_ptr<ContextIndexer> indexer;
    std::shared_ptr<AssistantRole> assistant;
    
    // Mutex for thread safety
    std::mutex mutex;

    // Individual message handlers
    void handleQuery(const nlohmann::json& data, const std::function<void(const std::string&)>& send_back);
    void handleGetStats(const std::function<void(const std::string&)>& send_back);
    void handleReloadIndex(const std::function<void(const std::string&)>& send_back);
    void handleGetProjectInfo(const std::function<void(const std::string&)>& send_back);

    // Helper to send response
    void sendResponse(const WSResponse& response, const std::function<void(const std::string&)>& send_back);
    void sendError(const std::string& message, const std::function<void(const std::string&)>& send_back);
};
