#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

#include "ContextIndexer.h"
#include "ToolManager.h"
#include "LLMProvider.h"
#include "ContextIndexerHelper/Searcher.h"

#include "AssistantResponse.h"
#include "Config.h"

class AssistantRole {
public:
    explicit AssistantRole(std::shared_ptr<LLMProvider> llmProvider, 
                           const Config& config,
                           const std::string& projectDir);
    ~AssistantRole();

    AssistantResponse processQuery(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        ContextIndexer& indexer,
        const nlohmann::json& continuation_history,
        const std::function<void(const std::string&)>& send_thought,
        const std::function<void(const std::string&)>& send_stream_chunk,
        std::atomic<bool>& is_interrupted
    );

    nlohmann::json generatePlan(const std::string& user_query);

private:
    const Config& config;
    std::shared_ptr<LLMProvider> llmProvider;
    std::unique_ptr<ToolManager> toolManager;
};