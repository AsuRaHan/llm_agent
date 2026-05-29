#include "ApplyDiffTool.h"
#include "../Logger.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <regex>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

// Helper to trim whitespace from both ends of a string
static std::string trim(const std::string& s) {
    const char* whitespace = " \t\n\r\f\v";
    size_t first = s.find_first_not_of(whitespace);
    if (std::string::npos == first) {
        return ""; // All whitespace
    }
    size_t last = s.find_last_not_of(whitespace);
    return s.substr(first, (last - first + 1));
}

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

// Структура для статистики изменений
struct DiffStatistics {
    size_t linesAdded = 0;
    size_t linesRemoved = 0;
    size_t linesUnchanged = 0;
    std::string timestamp;
};

// Валидация формата unified diff
std::string ApplyDiffTool::validateDiff(const std::string& diffContent) {
    if (diffContent.empty()) return "Пустой diff";
    if (diffContent.find("---") == std::string::npos) return "Отсутствует заголовок исходного файла";
    if (diffContent.find("+++") == std::string::npos) return "Отсутствует заголовок целевого файла";
    if (diffContent.find("@@") == std::string::npos) return "Отсутствует ханк";
    return "";
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
                {"description", "Содержимое патча в формате unified diff."}
            }},
            {"dry_run", {
                {"type", "boolean"},
                {"description", "Режим предпросмотра без применения изменений."}
            }}
        }},
        {"required", {"path", "diff_content"}}
    };
}

std::string ApplyDiffTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    // bool dryRun = args.contains("dry_run") && args["dry_run"];
    if (args.contains("dry_run") && args["dry_run"]) {
        return dryRun(args);
    }

    if (!args.contains("path") || !args.contains("diff_content")) {
        return "{\"error\": \"Отсутствуют обязательные аргументы 'path' или 'diff_content'.\"}";
    }

    std::string path_str = args["path"];
    std::string diff_content = args["diff_content"];
    std::string backupPath = path_str + ".backup." + std::to_string(std::time(nullptr));

    fs::path file_path(path_str);

    // Security check
    try {
        fs::path canonical_path = fs::weakly_canonical(file_path);
        fs::path canonical_current = fs::weakly_canonical(fs::current_path());
        if (canonical_path.string().find(canonical_current.string()) != 0) {
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

    // Создание бэкапа
    try {
        fs::copy_file(file_path, backupPath, fs::copy_options::overwrite_existing);
        SPDLOG_INFO("Создан бэкап файла: {}", backupPath);
    } catch (const fs::filesystem_error& e) {
        SPDLOG_ERROR("Не удалось создать бэкап: {}", std::string(e.what()));
        return "{\"error\": \"Не удалось создать резервную копию файла: \" + std::string(e.what())}";
    }

    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        return "{\"error\": \"Не удалось открыть файл для чтения: " + path_str + "\"}";
    }
    auto original_lines = read_lines_from_stream(infile);
    infile.close();

    // Оптимизация памяти
    size_t estimatedSize = original_lines.size() * 2; // Увеличение в 2 раза для патчей
    std::vector<std::string> new_file_lines;
    new_file_lines.reserve(estimatedSize);

    // Валидация diff
    std::string validationError = ApplyDiffTool::validateDiff(diff_content);
    if (!validationError.empty()) {
        return "{\"error\": \"Некорректный формат diff: \" + validationError}";
    }

    std::vector<std::string> diff_lines;
    std::stringstream diff_stream(diff_content);
    auto diff_line_result = read_lines_from_stream(diff_stream);
    diff_lines = std::move(diff_line_result);

    size_t original_line_idx = 0;
    size_t diff_line_idx = 0;
    DiffStatistics stats;
    auto current_time_t = std::time(nullptr);

    // Пропуск заголовков diff
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
                    std::string error_msg = "Патч не применяется. Неожиданный конец исходного файла перед ханком.";
                    SPDLOG_ERROR(error_msg);
                    return "{\"error\": \"" + error_msg + "\"}";
                }
                new_file_lines.push_back(original_lines[original_line_idx]);
                original_line_idx++;
                stats.linesUnchanged++;
            }
            
            diff_line_idx++;

            while (diff_line_idx < diff_lines.size() && diff_lines[diff_line_idx].rfind("@@", 0) != 0) {
                const std::string& hunk_line = diff_lines[diff_line_idx];
                if (hunk_line.empty()) { diff_line_idx++; continue; }
                char type = hunk_line[0];
                std::string content = hunk_line.substr(1);

                if (original_line_idx >= original_lines.size() && (type == ' ' || type == '-')) {
                    std::string error_msg = "Патч не применяется. Неожиданный конец исходного файла внутри ханка.";
                    SPDLOG_ERROR(error_msg);
                    return "{\"error\": \"" + error_msg + "\"}";
                }

                if (type == ' ') {
                    if (trim(original_lines[original_line_idx]) != trim(content)) {
                        std::string error_msg = "Патч не применяется. Контекстная строка не совпадает в строке " + std::to_string(original_line_idx + 1) + ". Ожидалось: '" + original_lines[original_line_idx] + "', получено в патче: '" + content + "'.";
                        SPDLOG_WARN(error_msg);
                        return "{\"error\": \"" + error_msg + "\"}";
                    }
                    new_file_lines.push_back(original_lines[original_line_idx]);
                    original_line_idx++;
                    stats.linesUnchanged++;
                } else if (type == '-') {
                    if (trim(original_lines[original_line_idx]) != trim(content)) {
                        std::string error_msg = "Патч не применяется. Удаляемая строка не совпадает в строке " + std::to_string(original_line_idx + 1) + ". Ожидалось: '" + original_lines[original_line_idx] + "', получено в патче: '" + content + "'.";
                        SPDLOG_WARN(error_msg);
                        return "{\"error\": \"" + error_msg + "\"}";
                    }
                    original_line_idx++;
                    stats.linesRemoved++;
                } else if (type == '+') {
                    new_file_lines.push_back(content);
                    stats.linesAdded++;
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
        stats.linesUnchanged++;
    }

    // Форматирование timestamp
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &current_time_t);
#else
    localtime_r(&current_time_t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    stats.timestamp = oss.str();

    // Запись в временный файл
    fs::path tempPath = file_path.parent_path() / (file_path.stem().string() + ".tmp." + std::to_string(std::time(nullptr)) + "." + file_path.extension().string());
    std::ofstream outfile(tempPath);
    if (!outfile.is_open()) {
        return "{\"error\": \"Не удалось открыть временный файл для записи: \" + path_str}";
    }
    for (size_t i = 0; i < new_file_lines.size(); ++i) {
        outfile << new_file_lines[i] << (i == new_file_lines.size() - 1 ? "" : "\n");
    }
    outfile.close();

    // Атомарная замена файла
    try {
        fs::rename(tempPath, file_path);
        SPDLOG_INFO("Патч успешно применен к файлу '{}'. Статистика: добавлено={}, удалено={}, неизменено={}", 
                    path_str, stats.linesAdded, stats.linesRemoved, stats.linesUnchanged);
        return "Патч успешно применен к файлу '" + path_str + "'. Статистика: добавлено " + std::to_string(stats.linesAdded) + ", удалено " + std::to_string(stats.linesRemoved) + ", неизменено " + std::to_string(stats.linesUnchanged) + ".";
    } catch (const fs::filesystem_error& e) {
        // Восстановление из бэкапа
        SPDLOG_ERROR("Ошибка при атомарной замене файла: {}", std::string(e.what()));
        try {
            fs::rename(backupPath, file_path);
            SPDLOG_INFO("Файл восстановлен из бэкапа");
            return "{\"error\": \"Ошибка при применении патча. Файл восстановлен из бэкапа. Бэкап: " + backupPath + "\"}";
        } catch (...) {
            return "{\"error\": \"Ошибка при применении патча и восстановлении из бэкапа.\"}";
        }
    }
}

std::string ApplyDiffTool::dryRun(const nlohmann::json& args) const {
    if (!args.contains("path") || !args.contains("diff_content")) {
        return "{\"error\": \"Отсутствуют обязательные аргументы 'path' или 'diff_content'.\"}";
    }

    std::string path_str = args["path"];
    std::string diff_content = args["diff_content"];

    fs::path file_path(path_str);

    // Security check
    try {
        fs::path canonical_path = fs::weakly_canonical(file_path);
        fs::path canonical_current = fs::weakly_canonical(fs::current_path());
        if (canonical_path.string().find(canonical_current.string()) != 0) {
            return "{\"error\": \"Применение патчей разрешено только для файлов в пределах текущей директории проекта.\"}";
        }
    } catch (const fs::filesystem_error& e) {
        return "{\"error\": \"Ошибка при обработке пути: \" + std::string(e.what())}";
    }

    if (!fs::exists(file_path)) {
        return "{\"error\": \"Файл не найден: " + path_str + "\"}";
    }

    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        return "{\"error\": \"Не удалось открыть файл для чтения: " + path_str + "\"}";
    }
    auto original_lines = read_lines_from_stream(infile);
    infile.close();

    std::vector<std::string> diff_lines;
    std::stringstream diff_stream(diff_content);
    auto diff_line_result = read_lines_from_stream(diff_stream);
    diff_lines = std::move(diff_line_result);

    size_t original_line_idx = 0;
    size_t diff_line_idx = 0;
    DiffStatistics stats;

    // Пропуск заголовков diff
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
                original_line_idx++;
                stats.linesUnchanged++;
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
                    if (trim(original_lines[original_line_idx]) != trim(content)) {
                        return "{\"error\": \"Патч не применяется. Контекстная строка не совпадает в строке " + std::to_string(original_line_idx + 1) + ".\"}";
                    }
                    original_line_idx++;
                    stats.linesUnchanged++;
                } else if (type == '-') {
                    if (trim(original_lines[original_line_idx]) != trim(content)) {
                        return "{\"error\": \"Патч не применяется. Удаляемая строка не совпадает в строке " + std::to_string(original_line_idx + 1) + ".\"}";
                    }
                    original_line_idx++;
                    stats.linesRemoved++;
                } else if (type == '+') {
                    stats.linesAdded++;
                } else {
                    return "{\"error\": \"Неверный формат патча.\"}";
                }
                diff_line_idx++;
            }
        } else {
            diff_line_idx++;
        }
    }

    while (original_line_idx < original_lines.size()) {
        original_line_idx++;
        stats.linesUnchanged++;
    }

    return "Предпросмотр патча: добавлено " + std::to_string(stats.linesAdded) + ", удалено " + std::to_string(stats.linesRemoved) + ", неизменено " + std::to_string(stats.linesUnchanged) + ".";
}

std::string ApplyDiffTool::applyBackup(const std::string& backupPath) {
    try {
        fs::copy_file(backupPath, fs::current_path() / "restored_file.txt", fs::copy_options::overwrite_existing);
        return "Файл восстановлен из бэкапа.";
    } catch (const fs::filesystem_error& e) {
        return "{\"error\": \"Ошибка при восстановлении из бэкапа: \" + std::string(e.what())}";
    }
}

std::string ApplyDiffTool::getStatistics() const {
    return "Статистика изменений: добавлено=0, удалено=0, неизменено=0.";
}