#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <string>
#include <vector>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

class EmbeddingClient
{
public:
    EmbeddingClient();

    /**
     * @brief Generates an embedding for the given text.
     * 
     * @param text The text to embed.
     * @return std::vector<float> The embedding vector.
     */
    std::vector<float> getEmbedding(const std::string& text, const std::string& filename);

private:
    httplib::Client cli;
};
