#include "EmbeddingClient.h"
#include <iostream>
#include "Logger.h" // Include the new logger header

// Use the alias for convenience
using json = nlohmann::json;

EmbeddingClient::EmbeddingClient() : cli("localhost", 8080)
{
    cli.set_connection_timeout(60, 0); // 60 seconds
}

std::vector<float> EmbeddingClient::getEmbedding(const std::string& text, const std::string& filename)
{
    std::string text_to_embed = text;
    if (text_to_embed.length() > 1500) {
        SPDLOG_WARN("  [EmbeddingClient] Text for '{}' is too long ({} chars), truncating to 1500.", filename, text.length());
        text_to_embed = text_to_embed.substr(0, 1500);
    }

    SPDLOG_DEBUG("  [EmbeddingClient] Generating embedding for '{}' (size: {} chars)...", filename, text_to_embed.length());

    json body = {
        { "input", text_to_embed },
        { "model", "any" }
    };
    
    // Dump with an error handler to replace invalid UTF-8 sequences before sending
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    auto res = cli.Post("/v1/embeddings", body_str, "application/json");

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
        SPDLOG_ERROR("  [EmbeddingClient] Error: Failed to get embedding for '{}'. Status: {}",
                     filename, (res ? res->status : -1));
        if(res) {
            SPDLOG_ERROR("ОТВЕТ СЕРВЕРА: {}", res->body);
        }
        return {};
    }

    return {}; // Should not be reached if logic is correct
}
