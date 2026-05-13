#include "Config.h"
#include "Logger.h"
#include <fstream>

using json = nlohmann::json;

void Config::create_default(const std::string& filepath) const {
    SPDLOG_INFO("Создание файла конфигурации по умолчанию: {}", filepath);
    json j;

    j["server"]["host"] = server_host;
    j["server"]["port"] = server_port;
    j["server"]["api_key"] = api_key;
    j["server"]["retry_count"] = retry_count;
    j["server"]["retry_delay_ms"] = retry_delay_ms;

    j["embedding"]["max_text_length"] = embedding_max_text_length;
    j["embedding"]["chunk_overlap"] = embedding_chunk_overlap;
    j["embedding"]["model_name"] = embedding_model_name;

    j["assistant"]["chat_completion_timeout_sec"] = chat_completion_timeout_sec;
    j["assistant"]["max_tool_calls"] = max_tool_calls;
    j["assistant"]["model_name"] = chat_model_name;

    j["tools"]["enable_dangerous_tools"] = enable_dangerous_tools;

    j["logging"]["log_file_path"] = log_file_path;
    j["logging"]["log_to_console"] = log_to_console;

    j["indexing"]["chunk_size"] = chunk_size;
    j["indexing"]["chunk_overlap"] = chunk_overlap;
    j["indexing"]["top_k_results"] = top_k_results;
    j["indexing"]["chunking_strategy"] = "tree-sitter-hybrid";
    j["indexing"]["ignored_directories"] = { "build", ".git", ".vscode", "CMakeFiles" };
    j["indexing"]["ignored_extensions"] = {
        ".exe", ".obj", ".pdb", ".ilk", ".sln", ".vcxproj", ".filters", ".user",
        ".recipe", ".tlog", ".lastbuildstate", ".bin", ".stamp", ".cmake",
        ".json", ".log"
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
        api_key = j.value("/server/api_key"_json_pointer, api_key);
        retry_count = j.value("/server/retry_count"_json_pointer, retry_count);
        retry_delay_ms = j.value("/server/retry_delay_ms"_json_pointer, retry_delay_ms);

        embedding_max_text_length = j.value("/embedding/max_text_length"_json_pointer, embedding_max_text_length);
        embedding_chunk_overlap = j.value("/embedding/chunk_overlap"_json_pointer, embedding_chunk_overlap);
        embedding_model_name = j.value("/embedding/model_name"_json_pointer, embedding_model_name);

        chat_completion_timeout_sec = j.value("/assistant/chat_completion_timeout_sec"_json_pointer, chat_completion_timeout_sec);
        max_tool_calls = j.value("/assistant/max_tool_calls"_json_pointer, max_tool_calls);
        chat_model_name = j.value("/assistant/model_name"_json_pointer, chat_model_name);

        enable_dangerous_tools = j.value("/tools/enable_dangerous_tools"_json_pointer, enable_dangerous_tools);

        log_file_path = j.value("/logging/log_file_path"_json_pointer, log_file_path);
        log_to_console = j.value("/logging/log_to_console"_json_pointer, log_to_console);

        chunk_size = j.value("/indexing/chunk_size"_json_pointer, chunk_size);
        chunk_overlap = j.value("/indexing/chunk_overlap"_json_pointer, chunk_overlap);
        top_k_results = j.value("/indexing/top_k_results"_json_pointer, top_k_results);
        chunking_strategy = j.value("/indexing/chunking_strategy"_json_pointer, chunking_strategy);
        
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