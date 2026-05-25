#pragma once

#include "AssistantRole.h" // For AssistantResponse
#include "ToolManager.h"
#include "LLMProvider.h" 
#include "Config.h"
#include "ContextIndexer.h"
#include "Searcher.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>

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

    AssistantResponse process(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        const nlohmann::json& continuation_history
    );
    // === НОВЫЙ ПРОДВИНУТЫЙ МЕТОД ===
    AssistantResponse processAdvanced(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        const nlohmann::json& continuation_history
    );
private:
    // Main loop and handlers for legacy `process` method
    AssistantResponse runReActIteration(class MessageBuilder& messageBuilder, int iteration);
    AssistantResponse handleContinuationAfterConfirmation(class MessageBuilder& messageBuilder);
    // Common handlers
    AssistantResponse handleToolCalls(const nlohmann::json& message, class MessageBuilder& messageBuilder, bool skip_danger_check = false);
    AssistantResponse executeToolCall(const nlohmann::json& call, class MessageBuilder& messageBuilder, bool skip_danger_check = false);
    AssistantResponse forceFinalAnswer(class MessageBuilder& messageBuilder);
    AssistantResponse getSummaryAnswer(MessageBuilder& messageBuilder);
    // Member variables
    std::shared_ptr<LLMProvider> m_llmProvider;
    ToolManager& m_toolManager;
    const Config& m_config;
    ContextIndexer& m_indexer;
    std::function<void(const std::string&)> m_send_thought;
    std::function<void(const std::string&)> m_send_stream_chunk;
    std::atomic<bool>& m_is_interrupted;
};