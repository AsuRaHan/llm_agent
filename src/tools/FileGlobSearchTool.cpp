#include "FileGlobSearchTool.h"
#include "../Logger.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <algorithm>

namespace fs = std::filesystem;

FileGlobSearchTool::FileGlobSearchTool(const Config& config) : config(config) {}

std::string FileGlobSearchTool::getName() const {
    return "file_glob_search";
}

std::string FileGlobSearchTool::getDescription() const {
    return "Ищет файлы в директории проекта, используя glob-паттерн (например, '*.cpp', 'src/*Tool.h'). Возвращает список совпадающих путей к файлам.";
}

nlohmann::json FileGlobSearchTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "Glob-паттерн для поиска. Используйте '*' как wildcard для любых символов и '?' для одного символа."}
            }}
        }},
        {"required", {"pattern"}}
    };
}

std::string FileGlobSearchTool::globToRegex(const std::string& glob) const {
    std::string out;
    out.reserve(glob.size());
    for (char c : glob) {
        switch (c) {
            case '*':  out += ".*"; break;
            case '?':  out += '.';  break;
            case '.':  out += "\\."; break;
            case '\\': out += "\\\\"; break;
            // Экранируем другие специальные символы regex
            case '^': case '$': case '|': case '(': case ')': case '[': case ']': case '{': case '}': case '+':
                out += '\\';
                out += c;
                break;
            default:   out += c;   break;
        }
    }
    return out;
}

std::string FileGlobSearchTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("pattern")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'pattern'.\"}";
    }
    std::string pattern = args["pattern"];
    SPDLOG_INFO("Выполнение file_glob_search с паттерном: '{}'", pattern);

    std::regex file_regex;
    try {
        file_regex.assign(globToRegex(pattern), std::regex::icase); // Поиск без учета регистра
    } catch (const std::regex_error& e) {
        std::string error_msg = "Неверный glob-паттерн, который привел к ошибке регулярного выражения: " + std::string(e.what());
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    std::vector<std::string> found_files;
    std::unordered_set<std::string> ignored_dirs(config.ignored_directories.begin(), config.ignored_directories.end());

    try {
        auto it = fs::recursive_directory_iterator(".");
        for (const auto& entry : it) {
            if (entry.is_directory() && ignored_dirs.count(entry.path().filename().string())) {
                it.disable_recursion_pending(); // Не заходим в игнорируемые директории
                continue;
            }

            if (entry.is_regular_file()) {
                std::string path_str = fs::weakly_canonical(entry.path()).string();
                std::replace(path_str.begin(), path_str.end(), '\\', '/');

                if (std::regex_search(path_str, file_regex)) {
                    found_files.push_back(path_str);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::string error_msg = "Ошибка во время поиска файлов: " + std::string(e.what());
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    if (found_files.empty()) {
        return "Не найдено файлов, соответствующих паттерну '" + pattern + "'.";
    }

    std::stringstream ss;
    ss << "Найдено " << found_files.size() << " файлов по паттерну '" << pattern << "':\n";
    for (const auto& file : found_files) {
        ss << "- " << file << "\n";
    }
    return ss.str();
}