#pragma once

#include "ITool.h"

class ListDirectoryTool : public ITool {
public:
    explicit ListDirectoryTool(const std::string& projectDir);
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;
private:
    std::string m_projectDir;
};