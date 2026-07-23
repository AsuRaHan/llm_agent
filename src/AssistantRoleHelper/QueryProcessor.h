#pragma once

#include "AssistantResponse.h"
#include "ToolManager.h"
#include "LLMProvider.h" 
#include "Config.h"
#include "ContextIndexer.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>

class MessageBuilder;

class QueryProcessor {
public:
    QueryProcessor(
        std::shared_ptr<LLMProvider> llmProvider,
        ToolManager& toolManager,
        const Config& config,
        ContextIndexer& indexer,
        const std::function<void(const std::string&)>& send_thought,
        const std::function<void(const std::string&)>& send_stream_chunk,
        std::atomic<bool>& is_interrupted
    );

    AssistantResponse processAdvanced(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        const nlohmann::json& continuation_history
    );
private:
    AssistantResponse runReActIteration(MessageBuilder& messageBuilder, int iteration, bool is_first_iteration_after_confirmation);
    AssistantResponse handleToolCalls(const nlohmann::json& message, MessageBuilder& messageBuilder, bool skip_danger_check = false);
    AssistantResponse executeToolCall(const nlohmann::json& call, MessageBuilder& messageBuilder, bool skip_danger_check = false);
    AssistantResponse forceFinalAnswer(MessageBuilder& messageBuilder);
    AssistantResponse getSummaryAnswer(MessageBuilder& messageBuilder);

    std::shared_ptr<LLMProvider> m_llmProvider;
    ToolManager& m_toolManager;
    const Config& m_config;
    ContextIndexer& m_indexer;
    std::function<void(const std::string&)> m_send_thought;
    std::function<void(const std::string&)> m_send_stream_chunk;
    std::atomic<bool>& m_is_interrupted;
};