#pragma once

#include "ITool.h"
#include "../Config.h" // For Config reference

class FileGlobSearchTool : public ITool {
public:
    FileGlobSearchTool(const Config& config, const std::string& projectDir);
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;
private:
    std::string globToRegex(const std::string& glob) const;
    const Config& config;
    std::string m_projectDir;
};