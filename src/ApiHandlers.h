#pragma once

#include <memory>
#include <string>
#include <functional>

class ContextIndexer;
class AssistantRole;
class WebSocketServer;
struct Config;

// Forward declare httplib::Server to avoid including the full header here.
namespace httplib {
    class Server;
}

class ApiHandlers {
public:
    explicit ApiHandlers(const Config& config);
    ~ApiHandlers(); // Required for pimpl with smart pointers
    
    /**
     * @brief Initialize HTTP server with all API endpoints
     * @param indexer Shared pointer to ContextIndexer
     * @param assistant Shared pointer to AssistantRole
     * @return true if server initialized successfully
     */
    bool initialize(std::shared_ptr<ContextIndexer> indexer,
                    std::shared_ptr<AssistantRole> assistant);

    /**
     * @brief Start the HTTP server (blocks current thread)
     */
    void startServer();

    /**
     * @brief Stop the HTTP server
     */
    void stopServer();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const;

    /**
     * @brief Get server address (host:port)
     */
    std::string getServerAddress() const;

private:
    const Config& config;
    std::string host;
    int port;
    std::shared_ptr<httplib::Server> httpServer; // Используем shared_ptr для управления временем жизни сервера
    std::string notFoundPageContent; // Содержимое страницы 404
    std::unique_ptr<WebSocketServer> wsServer; // Обработчик логики WebSocket
};
