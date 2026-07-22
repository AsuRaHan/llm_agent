#pragma once

#include "httplib.h"
#include "Config.h"
#include "AssistantRole.h"
#include "ContextIndexer.h"
#include "SessionManager.h"
#include "WebSocketServer.h"
#include "FileWatcher.h"
#include "LLMProvider.h"
#include <memory>

class ApiHandlers {
public:
    ApiHandlers(std::shared_ptr<LLMProvider> provider,
                const Config& config, 
                ContextIndexer& indexer,
                const std::string& projectDir,
                const std::filesystem::path& app_root_dir);
    void start(const std::string& projectDir);
    void stop();
    ~ApiHandlers();

private:
    void setupRoutes();

    const Config& config;
    httplib::Server svr;
    
    std::shared_ptr<AssistantRole> assistant;
    ContextIndexer& indexer;
    SessionManager sessionManager;
    WebSocketServer webSocketServer;
    FileWatcher fileWatcher;
};