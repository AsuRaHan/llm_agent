#include "Config.h"
#include "Logger.h"
#include <fstream>

using json = nlohmann::json;

void Config::create_default(const std::string& filepath) const {
    SPDLOG_INFO("Создание файла конфигурации по умолчанию: {}", filepath);
    json j;

    j["server"]["host"] = server_host;
    j["server"]["port"] = server_port;
    j["server"]["retry_count"] = retry_count;
    j["server"]["retry_delay_ms"] = retry_delay_ms;

    j["embedding"]["max_text_length"] = embedding_max_text_length;

    j["assistant"]["chat_completion_timeout_sec"] = chat_completion_timeout_sec;

    j["indexing"]["chunk_size"] = chunk_size;
    j["indexing"]["chunk_overlap"] = chunk_overlap;
    j["indexing"]["top_k_results"] = top_k_results;
    j["indexing"]["ignored_directories"] = { "build", ".git", ".vscode", "CMakeFiles" };
    j["indexing"]["ignored_extensions"] = {
        ".exe", ".obj", ".pdb", ".ilk", ".sln", ".vcxproj", ".filters", ".user",
        ".recipe", ".tlog", ".lastbuildstate", ".bin", ".stamp", ".cmake",
        ".json",
        ".log"
    };
    j["indexing"]["ignored_files"] = { ".gitignore" };

    std::ofstream file(filepath);
    file << j.dump(4);
}

bool Config::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        SPDLOG_WARN("Файл конфигурации '{}' не найден.", filepath);
        create_default(filepath);
        file.clear(); // Очищаем флаги ошибок перед повторной попыткой
        // Re-open the file after creating it
        file.open(filepath);
        if (!file.is_open()) {
            SPDLOG_CRITICAL("Не удалось создать или открыть файл конфигурации '{}'.", filepath);
            return false;
        }
    }

    try {
        json j;
        file >> j;

        // Safely get values, using defaults if a key is missing
        server_host = j.value("/server/host"_json_pointer, server_host);
        server_port = j.value("/server/port"_json_pointer, server_port);
        retry_count = j.value("/server/retry_count"_json_pointer, retry_count);
        retry_delay_ms = j.value("/server/retry_delay_ms"_json_pointer, retry_delay_ms);

        embedding_max_text_length = j.value("/embedding/max_text_length"_json_pointer, embedding_max_text_length);

        chat_completion_timeout_sec = j.value("/assistant/chat_completion_timeout_sec"_json_pointer, chat_completion_timeout_sec);

        chunk_size = j.value("/indexing/chunk_size"_json_pointer, chunk_size);
        chunk_overlap = j.value("/indexing/chunk_overlap"_json_pointer, chunk_overlap);
        top_k_results = j.value("/indexing/top_k_results"_json_pointer, top_k_results);
        
        if (j.contains("indexing") && j["indexing"].is_object()) {
            if (j["indexing"].contains("ignored_directories"))
                ignored_directories = j["indexing"]["ignored_directories"].get<std::vector<std::string>>();
            if (j["indexing"].contains("ignored_extensions"))
                ignored_extensions = j["indexing"]["ignored_extensions"].get<std::vector<std::string>>();
            if (j["indexing"].contains("ignored_files"))
                ignored_files = j["indexing"]["ignored_files"].get<std::vector<std::string>>();
        }

        SPDLOG_INFO("Конфигурация успешно загружена из '{}'.", filepath);
        return true;

    } catch (const json::exception& e) {
        SPDLOG_CRITICAL("Ошибка парсинга файла конфигурации '{}': {}", filepath, e.what());
        return false;
    }
}