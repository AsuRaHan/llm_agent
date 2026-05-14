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
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#ifdef _WIN32
std::string SH_GetWindowsVersion() {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((OSVERSIONINFO*)&osvi);
    std::ostringstream oss;
    oss << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
    return oss.str();
}

std::string SH_GetWindowsArchitecture() {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "64-bit";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "32-bit";
        default:
            return "Unknown";
    }
}
#endif

std::string ExecuteShellCommandTool::getName() const {
    return "execute_shell_command";
}

std::string ExecuteShellCommandTool::getDescription() const {
    std::string os_name, os_version, architecture;
#ifdef _WIN32
    os_name = "Windows";
    os_version = SH_GetWindowsVersion();
    architecture = SH_GetWindowsArchitecture();
#elif defined(__linux__)
    os_name = "Linux";
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        os_version = buffer.release;
        architecture = buffer.machine;
    } else {
        os_version = "Unknown";
        architecture = "Unknown";
    }
#elif defined(__APPLE__) && defined(__MACH__)
    os_name = "macOS";
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        os_version = buffer.release;
        architecture = buffer.machine;
    } else {
        os_version = "Unknown";
        architecture = "Unknown";
    }
#else
    os_name = "Unknown";
    os_version = "Unknown";
    architecture = "Unknown";
#endif
    return "Выполняет команду в системной оболочке. Операционная система в которой ты работаешь: " + os_name + " версия " + os_version + " архитектора " + architecture + " при выполнении команд учитывай это.\nВНИМАНИЕ: Это очень опасный инструмент. Он может изменять файлы, выполнять произвольный код и взаимодействовать с операционной системой. Используйте с предельной осторожностью.";
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

// Вспомогательная функция для выполнения команды и получения ее вывода.
std::string execute_command(const char* cmd) {
#ifdef _WIN32
    // На Windows мы используем WinAPI для надежного захвата вывода и поддержки UTF-8.
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "{\"error\": \"Не удалось создать pipe для вывода команды.\"}";
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    // Конвертируем команду из UTF-8 в wide string для CreateProcessW
    // Принудительно переключаем кодовую страницу на UTF-8 (65001) для дочернего процесса
    // Запускаем команду через `cmd.exe /C` для правильной работы PATH и встроенных команд
    std::string command_str = "cmd.exe /C \"chcp 65001 > nul && " + std::string(cmd) + "\"";
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, command_str.c_str(), -1, NULL, 0);
    if (wide_len == 0) {
        return "{\"error\": \"Не удалось конвертировать команду в wide string.\"}";
    }
    std::wstring wide_cmd(wide_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, command_str.c_str(), -1, &wide_cmd[0], wide_len);

    // Создаем процесс без окна консоли
    if (!CreateProcessW(NULL, &wide_cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return "{\"error\": \"Не удалось создать процесс. Код ошибки: " + std::to_string(GetLastError()) + "\"}";
    }

    CloseHandle(hWrite); // Закрываем конец для записи

    // Читаем вывод из pipe
    std::string result;
    char buffer[256];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead != 0) {
        result.append(buffer, bytesRead);
    }

    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
#else
    // Стандартная реализация для POSIX (Linux, macOS) с помощью popen
    std::array<char, 128> buffer;
    std::string result;
    std::string cmd_with_redirect = std::string(cmd) + " 2>&1"; // Перенаправляем stderr в stdout
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd_with_redirect.c_str(), "r"), pclose);
    if (!pipe) {
        return "{\"error\": \"popen() failed!\"}";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
#endif
}

std::string ExecuteShellCommandTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("command")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'command'.\"}";
    }
    std::string command = args["command"];
    SPDLOG_WARN("Выполнение ОПАСНОЙ команды в shell: '{}'", command);

    try {
        std::string result = execute_command(command.c_str());
        SPDLOG_INFO("Результат выполнения команды:\n{}", result);
        return result;
    } catch (const std::exception& e) {
        std::string error_msg = "Исключение при выполнении команды: " + std::string(e.what());
        SPDLOG_ERROR(error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }
}