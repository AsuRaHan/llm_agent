#include "AssistantRole.h"
#include "Logger.h"

using json = nlohmann::json;

AssistantRole::AssistantRole() : cli("localhost", 8080) {
    cli.set_connection_timeout(300, 0); // 5 minutes timeout for chat completion
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

    auto res = cli.Post("/v1/chat/completions", body.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            if (json_body.contains("choices") && json_body["choices"].is_array() && !json_body["choices"].empty()) {
                const auto& first_choice = json_body["choices"][0];
                if (first_choice.contains("message") && first_choice["message"].contains("content")) {
                    return first_choice["message"]["content"].get<std::string>();
                }
            }
        } catch (const json::parse_error& e) {
            SPDLOG_ERROR("Failed to parse JSON response. Details: {}", e.what());
            return "Ошибка обработки ответа от модели.";
        }
    } else {
        SPDLOG_ERROR("Failed to get analysis. Status: {}", (res ? res->status : -1));
        if(res) {
            SPDLOG_ERROR("Server response: {}", res->body);
        }
        return "Не удалось связаться с моделью.";
    }

    return "Получен неожиданный ответ от модели.";
}
