#pragma once

#include "ITool.h"

class ReadFileTool : public ITool {
public:
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;
};