#pragma once

#include "Tool.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace llm_agent {
namespace tools {

/**
 * @brief Инструмент для применения патчей в формате unified diff
 * 
 * Безопасно применяет изменения к файлу с полной валидацией,
 * созданием бэкапа и атомарной заменой.
 */
class ApplyDiffTool : public Tool {
public:
    ApplyDiffTool();
    ~ApplyDiffTool() override = default;

    // Tool interface
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;

private:
    // Валидация и безопасность
    std::string validateDiff(const std::string& diffContent) const;
    std::string validatePath(const std::string& path) const;
    std::string validateFileAccess(const std::string& path) const;
    
    // Работа с файлами
    std::string applyPatch(const std::string& filePath, const std::string& diffContent) const;
    std::string createBackup(const std::string& filePath) const;
    std::string restoreFromBackup(const std::string& filePath, const std::string& backupPath) const;
    std::string applyAtomicReplace(const std::string& tempPath, const std::string& targetPath) const;
    
    // Предпросмотр
    std::string dryRun(const nlohmann::json& args) const;
    
    // Чтение и обработка
    std::vector<std::string> readLinesFromFile(const std::string& filePath) const;
    std::vector<std::string> parseDiff(const std::string& diffContent) const;
    std::string applyHunk(const std::vector<std::string>& originalLines, 
                          const std::vector<std::string>& diffLines,
                          size_t& originalLineIdx,
                          size_t hunkStartLine) const;
    
    // Статистика
    struct DiffStatistics {
        size_t linesAdded = 0;
        size_t linesRemoved = 0;
        size_t linesUnchanged = 0;
        size_t hunksApplied = 0;
        size_t hunksSkipped = 0;
        size_t errors = 0;
        std::string timestamp;
        std::string filePath;
    };
    
    // Вспомогательные функции
    static std::string trim(const std::string& s);
    static std::string normalizePath(const std::string& path);
    static bool isPathSafe(const std::string& path);
    static std::string escapeStringForJson(const std::string& s);
    
    // Логирование
    void logSuccess(const std::string& message) const;
    void logError(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

} // namespace tools
} // namespace llm_agent
