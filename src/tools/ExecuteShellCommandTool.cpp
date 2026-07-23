#include "ExecuteShellCommandTool.h"
#include "../Logger.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#ifdef _WIN32
#include <windows.h>
#endif

// Helper function to convert OEM encoded string to UTF-8 on Windows
#ifdef _WIN32
std::string oem_to_utf8(const std::string& oem_str) {
    if (oem_str.empty()) {
        return "";
    }
    // Get required buffer size for wide char
    int wide_len = MultiByteToWideChar(CP_OEMCP, 0, oem_str.c_str(), -1, NULL, 0);
    if (wide_len == 0) {
        return "[Encoding Error: OEM to Wide failed]";
    }
    std::wstring wide_str(wide_len, 0);
    MultiByteToWideChar(CP_OEMCP, 0, oem_str.c_str(), -1, &wide_str[0], wide_len);

    // Get required buffer size for UTF-8
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8_len == 0) {
        return "[Encoding Error: Wide to UTF-8 failed]";
    }
    std::string utf8_str(utf8_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, &utf8_str[0], utf8_len, NULL, NULL);
    
    if (!utf8_str.empty() && utf8_str.back() == '\0') {
        utf8_str.pop_back();
    }

    return utf8_str;
}
#endif

std::string ExecuteShellCommandTool::getName() const {
    return "execute_shell_command";
}

std::string ExecuteShellCommandTool::getDescription() const {
    return "Выполняет команду в системной оболочке (shell/cmd). ОСТОРОЖНО: Эта команда может изменять файлы или состояние системы. Используй ее только если уверен в своих действиях. Возвращает JSON с полями 'output', 'exit_code' и, в случае неудачи, 'error'.";
}

nlohmann::json ExecuteShellCommandTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Команда для выполнения в shell."}
            }}
        }},
        {"required", {"command"}}
    };
}

std::string ExecuteShellCommandTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("command")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'command'.\"}";
    }
    std::string command = args["command"];

    SPDLOG_WARN("[Tool:execute_shell_command] Выполнение ОПАСНОЙ команды в shell: '{}'", command);

    std::string result_str;
    int exit_code = -1;

#ifdef _WIN32
    std::shared_ptr<FILE> pipe(_popen(command.c_str(), "r"), _pclose);
#else
    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
#endif

    if (!pipe) {
        return "{\"error\": \"Не удалось выполнить команду _popen.\"}";
    }

    std::array<char, 256> buffer;
    std::string raw_output;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        raw_output += buffer.data();
    }

#ifdef _WIN32
    exit_code = _pclose(pipe.get());
    result_str = oem_to_utf8(raw_output);
#else
    int status = pclose(pipe.get());
    if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
    result_str = raw_output; // Assuming Linux is already UTF-8
#endif

    SPDLOG_INFO("[Tool:execute_shell_command] Команда завершилась с кодом {}. Результат:\n{}", exit_code, result_str);

    nlohmann::json result_json;
    if (exit_code != 0) {
        result_json["error"] = "Команда завершилась с ошибкой (код " + std::to_string(exit_code) + ").";
    }
    result_json["exit_code"] = exit_code;
    result_json["output"] = result_str;

    return result_json.dump();
}