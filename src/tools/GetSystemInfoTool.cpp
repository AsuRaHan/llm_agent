#include "GetSystemInfoTool.h"
#include "../Logger.h"
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/utsname.h>
#include <unistd.h>
#endif

std::string GetSystemInfoTool::getName() const {
    return "get_system_info";
}

std::string GetSystemInfoTool::getDescription() const {
    return "Возвращает информацию об операционной системе, на которой запущен агент (например, 'Windows', 'Linux', 'macOS').";
}

nlohmann::json GetSystemInfoTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {}},
        {"required", nlohmann::json::array()}
    };
}

#ifdef _WIN32
std::string GetWindowsVersion() {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((OSVERSIONINFO*)&osvi);
    std::ostringstream oss;
    oss << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
    return oss.str();
}

std::string GetWindowsArchitecture() {
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

std::string GetSystemInfoTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    std::string os_name, os_version, architecture;
    
#ifdef _WIN32
    os_name = "Windows";
    os_version = GetWindowsVersion();
    architecture = GetWindowsArchitecture();
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
    
    nlohmann::json result = {
        {"operating_system", os_name},
        {"version", os_version},
        {"architecture", architecture}
    };
    
    SPDLOG_INFO("Выполнение get_system_info. Результат: {}", result.dump());
    return result.dump();
}
