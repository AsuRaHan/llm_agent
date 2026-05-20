#include "ApiHandlers.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

ApiHandlers::ApiHandlers(std::shared_ptr<LLMProvider> provider, const Config& config, ContextIndexer& indexer)
    : config(config),
      assistant(std::make_shared<AssistantRole>(provider, config)),
      indexer(indexer),
      sessionManager(),
      webSocketServer(config, *assistant, indexer, sessionManager),
      fileWatcher(indexer)
{
    // "Знакомим" компоненты друг с другом через колбэки, чтобы избежать циклических зависимостей
    fileWatcher.setBroadcastCallback([this](const nlohmann::json& payload){
        this->webSocketServer.broadcast(payload);
    });

    webSocketServer.setFileWatcherControlCallback([this](const std::string& command){
        if (command == "freeze") {
            this->fileWatcher.freeze();
        } else if (command == "unfreeze") {
            this->fileWatcher.unfreeze();
        }
    });

    setupRoutes();
}

ApiHandlers::~ApiHandlers() {
    stop();
}

void ApiHandlers::setupRoutes() {
    svr.WebSocket("/ws", [this](const httplib::Request& req, httplib::ws::WebSocket& ws) {
        webSocketServer.handleConnection(req, ws);
    });

    auto base_dir = config.web_server_root_dir;
    if (!svr.set_mount_point("/", base_dir)) {
        SPDLOG_ERROR("Указанная директория frontend '{}' не существует.", base_dir);
    }
    
    svr.set_error_handler([base_dir](const httplib::Request&, httplib::Response &res) {
        std::ifstream file(base_dir + "/404.html");
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else {
            res.set_content("Not Found", "text/plain");
        }
        res.status = 404;
    });

    SPDLOG_INFO("HTTP маршруты настроены. Статика раздается из '{}'.", base_dir);
}

void ApiHandlers::start(const std::string& projectDir) {
    fileWatcher.start(projectDir);
    SPDLOG_INFO("Запуск веб-сервера на {}:{}", config.web_server_host, config.web_server_port);
    svr.listen(config.web_server_host.c_str(), config.web_server_port);
    SPDLOG_INFO("Веб-сервер остановлен.");
}

void ApiHandlers::stop() {
    // Проверяем, запущен ли сервер, чтобы избежать ошибок при повторном вызове
    if (svr.is_running()) {
        fileWatcher.stop();
        svr.stop();
    }
}