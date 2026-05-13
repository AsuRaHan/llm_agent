#include "EmbeddingClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "Logger.h" // Include the new logger header
#include "Config.h"

// Use the alias for convenience
using json = nlohmann::json;

EmbeddingClient::EmbeddingClient(const Config& config)
    : config(config), cli(config.server_host, config.server_port)
{
    cli.set_connection_timeout(config.embedding_timeout_sec, 0);
}

std::vector<float> EmbeddingClient::getEmbedding(const std::string& text, const std::string& filename)
{
    std::string text_to_embed = text;
    SPDLOG_DEBUG("  [EmbeddingClient] Generating embedding for '{}' (size: {} chars)...", filename, text_to_embed.length());

    json body = {
        { "input", text_to_embed },
        { "model", config.embedding_model_name }
    };
    
    // Dump with an error handler to replace invalid UTF-8 sequences before sending
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    
    httplib::Headers headers;
    if (!config.api_key.empty()) {
        // Typically, the key is sent as "Bearer <key>"
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/embeddings", headers, body_str, "application/json");
        if (res) { // If we got any response (even an error status), we can break.
            break;
        }
        // If res is false, it's a connection error.
        SPDLOG_WARN("  [EmbeddingClient] Попытка {}/{} для '{}' не удалась. Ошибка соединения: {}. Повтор через {} мс...",
                    attempt, config.retry_count, filename, httplib::to_string(res.error()), config.retry_delay_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            // Assuming the structure is { "data": [ { "embedding": [ ... ] } ] }
            if (json_body.contains("data") && json_body["data"].is_array() && !json_body["data"].empty()) {
                const auto& first_item = json_body["data"][0];
                if (first_item.contains("embedding")) {
                    return first_item["embedding"].get<std::vector<float>>();
                }
            }
        } catch (const json::exception& e) { // Catch any nlohmann::json exception
            SPDLOG_ERROR("  [EmbeddingClient] Error: Failed to parse JSON response for '{}'. Details: {}", filename, e.what());
            return {};
        }
    } else {
        if (res) { // HTTP error
            SPDLOG_ERROR("  [EmbeddingClient] Error: Failed to get embedding for '{}'. Status: {}", filename, res->status);
            SPDLOG_ERROR("ОТВЕТ СЕРВЕРА: {}", res->body);
        } else { // Connection error
            auto err = res.error();
            SPDLOG_ERROR("  [EmbeddingClient] Error: Connection failed for '{}'. Details: {}", filename, httplib::to_string(err));
        }
    }

    return {}; // Should not be reached if logic is correct
}

bool EmbeddingClient::checkConnection() const {
    SPDLOG_INFO("Проверка соединения с сервером LLM на {}:{}...", cli.host(), cli.port());
    
    // Use a temporary client to set a shorter timeout specifically for the health check
    httplib::Client temp_cli(cli.host(), cli.port());
    temp_cli.set_connection_timeout(5, 0); // 5 seconds timeout for health check

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        // Pass headers to the Get request
        auto res = temp_cli.Get("/health", headers); // Standard health check endpoint

        if (res) {
            if (res->status == 200) {
                try {
                    auto json_body = json::parse(res->body);
                    if (json_body.contains("status") && json_body["status"] == "ok") {
                        SPDLOG_INFO("Соединение с сервером LLM успешно установлено, статус 'ok'.");
                        return true;
                    }
                    SPDLOG_WARN("Сервер LLM доступен, но статус не 'ok'. Ответ: {}. Попытка {}/{}", res->body, attempt, config.retry_count);
                } catch (const json::exception& e) {
                    SPDLOG_WARN("Сервер LLM доступен, но не удалось разобрать ответ /health: {}. Попытка {}/{}", e.what(), attempt, config.retry_count);
                }
            } else {
                SPDLOG_WARN("Сервер LLM доступен, но /health ответил с ошибкой. Статус: {}. Попытка {}/{}", res->status, attempt, config.retry_count);
            }
        } else {
            auto err = res.error();
            SPDLOG_WARN("Не удалось установить соединение с сервером LLM. Ошибка: {}. Попытка {}/{}", httplib::to_string(err), attempt, config.retry_count);
        }
        if (attempt < config.retry_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
        }
    }
    SPDLOG_ERROR("Не удалось подтвердить работоспособность сервера LLM после {} попыток.", config.retry_count);
    return false;
}
