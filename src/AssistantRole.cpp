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
#include <httplib.h> // Include httplib header

using json = nlohmann::json;
using namespace httplib;

AssistantRole::AssistantRole(std::shared_ptr<LLMProvider> llmProvider, const Config& config) 
    : llmProvider(llmProvider),
      config(config),
      toolManager(std::make_unique<ToolManager>(config))
{
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
        AssistantResponse llm_response = llmProvider->processChat(messages, toolManager->getToolsSpecification(), send_thought);

        if (llm_response.step_failed) {
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = llm_response.error_message,
                .recovery_options = {"retry"}
            };
        }

        json response_json = llm_response.llm_response;

        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            SPDLOG_ERROR("Ответ LLM не содержит 'choices'. Тело: {}", response_json.dump(2));
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
    auto final_llm_response = llmProvider->processChat(messages, json::array(), send_thought);

    if (!final_llm_response.step_failed && final_llm_response.llm_response.contains("choices") && !final_llm_response.llm_response["choices"].empty()) {
        std::string final_text = final_llm_response.llm_response["choices"][0]["message"]["content"];
        messages.push_back({{"role", "assistant"}, {"content", final_text}});
        return {
            .text = final_text, 
            .is_final = true, 
            .conversation_history = messages
        };
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

    json response_json = llmProvider->generatePlan(user_query);

    if (response_json.contains("error")) {
        SPDLOG_ERROR("Не удалось сгенерировать план: {}", response_json["error"].get<std::string>());
        return json::array({"Ошибка: " + response_json["error"].get<std::string>()});
    }
    
    std::string content = response_json["choices"][0]["message"]["content"];

    json plan_json = parsePlanFromMarkdown(content);
    if (plan_json.is_object() && plan_json.contains("plan") && plan_json["plan"].is_array()) {
        SPDLOG_INFO("План успешно сгенерирован и разобран.");
        return plan_json["plan"];
    }

    SPDLOG_ERROR("Не удалось извлечь корректный план из ответа LLM. Ответ: {}", content);
    return json::array({"Ошибка: модель вернула некорректный формат плана."});
}
