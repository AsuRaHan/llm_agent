#pragma once

#include <string>
#include <vector>
#include <thread>
#include <optional>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "Config.h" // Include for ServerProperties and Config

class EmbeddingClient
{
public:
    explicit EmbeddingClient(const Config& config);

    /**
     * @brief Generates an embedding for the given text.
     * 
     * @param text The text to embed.
     * @return std::vector<float> The embedding vector.
     */
    std::vector<float> getEmbedding(const std::string& text, const std::string& filename);
    std::optional<ServerProperties> fetchServerProperties() const;

private:
    const Config& config;
    httplib::Client cli;
    bool probeEmbeddingEndpoint() const;
};
