#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

/**
 * FileReader: Ответственен за чтение содержимого файлов и конкретных чанков
 */
class FileReader
{
public:
    /**
     * Читает полное содержимое файла в виде строки
     * @param path Путь к файлу
     * @return Содержимое файла или пустая строка если ошибка
     */
    static std::string readFile(const fs::path& path);

    /**
     * Читает конкретный чанк из файла по смещению и длине
     * @param path Путь к файлу
     * @param start_byte Смещение в байтах
     * @param length Длина чанка в байтах
     * @return Содержимое чанка или пустая строка если ошибка
     */
    static std::string readChunk(const std::string& path, size_t start_byte, size_t length);
};
