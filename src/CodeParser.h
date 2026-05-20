#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "Config.h"

/**
 * Структура для хранения информации о семантическом чанке кода
 */
struct CodeChunk {
    std::string text;           // Текст чанка
    size_t start_byte;          // Начальная позиция в файле
    size_t length;              // Длина чанка
    std::string language;       // Язык программирования
};

/**
 * CodeParser: Парсит код с использованием tree-sitter для семантического разбиения
 */
class CodeParser {
public:
    explicit CodeParser(const Config& config);
    ~CodeParser();

    /**
     * Парсит код и разбивает на семантические чанки
     * @param content - содержимое файла
     * @param extension - расширение файла для определения языка
     * @return вектор семантических чанков
     */
    std::vector<CodeChunk> parse(const std::string& content, const std::string& extension);

    /**
     * Определяет язык программирования по расширению файла
     */
    static std::string detectLanguage(const std::string& extension);

private:
    const Config& config;

    // Tree-sitter парсеры для разных языков
    // TODO: Добавить инициализацию парсеров
};
