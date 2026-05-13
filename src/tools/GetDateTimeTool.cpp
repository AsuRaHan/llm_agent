#include "GetDateTimeTool.h"
#include "../Logger.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

std::string GetDateTimeTool::getName() const {
    return "get_datetime";
}

std::string GetDateTimeTool::getDescription() const {
    return "Возвращает текущую дату и время в формате ISO 8601 (YYYY-MM-DDTHH:MM:SSZ). Полезно для добавления временных меток в логи или файлы.";
}

nlohmann::json GetDateTimeTool::getParameters() const {
    // Этот инструмент не имеет параметров
    return {
        {"type", "object"},
        {"properties", {}},
        {"required", nlohmann::json::array()}
    };
}

std::string GetDateTimeTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    // Этот инструмент игнорирует любые аргументы
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    // Используем потокобезопасные версии gmtime для форматирования в UTC
#ifdef _WIN32
    struct tm buf;
    gmtime_s(&buf, &in_time_t);
    ss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%SZ");
#else
    struct tm buf;
    gmtime_r(&in_time_t, &buf);
    ss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%SZ");
#endif

    std::string result = ss.str();
    SPDLOG_INFO("Выполнение get_datetime. Результат: {}", result);
    return result;
}