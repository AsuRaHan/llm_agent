#include "GetSystemInfoTool.h"
#include "../Logger.h"

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

std::string GetSystemInfoTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    std::string os_name;
#ifdef _WIN32
    os_name = "Windows";
#elif defined(__linux__)
    os_name = "Linux";
#elif defined(__APPLE__) && defined(__MACH__)
    os_name = "macOS";
#else
    os_name = "Unknown";
#endif
    
    std::string result = "{\"operating_system\": \"" + os_name + "\"}";
    SPDLOG_INFO("Выполнение get_system_info. Результат: {}", result);
    return result;
}