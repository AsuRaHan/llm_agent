#pragma once

#include <string>
#include <vector>
#include <memory>
#include "LLMProvider.h"
#include "Config.h"

class EmbeddingClient
{
public:
    explicit EmbeddingClient(std::shared_ptr<LLMProvider> provider);

    /**
     * @brief Generates an embedding for the given text.
     * 
     * @param text The text to embed.
     * @param filename The name of the file being embedded, for logging purposes.
     * @return std::vector<float> The embedding vector, or an empty vector on failure.
     */
    std::vector<float> getEmbedding(const std::string& text, const std::string& filename);

private:
    std::shared_ptr<LLMProvider> llmProvider;
};
