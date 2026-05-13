#pragma once

#include "ITool.h"
#include <string>

class GetSystemInfoTool : public ITool {
public:
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;
};