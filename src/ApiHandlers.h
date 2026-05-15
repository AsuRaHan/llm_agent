#pragma once

#include "httplib.h"
#include "Config.h"
#include "AssistantRole.h"
#include "ContextIndexer.h"
#include "SessionManager.h"
#include "WebSocketServer.h"
#include <memory>

class ApiHandlers {
public:
    ApiHandlers(const Config& config, AssistantRole& assistant, ContextIndexer& indexer);
    void start();
    void stop();

private:
    void setupRoutes();

    const Config& config;
    httplib::Server svr;
    
    AssistantRole& assistant;
    ContextIndexer& indexer;
    SessionManager sessionManager;
    WebSocketServer webSocketServer;
};