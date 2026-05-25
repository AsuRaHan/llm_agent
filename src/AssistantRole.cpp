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
    const std::function<void(const std::string&)>& send_stream_chunk,
    std::atomic<bool>& is_interrupted
) {
    QueryProcessor processor(llmProvider, *toolManager, config, indexer, send_thought, send_stream_chunk, is_interrupted);
    // return processor.process(userQuery, initialContext, continuation_history);
    // Используем новый продвинутый метод
    return processor.processAdvanced(userQuery, initialContext, continuation_history);
}



nlohmann::json AssistantRole::generatePlan(const std::string& user_query) {
    SPDLOG_INFO("Генерация плана для запроса: '{}'", user_query);

    json response_json = llmProvider->generatePlan(user_query);

    if (response_json.contains("error")) {
        SPDLOG_ERROR("Не удалось сгенерировать план: {}", response_json["error"].get<std::string>());
        return json::array({"Ошибка: " + response_json["error"].get<std::string>()});
    }
    
    // Поскольку мы используем response_format: json_object, ответ модели должен быть строкой, содержащей валидный JSON.
    std::string content_str = response_json["choices"][0]["message"]["content"];

    try {
        json plan_json = json::parse(content_str);
        if (plan_json.is_object() && plan_json.contains("plan") && plan_json["plan"].is_array()) {
            SPDLOG_INFO("План успешно сгенерирован и разобран.");
            return plan_json["plan"];
        }
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Не удалось разобрать JSON из ответа LLM при генерации плана: {}. Ответ: {}", e.what(), content_str);
        return json::array({"Ошибка: модель вернула некорректный JSON в плане."});
    }

    SPDLOG_ERROR("Не удалось извлечь корректный план из ответа LLM. Ответ: {}", content_str);
    return json::array({"Ошибка: модель вернула некорректный формат плана."});
}
