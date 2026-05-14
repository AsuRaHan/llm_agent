#include "WebSearchTool.h"
#include "../Logger.h"
#include <httplib.h>
#include <sstream>

WebSearchTool::WebSearchTool(const Config& config) : config(config) {}

std::string WebSearchTool::getName() const {
    return "web_search";
}

std::string WebSearchTool::getDescription() const {
    return "Ищет информацию в интернете с помощью поисковой системы. Используй это для общих вопросов о программировании, поиска документации, ошибок или информации, которой нет в кодовой базе проекта.";
}

nlohmann::json WebSearchTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"description", "Поисковый запрос, который нужно отправить в интернет."}
            }}
        }},
        {"required", {"query"}}
    };
}

std::string WebSearchTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (config.web_search_api_key.empty()) {
        SPDLOG_ERROR("[Tool:web_search] API ключ для веб-поиска не указан в config.json.");
        return "{\"error\": \"API ключ для веб-поиска не настроен.\"}";
    }

    if (!args.contains("query")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'query'.\"}";
    }
    std::string query = args["query"];
    SPDLOG_INFO("[Tool:web_search] Поиск в интернете: '{}'", query);

    // Serper.dev requires an HTTPS client. httplib handles this automatically.
    httplib::Client cli("google.serper.dev");
    cli.set_connection_timeout(10);
    cli.set_read_timeout(20);

    nlohmann::json body = {
        {"q", query}
    };

    httplib::Headers headers = {
        {"X-API-KEY", config.web_search_api_key},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/search", headers, body.dump(), "application/json");

    if (!res) {
        auto err = res.error();
        std::string error_msg = "Ошибка соединения с сервисом веб-поиска: " + httplib::to_string(err);
        SPDLOG_ERROR("[Tool:web_search] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    if (res->status != 200) {
        std::string error_msg = "Сервис веб-поиска вернул ошибку. Статус: " + std::to_string(res->status) + ". Ответ: " + res->body;
        SPDLOG_ERROR("[Tool:web_search] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    try {
        nlohmann::json response_json = nlohmann::json::parse(res->body);
        std::stringstream result_ss;
        result_ss << "Результаты веб-поиска по запросу '" << query << "':\n\n";

        if (response_json.contains("organic")) {
            int count = 0;
            for (const auto& item : response_json["organic"]) {
                if (count >= 5) break; // Ограничимся 5 результатами
                result_ss << (count + 1) << ". " << item.value("title", "Без заголовка") << "\n";
                result_ss << "   Ссылка: " << item.value("link", "Нет ссылки") << "\n";
                result_ss << "   Фрагмент: " << item.value("snippet", "Нет описания") << "\n\n";
                count++;
            }
        } else {
             result_ss << "В ответе API не найдены органические результаты.";
        }
        return result_ss.str();
    } catch (const nlohmann::json::exception& e) {
        std::string error_msg = "Не удалось разобрать JSON-ответ от сервиса веб-поиска: " + std::string(e.what());
        SPDLOG_ERROR("[Tool:web_search] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }
}