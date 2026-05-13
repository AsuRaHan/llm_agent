#pragma once

#include <string>
#include "nlohmann/json.hpp"

// Прямое объявление (forward declaration) для избежания циклических зависимостей.
// Инструментам может понадобиться доступ к Indexer, но Indexer не должен зависеть от конкретных инструментов.
class ContextIndexer;

/**
 * @brief Абстрактный базовый класс (интерфейс) для всех инструментов,
 * которые может использовать агент.
 */
class ITool {
public:
    virtual ~ITool() = default;

    // Имя инструмента, которое будет использоваться в вызовах LLM (e.g., "read_file").
    virtual std::string getName() const = 0;

    // Описание для LLM, что делает инструмент и когда его использовать.
    virtual std::string getDescription() const = 0;

    // JSON-схема параметров инструмента в формате OpenAI.
    virtual nlohmann::json getParameters() const = 0;

    // Основная функция, выполняющая логику инструмента.
    virtual std::string execute(const nlohmann::json& args, ContextIndexer* indexer) = 0;

    // Вспомогательная функция для генерации полной спецификации для API LLM.
    nlohmann::json getSpecification() const;
};