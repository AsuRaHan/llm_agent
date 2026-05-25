#pragma once

#include "LLMProvider.h"
#include "Config.h"
#include <httplib.h>

class OpenAIProvider : public LLMProvider {
public:
    explicit OpenAIProvider(const Config& config);

    AssistantResponse processChat(
        const nlohmann::json& messages,
        const nlohmann::json& tools,
        const std::function<void(const std::string&)>& send_thought,
        const std::function<void(const std::string&)>& send_stream_chunk
    ) override;

    std::vector<float> createEmbedding(const std::string& text) override;

    std::string generateChunkSummary(const std::string& code_chunk, const std::string& chunk_name) override;

    std::optional<ServerProperties> fetchServerProperties() const override;

private:
    const Config& config;
    httplib::Client cli;
};