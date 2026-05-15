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
    // Устанавливаем таймауты. Connection timeout - на установку соединения, read/write - на операции.
    // Для эмбеддингов важен read_timeout, так как модель может долго их считать.
    cli.set_connection_timeout(5, 0); // 5 секунд на подключение
    cli.set_read_timeout(config.embedding_timeout_sec, 0);
}

std::vector<float> EmbeddingClient::getEmbedding(const std::string& text, const std::string& filename)
{
    std::string text_to_embed = text;
    SPDLOG_DEBUG("Generating embedding for '{}' (size: {} chars)...", filename, text_to_embed.length());

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
        SPDLOG_WARN("Попытка {}/{} для '{}' не удалась. Ошибка соединения: {}. Повтор через {} мс...",
                    attempt, config.retry_count, filename, httplib::to_string(res.error()), config.retry_delay_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            // Assuming the structure is { "data": [ { "embedding": [ ... ] } ] }
            if (json_body.contains("data") && json_body["data"].is_array() && !json_body["data"].empty()) {
                const auto& first_item = json_body["data"][0];
                if (first_item.contains("embedding") && first_item["embedding"].is_array()) {
                    return first_item["embedding"].get<std::vector<float>>();
                }
            }
            // If we reach here, the structure is valid JSON but not what we expected. Log it.
            SPDLOG_ERROR("Error: Unexpected JSON structure in embedding response for '{}'. Body: {}", filename, res->body);
        } catch (const json::exception& e) { // Catch any nlohmann::json exception
            SPDLOG_ERROR("Error: Failed to parse JSON response for '{}'. Details: {}", filename, e.what());
        }
    } else {
        if (res) { // HTTP error
            SPDLOG_ERROR("Error: Failed to get embedding for '{}'. Status: {}. Body: {}", filename, res->status, res->body);
        } else { // Connection error
            auto err = res.error();
            SPDLOG_ERROR("Error: Connection failed for '{}'. Details: {}", filename, httplib::to_string(err));
        }
    }

    return {}; // Return empty vector on any failure
}

bool EmbeddingClient::probeEmbeddingEndpoint() const {
    SPDLOG_DEBUG("Проверка эндпоинта /v1/embeddings...");
    httplib::Client probe_cli(cli.host(), cli.port());
    probe_cli.set_connection_timeout(2, 0); // Короткий таймаут для проверки

    // Мы намеренно отправляем невалидный запрос (пустое тело).
    // Ожидаемый ответ от существующего эндпоинта - это ошибка клиента (4xx) или сервера (5xx),
    // но не ошибка "Not Found" (404).
    auto res = probe_cli.Post("/v1/embeddings", "{}", "application/json");

    if (!res) {
        SPDLOG_WARN("Проверка не удалась: не удалось подключиться к эндпоинту /v1/embeddings. Ошибка: {}", httplib::to_string(res.error()));
        return false;
    }

    if (res->status == 404) {
        SPDLOG_WARN("Проверка не удалась: эндпоинт /v1/embeddings не найден (сервер вернул 404).");
        return false;
    }
    
    // Любой другой статус (например, 400, 401, 422, 500) подтверждает, что эндпоинт существует.
    SPDLOG_DEBUG("Проверка успешна: эндпоинт /v1/embeddings существует (сервер ответил статусом {}).", res->status);
    return true;
}

std::optional<ServerProperties> EmbeddingClient::fetchServerProperties() const {
    SPDLOG_INFO("Получение свойств с сервера LLM на {}:{}...", cli.host(), cli.port());
    
    httplib::Client temp_cli(cli.host(), cli.port());
    temp_cli.set_connection_timeout(5, 0);
    temp_cli.set_read_timeout(10, 0); // Таймаут на чтение ответа

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    auto res = temp_cli.Get("/props", headers);

    if (!res) {
        SPDLOG_CRITICAL("Не удалось подключиться к серверу LLM для получения свойств. Ошибка: {}", httplib::to_string(res.error()));
        return std::nullopt;
    }

    if (res->status != 200) {
        SPDLOG_CRITICAL("Сервер LLM ответил ошибкой на запрос /props. Статус: {}.", res->status);
        return std::nullopt;
    }

    try {
        json body = json::parse(res->body);
        ServerProperties props;

        // Адаптация к новому и старому формату /props
        props.model_path = body.value("model_path", "unknown");
        props.chat_template = body.value("chat_template", "");
        props.context_size = body.value("default_generation_settings", json::object()).value("n_ctx", body.value("context_size", 4096));

        // В новом формате /props нет поля "embedding", поэтому проверяем его наличие или делаем прямой запрос
        props.embedding_enabled = body.value("embedding", probeEmbeddingEndpoint());

        SPDLOG_INFO("Свойства сервера успешно получены:");
        SPDLOG_INFO("  - Модель: {}", props.model_path);
        SPDLOG_INFO("  - Шаблон чата: '{}'", props.chat_template.empty() ? "не указан" : props.chat_template);
        SPDLOG_INFO("  - Размер контекста: {}", props.context_size);
        SPDLOG_INFO("  - Поддержка эмбеддингов: {}", props.embedding_enabled ? "Да" : "Нет");

        return props;
    } catch (const json::exception& e) {
        SPDLOG_CRITICAL("Не удалось разобрать JSON-ответ от /props: {}", e.what());
        return std::nullopt;
    }
}
