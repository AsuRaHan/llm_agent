#include "WriteFileTool.h"
#include "../Logger.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

std::string WriteFileTool::getName() const {
    return "write_file";
}

std::string WriteFileTool::getDescription() const {
    return "Записывает или дописывает контент в файл. ВНИМАНИЕ: этот инструмент перезаписывает существующие файлы или создает новые. Используйте с осторожностью.";
}

nlohmann::json WriteFileTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Путь к файлу, в который нужно записать данные."}
            }},
            {"content", {
                {"type", "string"},
                {"description", "Содержимое, которое нужно записать в файл."}
            }},
            {"mode", {
                {"type", "string"},
                {"description", "Режим записи: 'write' для перезаписи файла (по умолчанию) или 'append' для добавления в конец."},
                {"enum", {"write", "append"}},
                {"default", "write"}
            }}
        }},
        {"required", {"path", "content"}}
    };
}

std::string WriteFileTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("path") || !args.contains("content")) {
        return "{\"error\": \"Отсутствуют обязательные аргументы 'path' или 'content'.\"}";
    }

    std::string path_str = args["path"];
    std::string content = args["content"];
    std::string mode_str = args.value("mode", "write");

    fs::path file_path(path_str);

    // Простая проверка безопасности: не позволяем выходить за пределы текущей директории
    fs::path canonical_path;
    try {
        canonical_path = fs::weakly_canonical(file_path);
        if (canonical_path.string().find(fs::current_path().string()) != 0) {
            SPDLOG_WARN("Попытка записи файла вне текущей директории проекта: {}", path_str);
            return "{\"error\": \"Запись файлов разрешена только в пределах текущей директории проекта.\"}";
        }
    } catch (const fs::filesystem_error& e) {
        std::string error_msg = "Ошибка при обработке пути к файлу: " + std::string(e.what());
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    // Убедимся, что родительская директория существует
    if (file_path.has_parent_path() && !fs::exists(file_path.parent_path())) {
        fs::create_directories(file_path.parent_path());
    }

    std::ios_base::openmode open_mode = std::ios::out;
    if (mode_str == "append") {
        open_mode |= std::ios::app;
    } // 'write' is the default (trunc)

    std::ofstream file(file_path, open_mode);
    if (!file.is_open()) {
        std::string error_msg = "Не удалось открыть файл для записи: " + path_str;
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    file << content;
    file.close();

    SPDLOG_INFO("Файл '{}' успешно {} (режим: {}).", path_str, (mode_str == "append" ? "дополнен" : "записан"), mode_str);
    return "Файл '" + path_str + "' успешно " + (mode_str == "append" ? "дополнен" : "записан") + ".";
}