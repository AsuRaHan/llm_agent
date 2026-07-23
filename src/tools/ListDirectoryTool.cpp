#include "ListDirectoryTool.h"
#include "Logger.h"
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

ListDirectoryTool::ListDirectoryTool(const std::string& projectDir) : m_projectDir(projectDir) {}

std::string ListDirectoryTool::getName() const {
    return "list_directory";
}

std::string ListDirectoryTool::getDescription() const {
    return "Lists the contents (files and subdirectories) of a specified directory. If no path is provided, it lists the current working directory.";
}

nlohmann::json ListDirectoryTool::getParameters() const {
    return nlohmann::json::parse(R"({
        "type": "object",
        "properties": {
            "path": {
                "type": "string",
                "description": "The relative or absolute path to the directory to list. Defaults to the current directory if not provided."
            }
        },
        "required": []
    })");
}

std::string ListDirectoryTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    (void)indexer; // This tool doesn't use the indexer, so mark it as unused.
    std::string path_str = m_projectDir; // Default to project directory
    if (args.contains("path") && args["path"].is_string()) {
        path_str = args["path"].get<std::string>();
    }

    SPDLOG_INFO("[Tool:list_directory] Listing directory: {}", path_str);

    try {
        std::stringstream result;
        result << "Contents of directory '" << path_str << "':\n";
        for (const auto& entry : fs::directory_iterator(path_str)) {
            result << (entry.is_directory() ? "[DIR] " : "[FILE] ")
                   << fs::relative(entry.path()).string() << "\n";
        }
        return result.str();
    } catch (const fs::filesystem_error& e) {
        SPDLOG_ERROR("[Tool:list_directory] Filesystem error for path '{}': {}", path_str, e.what());
        return "Error: Could not access directory '" + path_str + "'. Reason: " + e.what();
    }
}