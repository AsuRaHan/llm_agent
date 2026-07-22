#include "ToolManager.h"
#include "ReadFileTool.h" // Include the first tool
#include "../ContextIndexer.h" // Для доступа к мьютексу
#include <filesystem>
#include <mutex> // Для std::lock_guard
#include "ListDirectoryTool.h" // Include the new tool
#include "CodeSearchTool.h" // Include the code search tool
#include "Logger.h"
#include "GrepSearchTool.h" // Include the grep tool
#include "FileGlobSearchTool.h" // Include the new glob tool
#include "WriteFileTool.h"      // Include the new write tool
#include "EditFileTool.h"       // Include the new edit tool
// #include "ApplyDiffTool.h"      // Include the new diff tool
#include "ExecuteShellCommandTool.h" // Include the shell tool
#include "GetDateTimeTool.h"    // Include the datetime tool
#include "GetSystemInfoTool.h"  // Include the system info tool
#include "WebSearchTool.h"      // Include the new web search tool
#include "ReadUrlTool.h"        // Include the new URL reader tool
#include "OpenUrlTool.h"        // Include the new URL opener tool
#include "GitHubSearchTool.h"   // Include the new GitHub search tool
#include "../Config.h" // Needed for the constructor

namespace fs = std::filesystem;

ToolManager::ToolManager(const Config& config, const std::string& projectDir) 
    : m_projectDir(projectDir)
{
    SPDLOG_INFO("Включение инструментов...");
    // Register safe, read-only tools
    registerTool(std::make_unique<ReadFileTool>());
    registerTool(std::make_unique<ListDirectoryTool>());
    registerTool(std::make_unique<CodeSearchTool>());
    registerTool(std::make_unique<GrepSearchTool>());
    registerTool(std::make_unique<FileGlobSearchTool>(config));
    registerTool(std::make_unique<GetDateTimeTool>());
    registerTool(std::make_unique<GetSystemInfoTool>());
    registerTool(std::make_unique<ReadUrlTool>());
    registerTool(std::make_unique<OpenUrlTool>());
    registerTool(std::make_unique<GitHubSearchTool>());

    // Register web search tool only if an API key is provided
    if (!config.web_search_api_key.empty()) {
        registerTool(std::make_unique<WebSearchTool>(config));
    }

    // Register dangerous, write-access tools only if explicitly enabled
    if (config.enable_dangerous_tools) {
        SPDLOG_WARN("Включены ОПАСНЫЕ инструменты (например, запись файлов).");
        registerTool(std::make_unique<WriteFileTool>());
        registerTool(std::make_unique<EditFileTool>());
        // registerTool(std::make_unique<ApplyDiffTool>());
        registerTool(std::make_unique<ExecuteShellCommandTool>());
    }
}

void ToolManager::registerTool(std::unique_ptr<ITool> tool) {
    if (!tool) return;
    std::string name = tool->getName();
    SPDLOG_INFO("Зарегистрирован инструмент для модели: {}", name);
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
    nlohmann::json processed_args = args;

    if (it != tools.end()) {
        // Проверяем, нужно ли преобразовывать путь
        if (name == "read_file" || name == "write_file" || name == "edit_file" || name == "apply_diff" || name == "list_directory" || name == "grep_search") {
            if (processed_args.contains("path") && processed_args["path"].is_string()) {
                fs::path tool_path = processed_args["path"].get<std::string>();
                if (tool_path.is_relative() && !m_projectDir.empty()) {
                    fs::path absolute_path = fs::path(m_projectDir) / tool_path;
                    processed_args["path"] = fs::weakly_canonical(absolute_path).string();
                    SPDLOG_DEBUG("Разрешение относительного пути '{}' в '{}'", tool_path.string(), processed_args["path"].get<std::string>());
                }
            }
        }

        SPDLOG_INFO("Выполнение инструмента '{}' с аргументами: {}", name, processed_args.dump());
        try {
            if (!indexer) {
                SPDLOG_ERROR("ToolManager::executeTool вызван с null indexer. Невозможно обеспечить потокобезопасность.");
                return "{\"error\": \"Внутренняя ошибка: Indexer недоступен для выполнения инструмента.\"}";
            }

            // Централизованная блокировка для предотвращения гонки с FileWatcher при доступе к файлам и индексу.
            std::lock_guard<std::recursive_mutex> lock(indexer->getFileIndexer().mtx);
            auto result = it->second->execute(processed_args, indexer);
            return result;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Исключение при выполнении инструмента '{}': {}", name, e.what());
            return "{\"error\": \"Исключение при выполнении инструмента: " + std::string(e.what()) + "\"}";
        }
    }
    SPDLOG_ERROR("Попытка выполнить неизвестный инструмент: {}", name);
    return "{\"error\": \"Инструмент '" + name + "' не найден.\"}";
}