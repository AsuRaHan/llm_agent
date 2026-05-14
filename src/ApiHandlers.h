#pragma once

#include "Config.h"
#include "httplib.h"
#include <memory>

// Forward declarations
class ContextIndexer;
class AssistantRole;
class WebSocketServer;

class ApiHandlers {
public:
    explicit ApiHandlers(const Config& config);
    ~ApiHandlers();

    bool initialize(std::shared_ptr<ContextIndexer> indexer, std::shared_ptr<AssistantRole> assistant);
    void startServer();
    void stopServer();

private:
    const Config& config;
    std::unique_ptr<httplib::Server> httpServer;
    std::unique_ptr<WebSocketServer> wsServer;
};