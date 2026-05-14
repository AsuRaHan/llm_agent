#include "GitHubSearchTool.h"
#include "../Logger.h"
#include <httplib.h>
#include <sstream>
#include <iomanip> // For std::quoted

// Вспомогательная функция для URL-кодирования строки, так как в httplib нет публичной.
std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Оставляем безопасные символы как есть
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        // Все остальные символы кодируем
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }
    return escaped.str();
}

std::string GitHubSearchTool::getName() const {
    return "github_repository_search";
}

std::string GitHubSearchTool::getDescription() const {
    return "Ищет публичные репозитории на GitHub по ключевым словам. Полезно для поиска библиотек, фреймворков или примеров кода по заданной теме.";
}

nlohmann::json GitHubSearchTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"description", "Поисковый запрос для поиска репозиториев на GitHub."}
            }},
            {"type", {
                {"type", "string"},
                {"description", "Тип поиска на GitHub. Может быть 'repositories' (репозитории), 'code' (код), 'issues' (проблемы), 'commits' (коммиты), 'users' (пользователи). По умолчанию 'repositories'."},
                {"enum", {"repositories", "code", "issues", "commits", "users"}},
                {"default", "repositories"}
            }}
        }},
        {"required", {"query"}}
    };
}

// Вспомогательная функция для форматирования результатов поиска кода
std::string formatCodeSearchResults(const nlohmann::json& items) {
    std::stringstream ss;
    int count = 0;
    for (const auto& item : items) {
        if (count >= 5) break; // Ограничимся 5 результатами
        ss << (count + 1) << ". Файл: " << item.value("path", "Без пути") << " в репозитории: " << item["repository"].value("full_name", "Без имени") << "\n";
        ss << "   Ссылка: " << item.value("html_url", "Нет ссылки") << "\n";
        ss << "   Фрагмент: " << item.value("snippet", "Нет описания") << "\n\n"; // GitHub Code Search API может возвращать 'text_matches' с фрагментами
        count++;
    }
    return ss.str();
}

std::string GitHubSearchTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!args.contains("query")) {
        return "{\"error\": \"Отсутствует обязательный аргумент 'query'.\"}";
    }
    std::string query = args["query"];
    std::string search_type = args.value("type", "repositories"); // Получаем тип поиска, по умолчанию 'repositories'
    SPDLOG_INFO("[Tool:github_repository_search] Поиск на GitHub: '{}'", query);

    httplib::SSLClient cli("api.github.com");
    cli.set_connection_timeout(10);
    cli.set_read_timeout(20);

    // GitHub API требует User-Agent
    httplib::Headers headers = {
        {"User-Agent", "Smart-Hammer-Agent/1.0"}
    };

    // Формируем URL с параметрами запроса
    std::string path;
    if (search_type == "repositories") {
        path = "/search/repositories?q=" + url_encode(query) + "&sort=stars&order=desc";
    } else if (search_type == "code") {
        path = "/search/code?q=" + url_encode(query);
        // Для поиска кода полезно добавить Accept заголовок для получения фрагментов
        headers.emplace("Accept", "application/vnd.github.text-match+json");
    } else if (search_type == "issues") {
        path = "/search/issues?q=" + url_encode(query) + "&sort=updated&order=desc";
    } else if (search_type == "commits") {
        path = "/search/commits?q=" + url_encode(query) + "&sort=committer-date&order=desc";
    } else if (search_type == "users") {
        path = "/search/users?q=" + url_encode(query) + "&sort=followers&order=desc";
    } else {
        // Fallback to repositories if an unknown type is provided
        SPDLOG_WARN("[Tool:github_repository_search] Неизвестный тип поиска '{}'. Используется 'repositories'.", search_type);
        path = "/search/repositories?q=" + url_encode(query) + "&sort=stars&order=desc";
        search_type = "repositories";
    }

    auto res = cli.Get(path, headers);

    if (!res) {
        auto err = res.error();
        std::string error_msg = "Ошибка соединения с GitHub API: " + httplib::to_string(err);
        SPDLOG_ERROR("[Tool:github_repository_search] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    if (res->status != 200) {
        std::string error_msg = "GitHub API вернул ошибку. Статус: " + std::to_string(res->status) + ". Ответ: " + res->body;
        SPDLOG_ERROR("[Tool:github_repository_search] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }

    try {
        nlohmann::json response_json = nlohmann::json::parse(res->body);
        std::stringstream result_ss;
        result_ss << "Результаты поиска на GitHub (тип: " << search_type << ") по запросу '" << query << "':\n\n";

        if (response_json.contains("items") && response_json["items"].is_array()) {
            if (search_type == "repositories") {
                int count = 0;
                for (const auto& item : response_json["items"]) {
                    if (count >= 5) break; // Ограничимся 5 результатами
                    result_ss << (count + 1) << ". " << item.value("full_name", "Без имени") << " (⭐ " << item.value("stargazers_count", 0) << ")\n";
                    result_ss << "   Ссылка: " << item.value("html_url", "Нет ссылки") << "\n";
                    result_ss << "   Описание: " << item.value("description", "Нет описания") << "\n\n";
                    count++;
                }
            } else if (search_type == "code") {
                result_ss << formatCodeSearchResults(response_json["items"]);
            } else if (search_type == "issues") {
                // TODO: Implement specific formatting for issues
                result_ss << "Найдены проблемы (issues). Для детального просмотра используйте read_url.\n";
                result_ss << response_json["items"].dump(2); // Dump raw JSON for now
            } else if (search_type == "commits") {
                // TODO: Implement specific formatting for commits
                result_ss << "Найдены коммиты. Для детального просмотра используйте read_url.\n";
                result_ss << response_json["items"].dump(2); // Dump raw JSON for now
            } else if (search_type == "users") {
                // TODO: Implement specific formatting for users
                result_ss << "Найдены пользователи. Для детального просмотра используйте read_url.\n";
                result_ss << response_json["items"].dump(2); // Dump raw JSON for now
            }

            if (response_json["items"].empty()) {
                result_ss << "Ничего не найдено.";
            }
        } else {
             result_ss << "В ответе API не найдены элементы.";
        }
        return result_ss.str();
    } catch (const nlohmann::json::exception& e) {
        std::string error_msg = "Не удалось разобрать JSON-ответ от GitHub API: " + std::string(e.what());
        SPDLOG_ERROR("[Tool:github_repository_search] {}", error_msg);
        return "{\"error\": \"" + error_msg + "\"}";
    }
}