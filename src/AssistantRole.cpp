#include "AssistantRole.h"
#include "Logger.h"
#include "ContextIndexer.h" // For SearchResult definition
#include "tools/ToolManager.h" // Include the new ToolManager
#include <thread>
#include <chrono>
#include <sstream>
#include "Config.h"
using json = nlohmann::json;

AssistantRole::AssistantRole(const Config& config) 
    : config(config), 
      cli(config.server_host, config.server_port),
      toolManager(std::make_unique<ToolManager>(config))
{
    cli.set_connection_timeout(config.chat_completion_timeout_sec, 0);
}

// Destructor must be defined in the .cpp file where ToolManager is a complete type
AssistantRole::~AssistantRole() = default;

std::string AssistantRole::processQuery(const std::string& userQuery, const std::vector<SearchResult>& initialContext, ContextIndexer& indexer) {
    // 1. Initialize message history
    json messages = json::array();

    // System Prompt
    messages.push_back({
        {"role", "system"},
        {"content", "Ты — эксперт-программист и AI-ассистент. Твоя задача — отвечать на вопросы пользователя о кодовой базе.\n"
                    "Твой план действий:\n"
                    "1.  **Проанализируй контекст.** Тебе предоставлен релевантный контекст, найденный по вопросу пользователя. Внимательно изучи его.\n"
                    "2.  **Оцени достаточность контекста.** Если предоставленные фрагменты кода полностью отвечают на вопрос, сформируй ответ на их основе.\n"
                    "3.  **Используй инструменты, если необходимо.** Если контекст неполный, или тебе нужно увидеть файл целиком, чтобы понять общую картину, используй инструменты:\n"
                    "    *   `read_file`: чтобы прочитать весь файл, из которого взят фрагмент.\n"
                    "    *   `list_directory`: чтобы изучить структуру проекта.\n"
                    "    *   `code_search`: чтобы выполнить **новый** семантический поиск по другому запросу.\n"
                    "    *   `grep_search`: для поиска по точному совпадению или регулярному выражению.\n"
                    "    *   `file_glob_search`: для поиска файлов по маске (например, '*.cpp').\n"
                    "    *   `write_file`: для записи или изменения файлов (используй с осторожностью!).\n"
                    "    *   `edit_file`: для безопасной, точечной замены блока кода в файле.\n"
                    "4.  **Думай по шагам.** После каждого шага (вызова инструмента) анализируй полученную информацию и решай, что делать дальше, пока не соберешь достаточно данных для исчерпывающего ответа.\n"
                    "5.  **Дай точный ответ.** В конце, ссылаясь на собранную информацию, дай пользователю точный и подробный ответ."}
    });

    // RAG Context
    if (!initialContext.empty()) {
        std::stringstream context_ss;
        context_ss << "Вот релевантный контекст из кодовой базы, который может помочь с ответом:\n\n";
        for (const auto& result : initialContext) {
            context_ss << "--- ИЗ ФАЙЛА: " << result.filePath << " ---\n"
                       << "```\n"
                       << result.chunkText << "\n"
                       << "```\n\n";
        }
        messages.push_back({
            {"role", "user"},
            {"content", context_ss.str()}
        });
    }

    // Initial User Query
    messages.push_back({
        {"role", "user"},
        {"content", userQuery}
    });

    for (int i = 0; i < config.max_tool_calls; ++i) {
        SPDLOG_INFO("Итерация {} цикла обработки запроса...", i + 1);

        // 2. Prepare request for LLM
        json body = {
            {"messages", messages},
            {"tools", toolManager->getToolsSpecification()},
            {"tool_choice", "auto"},
            {"temperature", 0.1}
        };

        httplib::Headers headers;
        if (!config.api_key.empty()) {
            headers.emplace("Authorization", "Bearer " + config.api_key);
        }
        std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

        // 3. Call LLM
        httplib::Result res;
        for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
            res = cli.Post("/v1/chat/completions", headers, body_str, "application/json");
            if (res) break;
            SPDLOG_WARN("[AssistantRole] Попытка {}/{} для генерации ответа не удалась. Ошибка соединения: {}. Повтор...",
                        attempt, config.retry_count, httplib::to_string(res.error()));
            std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
        }

        if (!res || res->status != 200) {
            if (res) {
                SPDLOG_ERROR("Ошибка от LLM. Статус: {}. Тело: {}", res->status, res->body);
            } else {
                SPDLOG_ERROR("Ошибка соединения с LLM: {}", httplib::to_string(res.error()));
            }
            return "Ошибка: не удалось получить ответ от языковой модели.";
        }

        // 4. Process LLM response
        json response_json;
        try {
            response_json = json::parse(res->body);
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать JSON-ответ от LLM: {}", e.what());
            return "Ошибка: не удалось обработать ответ модели.";
        }

        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            SPDLOG_ERROR("Ответ LLM не содержит 'choices'.");
            return "Ошибка: получен некорректный ответ от модели.";
        }

        const auto& choice = response_json["choices"][0];
        const auto& message = choice["message"];

        // Case 1: LLM provides a direct answer
        if (message.contains("content") && message["content"].is_string() && !message["content"].get<std::string>().empty()) {
            SPDLOG_INFO("LLM предоставил прямой ответ. Завершение цикла.");
            return message["content"].get<std::string>();
        }

        // Case 2: LLM wants to call tools
        if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
            SPDLOG_INFO("LLM запросил вызов инструментов.");
            // Add the assistant's response (with tool calls) to the history
            messages.push_back(message);

            const auto& tool_calls = message["tool_calls"];
            for (const auto& call : tool_calls) {
                std::string tool_name = call["function"]["name"];
                json tool_args;
                // Handle cases where arguments are a stringified JSON or a direct JSON object
                if (call["function"]["arguments"].is_string()) {
                    tool_args = json::parse(call["function"]["arguments"].get<std::string>());
                } else {
                    tool_args = call["function"]["arguments"];
                }
                std::string tool_id = call["id"];

                // Execute the tool
                std::string result = toolManager->executeTool(tool_name, tool_args, &indexer);

                // Add the tool's result to the history
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_id},
                    {"content", result}
                });
            }
            // Continue to the next iteration of the loop
            continue;
        }

        // Fallback if the response is unexpected (e.g. content is null)
        SPDLOG_ERROR("Неожиданный формат ответа от LLM: {}", choice.dump(2));
        return "Ошибка: получен неожиданный формат ответа от модели.";
    }

    return "Ошибка: превышено максимальное количество вызовов инструментов (" + std::to_string(config.max_tool_calls) + ").";
}

std::string AssistantRole::generateProjectSummaryGreeting(int file_count, int embedding_count) {
    SPDLOG_INFO("Генерация приветственного сообщения о проекте...");

    std::string prompt_text = 
        "Ты — остроумный и дружелюбный ИИ-ассистент для программиста. Тебя называют «Smart Hammer». Ты только что закончил "
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

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    httplib::Result res;
    // Using a simplified retry logic for this non-critical call
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/chat/completions", headers, body_str, "application/json");
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

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/chat/completions", headers, body_str, "application/json");
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
