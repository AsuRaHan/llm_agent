#include "FileReader.h"
#include <fstream>
#include <sstream>
#include "Logger.h"

std::string FileReader::readFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        SPDLOG_ERROR("Не удалось открыть файл: {}", path.string());
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string FileReader::readChunk(const std::string& path, size_t start_byte, size_t length)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        SPDLOG_ERROR("Не удалось открыть файл для чтения чанка: {}", path);
        return "";
    }
    file.seekg(start_byte);
    std::string content(length, '\0');
    file.read(&content[0], length);
    return content;
}
