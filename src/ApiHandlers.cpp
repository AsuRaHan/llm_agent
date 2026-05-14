#include <httplib.h>
#include "ApiHandlers.h"
#include "ContextIndexer.h"
#include "AssistantRole.h"
#include "WebSocketServer.h"
#include "Config.h"
#include "Logger.h"
#include <memory>
#include <chrono>

ApiHandlers::ApiHandlers(const Config& config)
    : config(config), host(config.web_server_host), port(config.web_server_port), httpServer(nullptr) 
{
    wsServer = std::make_unique<WebSocketServer>(config);
}

// Destructor must be defined in the .cpp file where httplib::Server is a complete type.
ApiHandlers::~ApiHandlers() = default;

bool ApiHandlers::initialize(std::shared_ptr<ContextIndexer> indexer,
                              std::shared_ptr<AssistantRole> assistant) {    if (!indexer || !assistant || !wsServer) {
        SPDLOG_ERROR("ApiHandlers::initialize: null pointer passed");
        return false;
    }

    // Create HTTP server
    httpServer = std::make_shared<httplib::Server>();

    // Set base directory for serving static files
    if (!config.web_server_root_dir.empty()) {
        httpServer->set_base_dir(config.web_server_root_dir.c_str());
        SPDLOG_INFO("Serving static files from: {}", config.web_server_root_dir);
    }

    // Load custom 404 page content
    std::string notFoundFilePath = config.web_server_root_dir + "/404.html";
    std::ifstream notFoundFile(notFoundFilePath);
    if (notFoundFile.is_open()) {
        std::stringstream buffer;
        buffer << notFoundFile.rdbuf();
        notFoundPageContent = buffer.str();
        SPDLOG_INFO("Loaded custom 404 page from: {}", notFoundFilePath);
    } else {
        SPDLOG_WARN("Custom 404 page not found at: {}. Using default simple message.", notFoundFilePath);
        notFoundPageContent = "<h1>404 Not Found</h1><p>The requested resource was not found on this server.</p>";
    }

    // Set error handler for 404 pages
    httpServer->set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
        if (res.status == 404) {
            res.set_content(this->notFoundPageContent, "text/html");
            SPDLOG_DEBUG("Served custom 404 page for: {}", req.path);
        }
    });

    // ===== WebSocket Endpoint =====
    if (!wsServer->start(indexer, assistant)) {
        SPDLOG_ERROR("Failed to initialize WebSocket server logic");
        return false;
    }

    // ===== WebSocket Endpoint =====
    // WebSocket handler must be set directly on the server, not inside a Get/Post handler.
httpServer->WebSocket("/ws",
    [this](const httplib::Request& req, httplib::ws::WebSocket& ws) {
        // 1. Событие ON_OPEN (выполняется сразу при входе в лямбду)
        SPDLOG_INFO("[WebSocket] Connection opened from {}", req.remote_addr);

        std::string msg;
        try {
            // 2. Событие ON_MESSAGE (блокирующий цикл чтения сообщений)
            while (ws.read(msg)) {
                // Делегируем обработку в ваш класс wsServer
                // ВАЖНО: захватываем ws по ссылке осторожно, так как метод ws.send выполнится в контексте этого же потока
                this->wsServer->handleMessage(msg, [&ws](const std::string& response) {
                    ws.send(response);
                });
            }
        } 
        catch (const std::exception& e) {
            // 3. Событие ON_ERROR (если при чтении/записи произошло исключение)
            SPDLOG_ERROR("[WebSocket] Exception on connection from {}: {}", req.remote_addr, e.what());
        }

        // 4. Событие ON_CLOSE (выполняется, когда цикл ws.read завершился)
        SPDLOG_INFO("[WebSocket] Connection closed from {}", req.remote_addr);
    }
);

    // ===== CORS Headers Middleware =====
    httpServer->set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    httpServer->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        return true;
    });

    // ===== REST API Endpoints =====
    
    // GET /api/status
    httpServer->Get("/api/status", [indexer](const httplib::Request&, httplib::Response& res) {
        nlohmann::json response;
        response["status"] = "running";
        response["server_version"] = "1.0.0";
        response["embeddings_count"] = indexer->getEmbeddingsCount();
        response["files_count"] = indexer->getFileCount();
        response["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        res.set_content(response.dump(), "application/json");
        res.status = 200;
        SPDLOG_DEBUG("GET /api/status");
    });

    // GET /api/project-info
    httpServer->Get("/api/project-info", [indexer](const httplib::Request&, httplib::Response& res) {
        nlohmann::json response;
        response["files_count"] = indexer->getFileCount();
        response["embeddings_count"] = indexer->getEmbeddingsCount();
        response["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        res.set_content(response.dump(), "application/json");
        res.status = 200;
        SPDLOG_DEBUG("GET /api/project-info");
    });

    // GET /api/health
    httpServer->Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json response;
        response["status"] = "healthy";
        response["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        res.set_content(response.dump(), "application/json");
        res.status = 200;
    });

    // GET /api/server-config
    httpServer->Get("/api/server-config", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json response;
        response["host"] = host;
        response["port"] = port;
        response["api_version"] = "v1";
        response["api_endpoint"] = "/api/query";
        
        res.set_content(response.dump(), "application/json");
        res.status = 200;
        SPDLOG_DEBUG("GET /api/server-config");
    });

    // POST /api/query - Process user query
    httpServer->Post("/api/query", [indexer, assistant](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string query = body.value("text", "");
            
            if (query.empty()) {
                nlohmann::json error_resp;
                error_resp["type"] = "error";
                error_resp["message"] = "Query text is empty";
                res.set_content(error_resp.dump(), "application/json");
                res.status = 400;
                return;
            }

            SPDLOG_DEBUG("Processing query: {}", query);
            
            // Find relevant context
            auto topResults = indexer->findTopK(query, 5);
            
            // Process with assistant
            AssistantResponse response = assistant->processQuery(query, topResults, *indexer);
            
            // Build response
            nlohmann::json api_resp;
            api_resp["success"] = true;
            api_resp["answer"] = response.text;
            api_resp["context_count"] = topResults.size();
            api_resp["is_final"] = response.is_final;
            
            // Add context details
            nlohmann::json context_arr = nlohmann::json::array();
            for (const auto& result : topResults) {
                nlohmann::json ctx_item;
                ctx_item["file"] = result.filePath;
                ctx_item["score"] = result.score;
                ctx_item["text"] = result.chunkText.substr(0, 500);
                context_arr.push_back(ctx_item);
            }
            api_resp["context"] = context_arr;
            api_resp["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            
            // Используем error_handler_t::replace, чтобы избежать падения из-за невалидного UTF-8 ответа от LLM
            res.set_content(api_resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), "application/json");
            res.status = 200;
            SPDLOG_DEBUG("Query processed successfully");
            
        } catch (const std::exception& e) {
            nlohmann::json error_resp;
            error_resp["success"] = false;
            error_resp["message"] = std::string(e.what());
            res.set_content(error_resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), "application/json");
            res.status = 500;
            SPDLOG_ERROR("Query processing error: {}", e.what());
        }
    });

    // GET /api/stats
    httpServer->Get("/api/stats", [indexer](const httplib::Request&, httplib::Response& res) {
        try {
            nlohmann::json response;
            response["embeddings_count"] = indexer->getEmbeddingsCount();
            response["files_count"] = indexer->getFileCount();
            response["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            
            res.set_content(response.dump(), "application/json");
            res.status = 200;
        } catch (const std::exception& e) {
            nlohmann::json error_resp;
            error_resp["success"] = false;
            error_resp["message"] = std::string(e.what());
            res.set_content(error_resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), "application/json");
            res.status = 500;
            SPDLOG_ERROR("Stats error: {}", e.what());
        }
    });

    SPDLOG_INFO("HTTP/REST API handlers initialized on {}:{}", host, port);
    SPDLOG_INFO("WebSocket endpoint available at ws://{}:{}/ws", host, port);
    return true;
}

void ApiHandlers::startServer() {
    if (!httpServer) {
        SPDLOG_ERROR("ApiHandlers::startServer: Server not initialized");
        return;
    }

    SPDLOG_INFO("Starting HTTP/REST API server on {}:{}", host, port);
    
    if (!httpServer->listen(host.c_str(), port)) {
        SPDLOG_ERROR("Failed to start HTTP server on {}:{}", host, port);
        return;
    }
}

void ApiHandlers::stopServer() {
    if (!httpServer) {
        return;
    }

    httpServer->stop();
    SPDLOG_INFO("HTTP/REST API server stopped");
}

bool ApiHandlers::isRunning() const {
    return httpServer != nullptr;
}

std::string ApiHandlers::getServerAddress() const {
    return host + ":" + std::to_string(port);
}
