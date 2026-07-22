#pragma once

#include <filesystem>
#include "ITool.h"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>
#include "../Config.h"

class ContextIndexer; // Forward declaration

class ToolManager {
public:
    explicit ToolManager(const Config& config, const std::string& projectDir);

    void registerTool(std::unique_ptr<ITool> tool);
    nlohmann::json getToolsSpecification() const;
    std::string executeTool(const std::string& name, const nlohmann::json& args, ContextIndexer* indexer);

private:
    std::map<std::string, std::unique_ptr<ITool>> tools;
    std::string m_projectDir;
};