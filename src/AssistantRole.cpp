#include "AssistantRole.h"
#include "Logger.h"
#include "ContextIndexer.h" // For SearchResult definition
#include <thread>
#include <chrono>
#include <sstream>

#include "Config.h"

using json = nlohmann::json;

AssistantRole::AssistantRole(const Config& config) : config(config), cli(config.server_host, config.server_port) {
    cli.set_connection_timeout(config.chat_completion_timeout_sec, 0);
}

std::string AssistantRole::answerWithContext(const std::string& userQuery, const std::vector<SearchResult>& searchResults) {
    std::stringstream prompt_context;
    prompt_context << "Ты — эксперт-программист. Ответь на вопрос пользователя, основываясь на следующих наиболее релевантных фрагментах кода из проекта. "
                   << "Структурируй свой ответ, будь точным и, если уместно, ссылайся на файлы-источники.\n\n"
                   << "Вопрос пользователя: " << userQuery << "\n\n"
                   << "Контекст из проекта:\n";

    for (const auto& result : searchResults) {
        prompt_context << "--- ИЗ ФАЙЛА: " << result.filePath << " (схожесть: " << std::fixed << std::setprecision(2) << result.score << ") ---\n"
                       << "```\n"
                       << result.chunkText << "\n"
                       << "```\n\n";
    }
    prompt_context << "Проанализируй предоставленный контекст и дай исчерпывающий ответ на вопрос пользователя.";

    std::string prompt_text = prompt_context.str();

    SPDLOG_INFO("Генерация ответа с использованием {} фрагментов контекста...", searchResults.size());

    json body = {
        {"messages", json::array({
            { {"role", "system"}, {"content", "Вы — полезный помощник для программистов."} },
            { {"role", "user"}, {"content", prompt_text} }
        })},
        {"temperature", 0.3}
    };

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        res = cli.Post("/v1/chat/completions", body_str, "application/json");
        if (res) { // If we got any response (even an error status), we can break.
            break;
        }
        // If res is false, it's a connection error.
        SPDLOG_WARN("[AssistantRole] Попытка {}/{} для генерации ответа не удалась. Ошибка соединения: {}. Повтор через {} мс...",
                    attempt, config.retry_count, httplib::to_string(res.error()), config.retry_delay_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            if (json_body.contains("choices") && json_body["choices"].is_array() && !json_body["choices"].empty()) {
                const auto& first_choice = json_body["choices"][0];
                if (first_choice.contains("message") && first_choice["message"].contains("content")) {
                    return first_choice["message"]["content"].get<std::string>();
                }
            }
        } catch (const json::exception& e) { // Catch any nlohmann::json exception
            SPDLOG_ERROR("Failed to parse JSON response. Details: {}", e.what());
            return "Ошибка обработки ответа от модели.";
        }
    } else {
        if (res) { // HTTP error
            SPDLOG_ERROR("Failed to get analysis. Status: {}", res->status);
            SPDLOG_ERROR("Server response: {}", res->body);
        } else { // Connection error
            auto err = res.error();
            SPDLOG_ERROR("Failed to get analysis. Connection error: {}", httplib::to_string(err));
        }
        return "Не удалось связаться с моделью.";
    }

    return "Получен неожиданный ответ от модели.";
}

std::string AssistantRole::generateProjectSummaryGreeting(int file_count, int embedding_count) {
    SPDLOG_INFO("Генерация приветственного сообщения о проекте...");

    std::string prompt_text = 
        "Ты — остроумный и дружелюбный ИИ-ассистент для программиста. Ты только что закончил "
        "сканирование его проекта. Ты проиндексировал " + std::to_string(file_count) + 
        " файлов и создал " + std::to_string(embedding_count) + 
        " смысловых 'воспоминаний' (эмбеддингов). Твоя задача — сгенерировать короткое, "
        "креативное и ободряющее приветственное сообщение для разработчика. Дай ему понять, "
        "что ты в сети и готов помочь разобраться в коде. Не просто констатируй факты, "
        "прояви немного индивидуальности. Говори от первого лица. И не много расскажи о самом проекте который ты проанализировал.";

    json body = {
        {"messages", json::array({
            { {"role", "system"}, {"content", "Ты — полезный ИИ-ассистент."} },
            { {"role", "user"}, {"content", prompt_text} }
        })},
        {"temperature", 0.7} // A bit more creative
    };

    httplib::Result res;
    // Using a simplified retry logic for this non-critical call
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        res = cli.Post("/v1/chat/completions", body_str, "application/json");
        if (res) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            if (json_body.contains("choices") && !json_body["choices"].empty()) {
                return json_body["choices"][0]["message"]["content"].get<std::string>();
            }
        } catch (const json::exception& e) {
            SPDLOG_ERROR("[AssistantRole] Не удалось разобрать ответ для приветствия: {}", e.what());
        }
    }
    
    // Fallback message if AI fails
    return "Привет! Я просканировал твой проект. Готов отвечать на вопросы.";
}

std::string AssistantRole::generateChunkSummary(const std::string& codeChunk, const std::string& chunkName) {
    SPDLOG_DEBUG("[AssistantRole] Запрос на генерацию саммари для чанка '{}'...", chunkName);

    std::string system_prompt =
        "Твоя задача — создать очень короткое, лаконичное саммари (одно два предложения) для предоставленного фрагмента кода. "
        "Саммари должно описывать основное назначение или действие этого кода. "
        "Отвечай только текстом саммари, без лишних слов и преамбул.";

    std::string user_prompt = "Создай саммари для этого кода:\n```\n" + codeChunk + "\n```";

    json body = {
        {"messages", json::array({
            { {"role", "system"}, {"content", system_prompt} },
            { {"role", "user"}, {"content", user_prompt} }
        })},
        {"temperature", 0.0}, // Factual summary
        {"max_tokens", 200}   // Limit response size
    };

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        res = cli.Post("/v1/chat/completions", body_str, "application/json");
        if (res) break;
        SPDLOG_WARN("[AssistantRole] Попытка {}/{} для саммари '{}' не удалась. Ошибка соединения: {}. Повтор...",
                    attempt, config.retry_count, chunkName, httplib::to_string(res.error()));
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            if (json_body.contains("choices") && !json_body["choices"].empty()) {
                std::string summary = json_body["choices"][0]["message"]["content"].get<std::string>();
                // Clean up summary: remove quotes and newlines
                summary.erase(std::remove(summary.begin(), summary.end(), '\"'), summary.end());
                summary.erase(std::remove(summary.begin(), summary.end(), '\n'), summary.end());
                summary.erase(std::remove(summary.begin(), summary.end(), '\r'), summary.end());
                SPDLOG_DEBUG("[AssistantRole] Саммари для '{}': {}", chunkName, summary);
                return summary;
            }
        } catch (const json::exception& e) {
            SPDLOG_ERROR("[AssistantRole] Не удалось разобрать ответ для саммари '{}': {}", chunkName, e.what());
        }
    }

    SPDLOG_WARN("[AssistantRole] Не удалось сгенерировать саммари для '{}'. Будет использован пустой текст.", chunkName);
    return ""; // Return empty string on failure
}
