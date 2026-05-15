#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

#include "httplib.h"
#include "ContextIndexer.h"
#include "ToolManager.h"

// Forward-declaration
class Config;

struct AssistantResponse {
    std::string text;
    bool is_final;
    nlohmann::json conversation_history;
    bool requires_confirmation = false;
    nlohmann::json pending_tool_call = nullptr;
    bool plan_completed = true;
    bool step_failed = false; // true, если в процессе выполнения шага произошла ошибка

    // Поля для обработки ошибок
    std::string error_message = ""; // Детальное сообщение об ошибке
    std::vector<std::string> recovery_options; // Предлагаемые варианты восстановления (retry, skip, re-plan)
};


class AssistantRole {
public:
    explicit AssistantRole(const Config& config);
    ~AssistantRole();

    AssistantResponse processQuery(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        ContextIndexer& indexer,
        const nlohmann::json& continuation_history,
        const std::function<void(const std::string&)>& send_thought
    );

    nlohmann::json generatePlan(const std::string& user_query);

    std::string generateProjectSummaryGreeting(int file_count, int embedding_count);
    std::string generateChunkSummary(const std::string& codeChunk, const std::string& chunkName);

private:
    nlohmann::json parsePlanFromMarkdown(const std::string& text);

    const Config& config;
    httplib::Client cli;
    std::unique_ptr<ToolManager> toolManager;
};