#include "EmbeddingClient.h"
#include <iostream>

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
        std::cout << "  [EmbeddingClient] Text for '" << filename << "' is too long (" << text.length() << " chars), truncating to 1500." << std::endl;
        text_to_embed = text_to_embed.substr(0, 1500);
    }

    std::cout << "  [EmbeddingClient] Generating embedding for '" << filename << "' (size: " << text_to_embed.length() << " chars)..." << std::endl;

    json body = {
        { "input", text_to_embed },
        { "model", "any" }
    };
    
    auto res = cli.Post("/v1/embeddings", body.dump(), "application/json");

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
        } catch (const json::parse_error& e) {
            std::cerr << "  [EmbeddingClient] Error: Failed to parse JSON response for '" << filename << "'. Details: " << e.what() << std::endl;
            return {};
        }
    } else {
        std::cerr << "  [EmbeddingClient] Error: Failed to get embedding for '" << filename << "'. Status: " 
                  << (res ? res->status : -1) << std::endl;
        if(res) {
            std::cout << "SERVER RESPONSE: " << res->body << std::endl;
        }
        return {};
    }

    return {}; // Should not be reached if logic is correct
}
