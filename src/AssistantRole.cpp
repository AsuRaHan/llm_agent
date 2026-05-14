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
    // Устанавливаем таймауты. Connection - на установку соединения, read - на ожидание ответа.
    cli.set_connection_timeout(10, 0); // 10 секунд на подключение
    cli.set_read_timeout(config.chat_completion_timeout_sec, 0); // Таймаут на генерацию ответа моделью
}

// Destructor must be defined in the .cpp file where ToolManager is a complete type
AssistantRole::~AssistantRole() = default;

std::string AssistantRole::processQuery(const std::string& userQuery, const std::vector<SearchResult>& initialContext, ContextIndexer& indexer) {
    // 1. Initialize message history
    json messages = json::array();

    std::string system_prompt = "Ты — эксперт-программист и AI-ассистент. Твоя задача — отвечать на вопросы пользователя о кодовой базе.\n"
                                "У тебя есть лимит на вызов инструментов: " + std::to_string(config.max_tool_calls) + " раз на один запрос.\n\n"
                                "Твой план действий:\n"
                                "1.  **Проанализируй контекст.** Тебе предоставлен релевантный контекст, найденный по вопросу пользователя. Внимательно изучи его.\n"
                                "2.  **Оцени достаточность контекста.** Если предоставленные фрагменты кода полностью отвечают на вопрос, сформируй ответ на их основе.\n"
                                "3.  **Используй инструменты, если необходимо.** Если контекст неполный, или тебе нужно увидеть файл целиком, чтобы понять общую картину, используй инструменты:\n"
                                "    *   `read_file`: чтобы прочитать весь файл, из которого взят фрагмент.\n"
                                "    *   `web_search`: чтобы найти информацию в интернете (документация, ошибки, общие вопросы).\n"
                                "    *   `read_url`: чтобы прочитать содержимое веб-страницы по URL (например, из результатов `web_search`).\n"
                                "    *   `github_repository_search`: для поиска репозиториев, кода, проблем, коммитов или пользователей на GitHub. Используй параметр 'type' для указания, что именно искать.\n"
                                "    *   `open_url_in_browser`: чтобы открыть URL в браузере пользователя.\n"
                                "    *   `list_directory`: чтобы изучить структуру проекта.\n"
                                "    *   `code_search`: чтобы выполнить **новый** семантический поиск по другому запросу.\n"
                                "    *   `grep_search`: для поиска по точному совпадению или регулярному выражению.\n"
                                "    *   `file_glob_search`: для поиска файлов по маске (например, '*.cpp').\n"
                                "    *   `write_file`: для записи или изменения файлов (используй с осторожностью!).\n"
                                "    *   `edit_file`: для безопасной, точечной замены блока кода в файле.\n"
                                "    *   `apply_diff`: для применения патча в формате unified diff к файлу.\n"
                                "    *   `execute_shell_command`: для выполнения команды в системной оболочке (shell/cmd).\n"
                                "    *   `get_datetime`: для получения текущей даты и времени в формате UTC.\n"
                                "    *   `get_system_info`: для получения информации об операционной системе, на которой запущен агент.\n"
                                "4.  **Думай по шагам.** После каждого шага (вызова инструмента) анализируй полученную информацию и решай, что делать дальше, пока не соберешь достаточно данных для исчерпывающего ответа.\n"
                                "5.  **Дай точный ответ.** В конце, ссылаясь на собранную информацию, дай пользователю точный и подробный ответ.";

    // System Prompt
    messages.push_back({
        {"role", "system"},
        {"content", system_prompt}
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
            {"model", config.chat_model_name},
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
            SPDLOG_ERROR("Попытка {} из {} для генерации ответа не удалась. Ошибка соединения: {}. Повтор...",
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
                const auto& function_call = call["function"];
                std::string tool_name = function_call["name"];
                json tool_args;
                std::string tool_id = call["id"];

                try {
                    // Handle cases where arguments are a stringified JSON or a direct JSON object
                    if (function_call["arguments"].is_string()) {
                        tool_args = json::parse(function_call["arguments"].get<std::string>());
                    } else {
                        tool_args = function_call["arguments"];
                    }
                } catch (const json::exception& e) {
                    SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
                    std::string error_content = "{\"error\": \"Invalid JSON in arguments: " + std::string(e.what()) + "\"}";
                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_id},
                        {"content", error_content}
                    });
                    continue; // Skip to the next tool call or next agent iteration
                }

                // Execute the tool
                std::string result = toolManager->executeTool(tool_name, tool_args, &indexer);

                // Add the tool's result to the history
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_id},
                    {"content", result}
                });
            }

            // Inform the model about remaining tool calls by appending to the last tool message
            int remaining_calls = config.max_tool_calls - (i + 1);
            if (remaining_calls > 0) {
                if (!messages.empty() && messages.back()["role"] == "tool") {
                    std::string current_content = messages.back()["content"];
                    messages.back()["content"] = current_content + "\n\n[SYSTEM_NOTE]: Инструменты выполнены. У тебя осталось " + std::to_string(remaining_calls) + " вызовов.";
                }
            }
            // Continue to the next iteration of the loop
            continue;
        }

        // Fallback if the response is unexpected (e.g. content is null)
        SPDLOG_ERROR("Неожиданный формат ответа от LLM: {}", choice.dump(2));
        return "Ошибка: получен неожиданный формат ответа от модели.";
    }

    SPDLOG_WARN("Превышено максимальное количество вызовов инструментов ({}). Запрос финального ответа от модели.", config.max_tool_calls);

    // Ask the model to summarize and give a final answer based on the conversation so far.
    messages.push_back({
        {"role", "system"},
        {"content", "Ты достиг максимального количества вызовов инструментов. Предоставь пользователю окончательный ответ, основываясь на уже собранной информации."}
    });

    json final_body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        // No "tools" or "tool_choice" here to force a direct answer
        {"temperature", 0.1}
    };

    // Re-create headers for the final call, as the previous one was in the loop's scope.
    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    // Make one last call to the LLM
    auto final_res = cli.Post("/v1/chat/completions", headers, final_body.dump(), "application/json");

    if (final_res && final_res->status == 200) {
        try {
            json final_response_json = json::parse(final_res->body);
            if (final_response_json.contains("choices") && !final_response_json["choices"].empty()) {
                return final_response_json["choices"][0]["message"]["content"].get<std::string>();
            }
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать финальный JSON-ответ от LLM: {}", e.what());
        }
    }

    // Fallback error message if the final call also fails.
    return "Ошибка: превышено максимальное количество вызовов инструментов (" + std::to_string(config.max_tool_calls) + "), и не удалось сгенерировать итоговый ответ.";
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
        {"model", config.chat_model_name},
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
        {"model", config.chat_model_name},
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
