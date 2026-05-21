#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include "AssistantResponse.h" // Для структуры AssistantResponse



class LLMProvider {
public:
    virtual ~LLMProvider() = default;

    struct ServerProperties {
        std::string name;
        std::string version;
        // Add other relevant properties
    };

    virtual AssistantResponse processChat(
        const nlohmann::json& messages,
        const nlohmann::json& tools,
        const std::function<void(const std::string&)>& send_thought
    ) = 0;

    virtual nlohmann::json generatePlan(const std::string& user_query) = 0;

    virtual std::vector<float> createEmbedding(const std::string& text) = 0;

    virtual std::string generateChunkSummary(const std::string& code_chunk, const std::string& chunk_name) = 0;

    virtual std::optional<ServerProperties> fetchServerProperties() const = 0;
};