#include "AssistantRole.h"
#include "AssistantRoleHelper/QueryProcessor.h"
#include "Logger.h"
#include "Config.h"
#include "ToolManager.h"
#include <regex>

using json = nlohmann::json;

AssistantRole::AssistantRole(std::shared_ptr<LLMProvider> llmProvider, const Config& config) 
    : llmProvider(llmProvider),
      config(config),
      toolManager(std::make_unique<ToolManager>(config))
{
}

// Destructor must be defined in the .cpp file where ToolManager is a complete type
AssistantRole::~AssistantRole() = default;

AssistantResponse AssistantRole::processQuery(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    ContextIndexer& indexer,
    const nlohmann::json& continuation_history,
    const std::function<void(const std::string&)>& send_thought,
    const std::function<void(const std::string&)>& send_stream_chunk
) {
    QueryProcessor processor(llmProvider, *toolManager, config, indexer, send_thought, send_stream_chunk);
    return processor.process(userQuery, initialContext, continuation_history);
}



nlohmann::json AssistantRole::parsePlanFromMarkdown(const std::string& text) {
    // Regex to find a JSON object within markdown code fences (```json ... ``` or ``` ... ```)
    std::regex re("```(?:json)?\\s*(\\{[\\s\\S]*\\})\\s*```");
    std::smatch match;

    if (std::regex_search(text, match, re) && match.size() > 1) {
        try {
            // The captured JSON string is in match[1]
            return json::parse(match[1].str());
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать JSON из markdown-блока: {}", e.what());
        }
    }
    
    // Fallback for raw JSON that might have been returned with surrounding text, but not in a code block
    size_t first_brace = text.find('{');
    size_t last_brace = text.rfind('}');
    if (first_brace != std::string::npos && last_brace != std::string::npos && last_brace > first_brace) {
        try {
            return json::parse(text.substr(first_brace, last_brace - first_brace + 1));
        } catch (const json::exception& e) {
            // Ignore if this also fails, we'll return null
        }
    }

    return nullptr; // Return null json object on failure
}

nlohmann::json AssistantRole::generatePlan(const std::string& user_query) {
    SPDLOG_INFO("Генерация плана для запроса: '{}'", user_query);

    json response_json = llmProvider->generatePlan(user_query);

    if (response_json.contains("error")) {
        SPDLOG_ERROR("Не удалось сгенерировать план: {}", response_json["error"].get<std::string>());
        return json::array({"Ошибка: " + response_json["error"].get<std::string>()});
    }
    
    std::string content = response_json["choices"][0]["message"]["content"];

    json plan_json = parsePlanFromMarkdown(content);
    if (plan_json.is_object() && plan_json.contains("plan") && plan_json["plan"].is_array()) {
        SPDLOG_INFO("План успешно сгенерирован и разобран.");
        return plan_json["plan"];
    }

    SPDLOG_ERROR("Не удалось извлечь корректный план из ответа LLM. Ответ: {}", content);
    return json::array({"Ошибка: модель вернула некорректный формат плана."});
}
