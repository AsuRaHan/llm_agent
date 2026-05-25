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
    // return processor.process(userQuery, initialContext, continuation_history);
}
