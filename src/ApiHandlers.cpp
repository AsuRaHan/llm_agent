#include "ApiHandlers.h"
#include "WebSocketServer.h"
#include "Logger.h"

ApiHandlers::ApiHandlers(const Config& config)
    : config(config),
      httpServer(std::make_unique<httplib::Server>()),
      wsServer(std::make_unique<WebSocketServer>(config))
{}

ApiHandlers::~ApiHandlers() {
    stopServer();
}

bool ApiHandlers::initialize(std::shared_ptr<ContextIndexer> indexer, std::shared_ptr<AssistantRole> assistant) {
    if (!httpServer) {
        SPDLOG_CRITICAL("HTTP server is not initialized.");
        return false;
    }

    if (!wsServer->initialize(indexer, assistant)) {
        SPDLOG_CRITICAL("Failed to initialize WebSocket server.");
        return false;
    }

    // --- WebSocket Endpoint ---
    httpServer->WebSocket("/ws", [this](const httplib::Request& /*req*/, httplib::ws::WebSocket& ws) {
        wsServer->handleConnection(ws);
    });

    // --- Static File Serving for Frontend ---
    auto& web_config = config.web_server_root_dir;
    if (!web_config.empty() && std::filesystem::exists(web_config)) {
        if (httpServer->set_mount_point("/", web_config.c_str())) {
            SPDLOG_INFO("Serving frontend files from '{}'", web_config);
        } else {
            SPDLOG_ERROR("Failed to set mount point for frontend files at '{}'", web_config);
        }
    } else {
        SPDLOG_WARN("Frontend root directory '{}' not found or not set. Frontend will not be served.", web_config);
    }

    return true;
}

void ApiHandlers::startServer() {
    SPDLOG_INFO("Starting HTTP/WebSocket server on {}:{}", config.web_server_host, config.web_server_port);
    httpServer->listen(config.web_server_host.c_str(), config.web_server_port);
}

void ApiHandlers::stopServer() {
    if (httpServer && httpServer->is_running()) {
        httpServer->stop();
    }
}