#include "AssistantRole.h"
#include "Logger.h"
#include <thread>
#include <chrono>

#include "Config.h"

using json = nlohmann::json;

AssistantRole::AssistantRole(const Config& config) : config(config), cli(config.server_host, config.server_port) {
    cli.set_connection_timeout(config.chat_completion_timeout_sec, 0);
}

std::string AssistantRole::analyzeCode(const std::string& filePath, const std::string& fileContent, const std::string& userQuery) {
    SPDLOG_INFO("Analyzing '{}'...", filePath);

    // Form the prompt for the Llama model
    std::string prompt_text = 
        "Ты эксперт-программист. Вот контекст из файла '" + filePath + "':\n```\n" + 
        fileContent + 
        "\n```\n\nВопрос пользователя: " + userQuery + 
        "\n\nДай краткий и точный ответ.";

    json body = {
        {"messages", json::array({
            { {"role", "system"}, {"content", "Вы — полезный помощник для программистов."} },
            { {"role", "user"}, {"content", prompt_text} }
        })},
        {"temperature", 0.3}
    };

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/chat/completions", body.dump(), "application/json");
        if (res) { // If we got any response (even an error status), we can break.
            break;
        }
        // If res is false, it's a connection error.
        SPDLOG_WARN("[AssistantRole] Попытка {}/{} для анализа '{}' не удалась. Ошибка соединения: {}. Повтор через {} мс...",
                    attempt, config.retry_count, filePath, httplib::to_string(res.error()), config.retry_delay_ms);
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
        "прояви немного индивидуальности. Говори от первого лица.";

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
        res = cli.Post("/v1/chat/completions", body.dump(), "application/json");
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
