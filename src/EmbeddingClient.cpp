#include "EmbeddingClient.h"
#include "Logger.h"

using json = nlohmann::json;

EmbeddingClient::EmbeddingClient(std::shared_ptr<LLMProvider> provider)
    : llmProvider(provider)
{
}

std::vector<float> EmbeddingClient::getEmbedding(const std::string& text, const std::string& filename)
{
    SPDLOG_DEBUG("Generating embedding for '{}' (size: {} chars)...", filename, text.length());
    
    auto embedding = llmProvider->createEmbedding(text);

    if (embedding.empty()) {
        SPDLOG_ERROR("Error: Failed to get embedding for '{}'.", filename);
    }

    return embedding;
}

