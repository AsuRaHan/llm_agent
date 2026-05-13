#include "ReadFileTool.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

std::string ReadFileTool::getName() const {
    return "read_file";
}

std::string ReadFileTool::getDescription() const {
    return "Reads the entire content of a file at the given path. Returns the file content as a string.";
}

nlohmann::json ReadFileTool::getParameters() const {
    return nlohmann::json::parse(R"({
        "type": "object",
        "properties": {
            "path": {
                "type": "string",
                "description": "The relative or absolute path to the file to read."
            }
        },
        "required": ["path"]
    })");
}

std::string ReadFileTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    (void)indexer; // This tool doesn't use the indexer, so mark it as unused.
    if (!args.contains("path")) {
        return "Error: 'path' argument is missing.";
    }
    std::string path = args["path"].get<std::string>();
    SPDLOG_INFO("[Tool:read_file] Reading file: {}", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        SPDLOG_ERROR("[Tool:read_file] Failed to open file: {}", path);
        return "Error: Could not open file '" + path + "'.";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}