#include "ToolManager.h"
#include "ReadFileTool.h" // Include the first tool
#include "ListDirectoryTool.h" // Include the new tool
#include "CodeSearchTool.h" // Include the code search tool
#include "Logger.h"
#include "GrepSearchTool.h" // Include the grep tool
#include "FileGlobSearchTool.h" // Include the new glob tool
#include "WriteFileTool.h"      // Include the new write tool
#include "../Config.h" // Needed for the constructor

ToolManager::ToolManager(const Config& config) {
    SPDLOG_INFO("Initializing ToolManager...");
    // Register safe, read-only tools
    registerTool(std::make_unique<ReadFileTool>());
    registerTool(std::make_unique<ListDirectoryTool>());
    registerTool(std::make_unique<CodeSearchTool>());
    registerTool(std::make_unique<GrepSearchTool>());
    registerTool(std::make_unique<FileGlobSearchTool>(config));

    // Register dangerous, write-access tools only if explicitly enabled
    if (config.enable_dangerous_tools) {
        SPDLOG_WARN("Включены ОПАСНЫЕ инструменты (например, запись файлов).");
        registerTool(std::make_unique<WriteFileTool>());
    }
}

void ToolManager::registerTool(std::unique_ptr<ITool> tool) {
    if (!tool) return;
    std::string name = tool->getName();
    SPDLOG_INFO("Registering tool: {}", name);
    tools[name] = std::move(tool);
}

nlohmann::json ToolManager::getToolsSpecification() const {
    nlohmann::json spec_array = nlohmann::json::array();
    for (const auto& [name, tool] : tools) {
        nlohmann::json tool_spec = {
            {"type", "function"},
            {"function", {
                {"name", tool->getName()},
                {"description", tool->getDescription()},
                {"parameters", tool->getParameters()}
            }}
        };
        spec_array.push_back(tool_spec);
    }
    return spec_array;
}

std::string ToolManager::executeTool(const std::string& name, const nlohmann::json& args, ContextIndexer* indexer) {
    auto it = tools.find(name);
    if (it != tools.end()) {
        SPDLOG_INFO("Executing tool '{}' with args: {}", name, args.dump());
        try {
            return it->second->execute(args, indexer);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Exception while executing tool '{}': {}", name, e.what());
            return "Error: Exception occurred during tool execution: " + std::string(e.what());
        }
    }
    SPDLOG_ERROR("Attempted to execute unknown tool: {}", name);
    return "Error: Tool '" + name + "' not found.";
}