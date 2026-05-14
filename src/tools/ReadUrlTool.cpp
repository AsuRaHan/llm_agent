#include "ReadUrlTool.h"
#include "../Logger.h"
#include <httplib.h>
#include <regex>
#include <memory>

std::string ReadUrlTool::getName() const {
    return "read_url";
}

std::string ReadUrlTool::getDescription() const {
    return "Загружает и возвращает текстовое содержимое (HTML) веб-страницы по указанному URL. НЕ открывает URL в браузере. Полезно для чтения статей, документации или GitHub issues, ссылки на которые были найдены с помощью 'web_search'.";
}

nlohmann::json ReadUrlTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {
                {"type", "string"},
                {"description", "Полный URL-адрес страницы, которую нужно прочитать."}
            }}
        }},
        {"required", {"url"}}
    };
}

std::string ReadUrlTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("url")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'url'.\"}";
    }
    std::string url_str = args["url"];
    SPDLOG_INFO("[Tool:read_url] Чтение URL: '{}'", url_str);

    try {
        httplib::SSLClient cli(url_str);
        cli.set_follow_location(true); // Следовать за редиректами
        cli.set_connection_timeout(10);
        cli.set_read_timeout(20);

        // Некоторые сайты блокируют запросы без User-Agent
        httplib::Headers headers = {
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36"}
        };

        auto res = cli.Get("/", headers); // Path is now part of the client constructor

        if (!res) {
            auto err = res.error();
            std::string error_msg = "Ошибка соединения при чтении URL: " + httplib::to_string(err);
            SPDLOG_ERROR("[Tool:read_url] {}", error_msg);
            return "{\"error\": \"" + error_msg + "\"}";
        }

        if (res->status >= 400) {
            std::string error_msg = "Сервер вернул ошибку при чтении URL. Статус: " + std::to_string(res->status) + ". Ответ: " + res->body;
            SPDLOG_ERROR("[Tool:read_url] {}", error_msg);
            return "{\"error\": \"" + error_msg + "\"}";
        }
        
        // Ограничиваем размер контента, чтобы не переполнить контекст модели
        const size_t max_content_length = 100000; // 100k символов
        if (res->body.length() > max_content_length) {
            SPDLOG_WARN("[Tool:read_url] Содержимое URL '{}' слишком велико ({} символов), обрезано до {}.", url_str, res->body.length(), max_content_length);
            return res->body.substr(0, max_content_length) + "\n\n... (содержимое было обрезано)";
        }

        return res->body;

    } catch (const std::exception& e) {
        std::string error_msg = "Исключение при чтении URL: " + std::string(e.what());
        SPDLOG_ERROR("[Tool:read_url] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }
}