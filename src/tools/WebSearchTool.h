#pragma once

#include "ITool.h"
#include "../Config.h" // For accessing config values

class WebSearchTool : public ITool {
public:
    explicit WebSearchTool(const Config& config);

    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;

private:
    const Config& config;
};