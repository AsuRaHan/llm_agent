#include "EditFileTool.h"
#include "../Logger.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

std::string EditFileTool::getName() const {
    return "edit_file";
}

std::string EditFileTool::getDescription() const {
    return "Безопасно заменяет блок кода в файле. Находит код, начиная с указанной строки, проверяет, совпадает ли он с 'old_code_block', и если да, заменяет его на 'new_code_block'. Это предпочтительнее, чем 'write_file' для точечных изменений.";
}

nlohmann::json EditFileTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Путь к файлу для редактирования."}
            }},
            {"start_line", {
                {"type", "integer"},
                {"description", "Номер строки (начиная с 1), с которой начинается заменяемый блок."}
            }},
            {"old_code_block", {
                {"type", "string"},
                {"description", "Точный текстовый блок, который должен быть заменен. Используется для проверки безопасности, чтобы избежать случайных изменений."}
            }},
            {"new_code_block", {
                {"type", "string"},
                {"description", "Новый блок кода, который будет вставлен вместо старого."}
            }}
        }},
        {"required", {"path", "start_line", "old_code_block", "new_code_block"}}
    };
}

std::string EditFileTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("path") || !args.contains("start_line") || !args.contains("old_code_block") || !args.contains("new_code_block")) {
        return "{\"error\": \"Отсутствуют обязательные аргументы 'path', 'start_line', 'old_code_block' или 'new_code_block'.\"}";
    }

    std::string path_str = args["path"];
    if (!args["start_line"].is_number_integer()) {
        return "{\"error\": \"Аргумент 'start_line' должен быть целым числом.\"}";
    }
    int start_line = args["start_line"]; // 1-based
    std::string old_code_block = args["old_code_block"];
    std::string new_code_block = args["new_code_block"];

    fs::path file_path(path_str);

    // Security check
    try {
        fs::path canonical_path = fs::weakly_canonical(file_path);
        if (canonical_path.string().find(fs::current_path().string()) != 0) {
            SPDLOG_WARN("Попытка редактирования файла вне текущей директории проекта: {}", path_str);
            return "{\"error\": \"Редактирование файлов разрешено только в пределах текущей директории проекта.\"}";
        }
    } catch (const fs::filesystem_error& e) {
        std::string error_msg = "Ошибка при обработке пути к файлу: " + std::string(e.what());
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    if (!fs::exists(file_path)) {
        return "{\"error\": \"Файл для редактирования не найден: " + path_str + "\"}";
    }

    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        return "{\"error\": \"Не удалось открыть файл для чтения: " + path_str + "\"}";
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    infile.close();

    if (start_line <= 0 || (size_t)start_line > lines.size()) {
        return "{\"error\": \"Неверный номер начальной строки. В файле " + std::to_string(lines.size()) + " строк, запрошена " + std::to_string(start_line) + ".\"}";
    }

    // Convert old_code_block to a vector of lines
    std::vector<std::string> old_lines;
    std::stringstream old_ss(old_code_block);
    while(std::getline(old_ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        old_lines.push_back(line);
    }

    // Check if the old block matches
    if ((size_t)start_line - 1 + old_lines.size() > lines.size()) {
        return "{\"error\": \"Безопасная замена не удалась. 'old_code_block' выходит за пределы файла.\"}";
    }
    for (size_t i = 0; i < old_lines.size(); ++i) {
        if (lines[start_line - 1 + i] != old_lines[i]) {
            std::string error_msg = "Безопасная замена не удалась. 'old_code_block' не совпадает с содержимым файла в строке " + std::to_string(start_line + i) + ".";
            SPDLOG_WARN(error_msg);
            return "{\"error\": \"" + error_msg + "\"}";
        }
    }

    // Convert new_code_block to a vector of lines
    std::vector<std::string> new_lines;
    std::stringstream new_ss(new_code_block);
    while(std::getline(new_ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        new_lines.push_back(line);
    }

    // Perform the replacement
    lines.erase(lines.begin() + start_line - 1, lines.begin() + start_line - 1 + old_lines.size());
    lines.insert(lines.begin() + start_line - 1, new_lines.begin(), new_lines.end());

    // Write back to the file
    std::ofstream outfile(file_path);
    if (!outfile.is_open()) {
        return "{\"error\": \"Не удалось открыть файл для записи: " + path_str + "\"}";
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        outfile << lines[i] << (i == lines.size() - 1 ? "" : "\n");
    }
    outfile.close();

    SPDLOG_INFO("Файл '{}' успешно изменен, начиная со строки {}.", path_str, start_line);
    return "Файл '" + path_str + "' успешно изменен.";
}