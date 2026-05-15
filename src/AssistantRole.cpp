#include "AssistantRole.h"
#include "UserSession.h"
#include "Logger.h"
#include "Config.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iterator>

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

AssistantResponse AssistantRole::processQuery(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    ContextIndexer& indexer,
    const nlohmann::json& continuation_history,
    const std::function<void(const std::string&)>& send_thought
) {
    // 1. Initialize message history
    json messages = json::array();
    bool is_continuation_after_confirmation = !continuation_history.empty() && userQuery.empty();

    if (is_continuation_after_confirmation) {
        // This is a continuation call after a dangerous tool was confirmed.
        // The tool call is in the history. We need to execute it before calling the LLM again.
        messages = continuation_history;
        const auto& last_message = messages.back();

        if (last_message.value("role", "") == "assistant" && last_message.contains("tool_calls")) {
            SPDLOG_INFO("Продолжение после подтверждения. Выполнение отложенного инструмента...");
            const auto& tool_calls = last_message["tool_calls"];

            for (const auto& call : tool_calls) {
                const auto& function_call = call["function"];
                std::string tool_name = function_call["name"];
                json tool_args;
                const std::string& tool_id = call["id"];

                try {
                    // Handle cases where arguments are a stringified JSON or a direct JSON object
                    if (function_call["arguments"].is_string()) {
                        tool_args = json::parse(function_call["arguments"].get<std::string>());
                    } else {
                        tool_args = function_call["arguments"];
                    }
        } catch (const json::exception& e) { // Ошибка парсинга аргументов инструмента
                    SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
                    std::string error_content = "{\"error\": \"Invalid JSON in arguments: " + std::string(e.what()) + "\"}";
                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_id},
                        {"content", error_content}
                    });
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = "Не удалось разобрать аргументы для инструмента '" + tool_name + "': " + e.what(),
                .recovery_options = {"retry", "skip"}
            };
                }

                // Execute the tool
                if (send_thought) {
                    send_thought("Выполняю подтвержденный инструмент: " + tool_name + "...");
                }
                std::string result = toolManager->executeTool(tool_name, tool_args, &indexer);

        // Проверяем, вернул ли инструмент ошибку в формате {"error": "..."}
        try {
            json result_json = json::parse(result);
            if (result_json.contains("error")) {
                SPDLOG_ERROR("Инструмент '{}' завершился с ошибкой: {}", tool_name, result_json["error"].get<std::string>());
                messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", result}});
                return {
                    .text = "",
                    .is_final = true,
                    .conversation_history = messages,
                    .step_failed = true,
                    .error_message = "Инструмент '" + tool_name + "' завершился с ошибкой: " + result_json["error"].get<std::string>(),
                    .recovery_options = {"retry", "skip"}
                };
            }
        } catch (const json::exception& e) {
            // Если результат не является JSON или не содержит "error", это не ошибка инструмента в данном формате.
            // Продолжаем обработку как обычный результат.
        }

                // Add the tool's result to the history
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_id},
                    {"content", result}
                });
            }
        }
        // If it's a continuation, the user query and context are already in the history.
        // We don't add them again.
    } else {
        // This is a new query or a simple follow-up.
        json messages_for_llm = json::array();

        // 1. Find the original system prompt content, if it exists.
        std::string system_prompt_content;
        auto system_msg_it = std::find_if(continuation_history.begin(), continuation_history.end(), [](const json& msg){
            return msg.value("role", "") == "system";
        });

        if (system_msg_it != continuation_history.end()) {
            // Use existing system prompt
            system_prompt_content = system_msg_it->value("content", "");
        } else {
            // Create a new one if it's the start of a conversation
            system_prompt_content = "Ты — эксперт-программист и AI-ассистент. Твоя задача — отвечать на вопросы пользователя о кодовой базе.\n"
                                    "У тебя есть лимит на вызов инструментов: " + std::to_string(config.max_tool_calls) + " раз на один запрос.\n\n"
                                    "Твой план действий:\n"
                                    "1.  **Проанализируй контекст.** Тебе предоставлен релевантный контекст, найденный по вопросу пользователя. Внимательно изучи его.\n"
                                    "2.  **Оцени достаточность контекста.** Если предоставленные фрагменты кода полностью отвечают на вопрос, сформируй ответ на их основе.\n"
                                    "3.  **Используй инструменты, если необходимо.** Если контекст неполный, или тебе нужно увидеть файл целиком, чтобы понять общую картину, используй инструменты:\n"
                                    "4.  **Думай по шагам.** После каждого шага (вызова инструмента) анализируй полученную информацию и решай, что делать дальше, пока не соберешь достаточно данных для исчерпывающего ответа.\n"
                                    "5.  **Дай точный ответ.** В конце, ссылаясь на собранную информацию, дай пользователю точный и подробный ответ.";
        }

        // 2. Append the new RAG context to the system prompt content
        if (!initialContext.empty()) {
            std::stringstream context_ss;
            context_ss << "\n\n---\n# Дополнительный релевантный контекст для текущего запроса:\n";
            for (const auto& result : initialContext) {
                context_ss << "--- ИЗ ФАЙЛА: " << result.filePath << " ---\n"
                           << "```\n"
                           << result.chunkText << "\n"
                           << "```\n\n";
            }
            system_prompt_content += context_ss.str();
        }

        // 3. Build the final message list for the LLM
        messages_for_llm.push_back({{"role", "system"}, {"content", system_prompt_content}});
        for (const auto& msg : continuation_history) {
            if (msg.value("role", "") != "system") {
                messages_for_llm.push_back(msg);
            }
        }
        messages = messages_for_llm;
    }

    // Count tool calls already in history to respect the limit across continuations
    int tool_calls_made = 0;
    for(const auto& msg : messages) {
        if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
            tool_calls_made += msg["tool_calls"].size();
        }
    }

    for (int i = tool_calls_made; i < config.max_tool_calls; ++i) {
        SPDLOG_INFO("Итерация {}/{} цикла обработки запроса...", i + 1, config.max_tool_calls);
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
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = "Не удалось получить ответ от языковой модели (ошибка соединения или таймаут).",
                .recovery_options = {"retry"}
            };
        }

        // 4. Process LLM response
        json response_json;
        try {
            response_json = json::parse(res->body);
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать JSON-ответ от LLM: {}. Тело: {}", e.what(), res->body);
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = "Не удалось обработать ответ модели (некорректный JSON): " + std::string(e.what()),
                .recovery_options = {"retry"}
            };
        }

        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            SPDLOG_ERROR("Ответ LLM не содержит 'choices'. Тело: {}", res->body);
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = "Получен некорректный ответ от модели (отсутствует поле 'choices').",
                .recovery_options = {"retry"}
            };
        }

        const auto& choice = response_json["choices"][0];
        const auto& message = choice["message"];

        // Case 1: LLM provides a direct answer
        if (message.contains("content") && message["content"].is_string() && !message["content"].get<std::string>().empty()) {
            SPDLOG_INFO("LLM предоставил прямой ответ. Завершение цикла.");
            std::string final_text = message["content"];

            // Add the final assistant message to the history before returning
            messages.push_back({
                {"role", "assistant"},
                {"content", final_text}});

            // Simple heuristic to detect a follow-up question
            bool is_final = true;
            if (!final_text.empty() && final_text.back() == '?') {
                is_final = false;
            }
            
            return {
                .text = final_text, 
                .is_final = is_final, 
                .conversation_history = messages
            };
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
                const std::string& tool_id = call["id"];

                // --- DANGEROUS TOOL CHECK ---
                const auto& dangerous_tools = config.dangerous_tools;
                if (std::find(dangerous_tools.begin(), dangerous_tools.end(), tool_name) != dangerous_tools.end()) {
                    SPDLOG_WARN("Обнаружен опасный инструмент '{}', требующий подтверждения.", tool_name);

                    std::string prompt = "Я собираюсь использовать инструмент '" + tool_name + "'. Разрешить выполнение?";
                    
                    return {
                        .text = prompt,
                        .is_final = false,
                        .conversation_history = messages,
                        .requires_confirmation = true,
                        .pending_tool_call = call,
                        .plan_completed = true
                    };
                }

                try {
                    // Handle cases where arguments are a stringified JSON or a direct JSON object
                    if (function_call["arguments"].is_string()) {
                        tool_args = json::parse(function_call["arguments"].get<std::string>());
                    } else {
                        tool_args = function_call["arguments"];
                    }
                } catch (const json::exception& e) { // Ошибка парсинга аргументов инструмента
                    SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
                    std::string error_content = "{\"error\": \"Invalid JSON in arguments: " + std::string(e.what()) + "\"}";
                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_id},
                        {"content", error_content}
                    });
                    return {
                        .text = "",
                        .is_final = true,
                        .conversation_history = messages,
                        .step_failed = true,
                        .error_message = "Не удалось разобрать аргументы для инструмента '" + tool_name + "': " + e.what(),
                        .recovery_options = {"retry", "skip"}
                    };
                }

                // Execute the tool
                if (send_thought) {
                    send_thought("Выполняю инструмент: " + tool_name + "...");
                }
                std::string result = toolManager->executeTool(tool_name, tool_args, &indexer);

                // Проверяем, вернул ли инструмент ошибку в формате {"error": "..."}
                try {
                    json result_json = json::parse(result);
                    if (result_json.contains("error")) {
                        SPDLOG_ERROR("Инструмент '{}' завершился с ошибкой: {}", tool_name, result_json["error"].get<std::string>());
                        messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", result}});
                        return {
                            .text = "",
                            .is_final = true,
                            .conversation_history = messages,
                            .step_failed = true,
                            .error_message = "Инструмент '" + tool_name + "' завершился с ошибкой: " + result_json["error"].get<std::string>(),
                            .recovery_options = {"retry", "skip"}
                        };
                    }
                } catch (const json::exception& e) {
                    // Если результат не является JSON или не содержит "error", это не ошибка инструмента в данном формате.
                    // Продолжаем обработку как обычный результат.
                }

                // Add the tool's result to the history
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_id},
                    {"content", result}
                });
            }

            // Inform the model about remaining tool calls by appending to the last tool message
            int remaining_calls = config.max_tool_calls - (i + 1); // i is 0-indexed
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
        return {
            .text = "",
            .is_final = true,
            .conversation_history = messages,
            .step_failed = true,
            .error_message = "Получен неожиданный формат ответа от модели (не текст и не вызов инструмента).",
            .recovery_options = {"retry"}
        };
    }

    SPDLOG_WARN("Превышено максимальное количество вызовов инструментов ({}). Запрос финального ответа.", config.max_tool_calls);
    
    // Ask the model to summarize and give a final answer based on the conversation so far.
    messages.push_back({
        {"role", "user"}, // Use 'user' to force a response
        {"content", "Ты достиг максимального количества вызовов инструментов. Предоставь пользователю окончательный ответ, основываясь на уже собранной информации."}
    });

    json final_body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        // No "tools" or "tool_choice" here to force a direct answer
        {"temperature", 0.1}
    };

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    // Make one last call to the LLM
    auto final_res = cli.Post("/v1/chat/completions", headers, final_body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), "application/json");

    if (final_res && final_res->status == 200) {
        try {
            json final_response_json = json::parse(final_res->body);
            if (final_response_json.contains("choices") && !final_response_json["choices"].empty()) {
                std::string final_text = final_response_json["choices"][0]["message"]["content"];
                messages.push_back({{"role", "assistant"}, {"content", final_text}});
                return {
                    .text = final_text, 
                    .is_final = true, 
                    .conversation_history = messages
                };
            }
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать финальный JSON-ответ от LLM: {}", e.what());
        }
    }

    return {
        .text = "",
        .is_final = true,
        .conversation_history = messages,
        .step_failed = true,
        .error_message = "Превышен лимит вызовов инструментов (" + std::to_string(config.max_tool_calls) + ").",
        .recovery_options = {"re-plan"}
    };
}

std::string AssistantRole::generateProjectSummaryGreeting(int file_count, int embedding_count) {
    SPDLOG_INFO("Генерация приветственного сообщения о проекте...");

    // std::string prompt_text = 
    //     "Ты — остроумный и дружелюбный ИИ-ассистент для программиста. Тебя называют «Smart Hammer». Ты только что закончил "
    //     "сканирование его проекта. Ты проиндексировал " + std::to_string(file_count) + 
    //     " файлов и создал " + std::to_string(embedding_count) + 
    //     " смысловых 'воспоминаний' (эмбеддингов). Твоя задача — сгенерировать короткое, "
    //     "креативное и ободряющее приветственное сообщение для разработчика. Дай ему понять, "
    //     "что ты в сети и готов помочь разобраться в коде. Не просто констатируй факты, "
    //     "прояви немного индивидуальности. Говори от первого лица.";

    // json body = {
    //     {"messages", json::array({
    //         { {"role", "system"}, {"content", "Ты — полезный ИИ-ассистент."} },
    //         { {"role", "user"}, {"content", prompt_text} }
    //     })},
    //     {"model", config.chat_model_name},
    //     {"temperature", 0.7} // A bit more creative
    // };

    // httplib::Headers headers;
    // if (!config.api_key.empty()) {
    //     headers.emplace("Authorization", "Bearer " + config.api_key);
    // }
    // std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    // httplib::Result res;
    // // Using a simplified retry logic for this non-critical call
    // for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
    //     res = cli.Post("/v1/chat/completions", headers, body_str, "application/json");
    //     if (res) break;
    //     std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    // }

    // if (res && res->status == 200) {
    //     try {
    //         auto json_body = json::parse(res->body);
    //         if (json_body.contains("choices") && !json_body["choices"].empty()) {
    //             return json_body["choices"][0]["message"]["content"].get<std::string>();
    //         }
    //     } catch (const json::exception& e) {
    //         SPDLOG_ERROR("Не удалось разобрать ответ для приветствия: {}", e.what());
    //     }
    // }
    
    // Fallback message if AI fails
    return "Привет! Я просканировал твой проект. Готов отвечать на вопросы.";
}

std::string AssistantRole::generateChunkSummary(const std::string& codeChunk, const std::string& chunkName) {
    SPDLOG_DEBUG("Запрос на генерацию саммари для чанка '{}'...", chunkName);

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
        SPDLOG_WARN("Попытка {}/{} для саммари '{}' не удалась. Ошибка соединения: {}. Повтор...",
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
                SPDLOG_DEBUG("Саммари для '{}': {}", chunkName, summary);
                return summary;
            }
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать ответ для саммари '{}': {}", chunkName, e.what());
        }
    }

    SPDLOG_WARN("Не удалось сгенерировать саммари для '{}'. Будет использован пустой текст.", chunkName);
    return ""; // Return empty string on failure
}

nlohmann::json AssistantRole::parsePlanFromMarkdown(const std::string& text) {
    // Regex to find a JSON object within markdown code fences (```json ... ``` or ``` ... ```)
    std::regex re("```(?:json)?\\s*(\\{[\\s\\S]*\\})\\s*```");
    std::smatch match;

    if (std::regex_search(text, match, re) && match.size() > 1) {
        try {
            // The captured JSON string is in match[1]
            return json::parse(match[1].str());
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Не удалось разобрать JSON из markdown-блока: {}", e.what());
        }
    }
    
    // Fallback for raw JSON that might have been returned with surrounding text, but not in a code block
    size_t first_brace = text.find('{');
    size_t last_brace = text.rfind('}');
    if (first_brace != std::string::npos && last_brace != std::string::npos && last_brace > first_brace) {
        try {
            return json::parse(text.substr(first_brace, last_brace - first_brace + 1));
        } catch (const json::exception& e) {
            // Ignore if this also fails, we'll return null
        }
    }

    return nullptr; // Return null json object on failure
}

nlohmann::json AssistantRole::generatePlan(const std::string& user_query) {
    SPDLOG_INFO("Генерация плана для запроса: '{}'", user_query);

    std::string system_prompt_text =
        "Ты — AI-архитектор Smart Hammer. Твоя задача — разбить задачу пользователя на "
        "минимальные атомарные шаги. Каждый шаг должен выполняться за один-два вызова инструментов.\n"
        "Выдай ответ СТРОГО в формате JSON-объекта с ключом 'plan', который содержит массив строк. "
        "Не добавляй никакого другого текста или markdown-разметки.\n"
        "Пример: {\"plan\": [\"Изучить структуру файла CodeParser.cpp\", \"Найти утечки памяти\", \"Применить diff-патч\"]}";

    json messages = {
        {{"role", "system"}, {"content", system_prompt_text}},
        {{"role", "user"}, {"content", "Задача: " + user_query}}
    };

    json body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        {"temperature", 0.0}, // We want a deterministic plan
        // Request JSON output format from the model
        {"response_format", { {"type", "json_object"} }}
    };

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump(-1, ' ', false, json::error_handler_t::replace);

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/chat/completions", headers, body_str, "application/json");
        if (res) break;
        SPDLOG_ERROR("Попытка {} из {} для генерации плана не удалась. Ошибка соединения: {}. Повтор...",
                    attempt, config.retry_count, httplib::to_string(res.error()));
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (!res || res->status != 200) {
        if (res) {
            SPDLOG_ERROR("Ошибка от LLM при генерации плана. Статус: {}. Тело: {}", res->status, res->body);
        } else {
            SPDLOG_ERROR("Ошибка соединения с LLM при генерации плана: {}", httplib::to_string(res.error()));
        }
        return json::array({"Ошибка: не удалось сгенерировать план."});
    }

    std::string content;
    try {
        json response_json = json::parse(res->body);
        content = response_json["choices"][0]["message"]["content"];
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Не удалось разобрать основной JSON-ответ от LLM: {}", e.what());
        return json::array({"Ошибка: модель вернула некорректный JSON."});
    }

    json plan_json = parsePlanFromMarkdown(content);
    if (plan_json.is_object() && plan_json.contains("plan") && plan_json["plan"].is_array()) {
        SPDLOG_INFO("План успешно сгенерирован и разобран.");
        return plan_json["plan"];
    }

    SPDLOG_ERROR("Не удалось извлечь корректный план из ответа LLM. Ответ: {}", content);
    return json::array({"Ошибка: модель вернула некорректный формат плана."});
}
