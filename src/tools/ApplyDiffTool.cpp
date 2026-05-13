#include "ApplyDiffTool.h"
#include "../Logger.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

// Helper to handle CRLF and LF line endings while reading
static std::vector<std::string> read_lines_from_stream(std::istream& stream) {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::string ApplyDiffTool::getName() const {
    return "apply_diff";
}

std::string ApplyDiffTool::getDescription() const {
    return "Применяет патч в формате unified diff к указанному файлу. Полезно для внесения сложных изменений в файл за один шаг. ВНИМАНИЕ: этот инструмент изменяет файлы.";
}

nlohmann::json ApplyDiffTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Путь к файлу, к которому нужно применить патч."}
            }},
            {"diff_content", {
                {"type", "string"},
                {"description", "Содержимое патча в формате unified diff. Должен включать заголовки '---' и '+++', а также хотя бы один блок '@@ ... @@'."}
            }}
        }},
        {"required", {"path", "diff_content"}}
    };
}

std::string ApplyDiffTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("path") || !args.contains("diff_content")) {
        return "{\"error\": \"Отсутствуют обязательные аргументы 'path' или 'diff_content'.\"}";
    }

    std::string path_str = args["path"];
    std::string diff_content = args["diff_content"];

    fs::path file_path(path_str);

    // Security check
    try {
        fs::path canonical_path = fs::weakly_canonical(file_path);
        if (canonical_path.string().find(fs::current_path().string()) != 0) {
            SPDLOG_WARN("Попытка применения патча к файлу вне текущей директории проекта: {}", path_str);
            return "{\"error\": \"Применение патчей разрешено только для файлов в пределах текущей директории проекта.\"}";
        }
    } catch (const fs::filesystem_error& e) {
        std::string error_msg = "Ошибка при обработке пути к файлу: " + std::string(e.what());
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    if (!fs::exists(file_path)) {
        return "{\"error\": \"Файл для применения патча не найден: " + path_str + "\"}";
    }

    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        return "{\"error\": \"Не удалось открыть файл для чтения: " + path_str + "\"}";
    }
    auto original_lines = read_lines_from_stream(infile);
    infile.close();

    std::vector<std::string> new_file_lines;
    std::stringstream diff_stream(diff_content);
    auto diff_lines = read_lines_from_stream(diff_stream);

    size_t original_line_idx = 0;
    size_t diff_line_idx = 0;

    while (diff_line_idx < diff_lines.size() && (diff_lines[diff_line_idx].rfind("---", 0) == 0 || diff_lines[diff_line_idx].rfind("+++", 0) == 0)) {
        diff_line_idx++;
    }

    std::regex hunk_regex(R"(@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@.*)");
    std::smatch match;

    while (diff_line_idx < diff_lines.size()) {
        if (std::regex_match(diff_lines[diff_line_idx], match, hunk_regex)) {
            int old_start = std::stoi(match[1].str());

            while (original_line_idx < (size_t)old_start - 1) {
                if (original_line_idx >= original_lines.size()) {
                     return "{\"error\": \"Патч не применяется. Неожиданный конец исходного файла перед ханком.\"}";
                }
                new_file_lines.push_back(original_lines[original_line_idx]);
                original_line_idx++;
            }
            
            diff_line_idx++;

            while (diff_line_idx < diff_lines.size() && diff_lines[diff_line_idx].rfind("@@", 0) != 0) {
                const std::string& hunk_line = diff_lines[diff_line_idx];
                if (hunk_line.empty()) { diff_line_idx++; continue; }
                char type = hunk_line[0];
                std::string content = hunk_line.substr(1);

                if (original_line_idx >= original_lines.size() && (type == ' ' || type == '-')) {
                    return "{\"error\": \"Патч не применяется. Неожиданный конец исходного файла внутри ханка.\"}";
                }

                if (type == ' ') {
                    if (original_lines[original_line_idx] != content) return "{\"error\": \"Патч не применяется. Контекстная строка не совпадает в строке " + std::to_string(original_line_idx + 1) + ".\"}";
                    new_file_lines.push_back(original_lines[original_line_idx]);
                    original_line_idx++;
                } else if (type == '-') {
                    if (original_lines[original_line_idx] != content) return "{\"error\": \"Патч не применяется. Удаляемая строка не совпадает в строке " + std::to_string(original_line_idx + 1) + ".\"}";
                    original_line_idx++;
                } else if (type == '+') {
                    new_file_lines.push_back(content);
                } else {
                    std::string error_msg = "Неверный формат патча. Неизвестный символ '" + std::string(1, type) + "' в начале строки: " + hunk_line;
                    SPDLOG_ERROR(error_msg);
                    return "{\"error\": \"" + error_msg + "\"}";
                }
                diff_line_idx++;
            }
        } else {
            diff_line_idx++;
        }
    }

    while (original_line_idx < original_lines.size()) {
        new_file_lines.push_back(original_lines[original_line_idx]);
        original_line_idx++;
    }

    std::ofstream outfile(file_path);
    if (!outfile.is_open()) return "{\"error\": \"Не удалось открыть файл для записи: " + path_str + "\"}";
    for (size_t i = 0; i < new_file_lines.size(); ++i) {
        outfile << new_file_lines[i] << (i == new_file_lines.size() - 1 ? "" : "\n");
    }
    outfile.close();

    SPDLOG_INFO("Патч успешно применен к файлу '{}'.", path_str);
    return "Патч успешно применен к файлу '" + path_str + "'.";
}