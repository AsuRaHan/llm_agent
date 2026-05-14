#pragma once

#include "Config.h"
#include "ContextIndexer.h" // For SearchResult
#include "tools/ToolManager.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

struct AssistantResponse {
    std::string text;
    bool is_final = true;
    nlohmann::json conversation_history;

    // Fields for pending action confirmation
    bool requires_confirmation = false;
    nlohmann::json pending_tool_call = nullptr;
    std::string confirmation_prompt;
};

class AssistantRole {
public:
    explicit AssistantRole(const Config& config);
    ~AssistantRole();

    AssistantResponse processQuery(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        ContextIndexer& indexer,
        const nlohmann::json& continuation_history = nlohmann::json::array(),
        const std::function<void(const std::string&)>& send_thought = nullptr
    );

    std::string generateProjectSummaryGreeting(int file_count, int embedding_count);
    std::string generateChunkSummary(const std::string& codeChunk, const std::string& chunkName);

private:
    const Config& config;
    httplib::Client cli;
public: // TODO: Refactor this. Server should not directly access assistant's members.
    std::unique_ptr<ToolManager> toolManager;
};