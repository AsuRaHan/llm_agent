#include "OpenUrlTool.h"
#include "../Logger.h"
#include <cstdlib> // For system()

std::string OpenUrlTool::getName() const {
    return "open_url_in_browser";
}

std::string OpenUrlTool::getDescription() const {
    return "Открывает указанный URL в веб-браузере по умолчанию на компьютере пользователя. Используй это, когда пользователь просит открыть ссылку.";
}

nlohmann::json OpenUrlTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {
                {"type", "string"},
                {"description", "Полный URL-адрес, который нужно открыть в браузере."}
            }}
        }},
        {"required", {"url"}}
    };
}

std::string OpenUrlTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("url")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'url'.\"}";
    }
    std::string url = args["url"];
    SPDLOG_INFO("[Tool:open_url_in_browser] Открытие URL в браузере: '{}'", url);

    std::string command;
#ifdef _WIN32
    command = "start \"\" \"" + url + "\"";
#elif __APPLE__
    command = "open \"" + url + "\"";
#else
    command = "xdg-open \"" + url + "\"";
#endif

    int result = std::system(command.c_str());

    if (result == 0) {
        return "URL '" + url + "' успешно открыт в браузере по умолчанию.";
    } else {
        return "{\"error\": \"Не удалось выполнить команду для открытия URL. Код возврата: " + std::to_string(result) + "\"}";
    }
}