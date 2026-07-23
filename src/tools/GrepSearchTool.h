#pragma once

#include "ITool.h"

class GrepSearchTool : public ITool {
public:
    explicit GrepSearchTool(const std::string& projectDir);
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;
private:
    std::string m_projectDir;
};