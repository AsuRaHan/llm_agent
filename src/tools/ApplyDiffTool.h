#pragma once

#include "ITool.h"
#include <string>
#include <optional>

class ApplyDiffTool : public ITool {
public:
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;
    std::string dryRun(const nlohmann::json& args) const;
    std::string applyBackup(const std::string& backupPath);
    std::string getStatistics() const;
    std::string validateDiff(const std::string& diffContent);
};