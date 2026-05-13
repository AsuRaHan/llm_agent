#pragma once

#include "ITool.h"
#include "../Config.h" // Нужен для доступа к списку игнорируемых директорий
#include <string>

class FileGlobSearchTool : public ITool {
public:
    // Принимаем конфиг, чтобы знать, какие папки игнорировать при поиске
    explicit FileGlobSearchTool(const Config& config);

    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getParameters() const override;
    std::string execute(const nlohmann::json& args, ContextIndexer* indexer) override;

private:
    const Config& config;
    // Вспомогательная функция для конвертации glob-паттерна в регулярное выражение
    std::string globToRegex(const std::string& glob) const;
};