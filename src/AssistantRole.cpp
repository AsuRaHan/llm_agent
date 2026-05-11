#include "AssistantRole.h"
#include <iostream>

using json = nlohmann::json;

AssistantRole::AssistantRole() : cli("localhost", 8080) {
    cli.set_connection_timeout(300, 0); // 5 minutes timeout for chat completion
}

std::string AssistantRole::analyzeCode(const std::string& filePath, const std::string& fileContent, const std::string& userQuery) {
    std::cout << "\n[Assistant] Analyzing '" << filePath << "'..." << std::endl;

    // Form the prompt for the Llama model
    std::string prompt_text = 
        "Ты эксперт-программист. Вот контекст из файла '" + filePath + "':\n```\n" + 
        fileContent + 
        "\n```\n\nВопрос пользователя: " + userQuery + 
        "\n\nДай краткий и точный ответ.";

    json body = {
        {"messages", json::array({
            {{"role", "system"}, {"content", "You are a helpful programming assistant."}},
            {{"role", "user"}, {"content", prompt_text}}
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
            std::cerr << "[Assistant] Error: Failed to parse JSON response. Details: " << e.what() << std::endl;
            return "Error processing model response.";
        }
    } else {
        std::cerr << "[Assistant] Error: Failed to get analysis. Status: "
                  << (res ? res->status : -1) << std::endl;
        if(res) {
            std::cerr << "SERVER RESPONSE: " << res->body << std::endl;
        }
        return "Failed to communicate with the model.";
    }

    return "Received an unexpected response from the model.";
}

