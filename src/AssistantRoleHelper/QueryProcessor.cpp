#include "AssistantRoleHelper/QueryProcessor.h"
#include "AssistantRoleHelper/MessageBuilder.h"
#include "Logger.h"
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

QueryProcessor::QueryProcessor(
    std::shared_ptr<LLMProvider> llmProvider,
    ToolManager& toolManager,
    const Config& config,
    ContextIndexer& indexer,
    const std::function<void(const std::string&)>& send_thought,
    const std::function<void(const std::string&)>& send_stream_chunk,
    std::atomic<bool>& is_interrupted
) : m_llmProvider(llmProvider),
    m_toolManager(toolManager),
    m_config(config),
    m_indexer(indexer),
    m_send_thought(send_thought),
    m_send_stream_chunk(send_stream_chunk),
    m_is_interrupted(is_interrupted)
{}

AssistantResponse QueryProcessor::process(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    const nlohmann::json& continuation_history
) {
    MessageBuilder messageBuilder(continuation_history, m_config);
    bool is_continuation = !continuation_history.empty() && userQuery.empty();

    if (is_continuation) {
        messageBuilder.initialize(initialContext);
        auto response = handleContinuationAfterConfirmation(messageBuilder);
        // If the tool execution failed, return immediately.
        if (response.step_failed) {
            response.conversation_history = messageBuilder.getMessages();
            return response;
        }
    }

    // ReAct Loop
    for (int i = 0; i < m_config.max_tool_calls; ++i) {
        if (i == 0 && !is_continuation) {
            messageBuilder.initialize(initialContext);
        }
        
        auto iteration_response = runReActIteration(messageBuilder, i);

        if (iteration_response.should_break) {
            break;
        }

        // If the iteration produced a final response (e.g., direct answer, error, confirmation needed), return it.
        if (iteration_response.is_final || iteration_response.requires_confirmation || iteration_response.step_failed) {
            return iteration_response;
        }

        // Otherwise, continue the loop.
    }

    return forceFinalAnswer(messageBuilder);
}

AssistantResponse QueryProcessor::runReActIteration(MessageBuilder& messageBuilder, int iteration) {
    // Check for interruption at the beginning of each iteration
    if (m_is_interrupted.load()) {
        SPDLOG_INFO("Agent interruption detected. Stopping processing.");
        return { .text = "Задача прервана пользователем.", .is_final = true, .conversation_history = messageBuilder.getMessages() };
    }

    SPDLOG_INFO("Итерация {}/{} цикла обработки запроса...", iteration + 1, m_config.max_tool_calls);
    
    AssistantResponse llm_response = m_llmProvider->processChat(messageBuilder.getMessages(), m_toolManager.getToolsSpecification(), m_send_thought, m_send_stream_chunk);

    // Check for interruption after the potentially long LLM call
    if (m_is_interrupted.load()) {
        SPDLOG_INFO("Agent interruption detected after LLM call. Stopping.");
        return { .text = "Задача прервана пользователем.", .is_final = true, .conversation_history = messageBuilder.getMessages() };
    }

    if (llm_response.step_failed) {
        return { .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), .step_failed = true, .error_message = llm_response.error_message, .recovery_options = {"retry"} };
    }

    json response_json = llm_response.llm_response;
    if (!response_json.contains("choices") || response_json["choices"].empty()) {
        SPDLOG_ERROR("Ответ LLM не содержит 'choices'. Тело: {}", response_json.dump(2));
        return { .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), .step_failed = true, .error_message = "Получен некорректный ответ от модели (отсутствует поле 'choices').", .recovery_options = {"retry"} };
    }

    const auto& message = response_json["choices"][0]["message"];

    // Case 2: Tool calls
    if (message.contains("tool_calls") && message["tool_calls"].is_array() && !message["tool_calls"].empty()) {
        SPDLOG_INFO("LLM запросил вызов инструментов.");
        messageBuilder.addToolCallMessage(message); // Add assistant's turn with tool_calls to history
        
        auto tool_response = handleToolCalls(message, messageBuilder);
        // If a tool requires confirmation or failed, stop and return that response.
        if (tool_response.requires_confirmation || tool_response.step_failed) {
            return tool_response;
        }
        // All tools executed, continue the loop.
        int remaining_iterations = m_config.max_tool_calls - (iteration + 1);
        messageBuilder.setRemainingIterationsSystemNote(remaining_iterations);
        return {.is_final = false}; // Signal to continue
    }
    // Case 1: Direct answer
    else if (message.contains("content") && message["content"].is_string()) {
        SPDLOG_INFO("LLM предоставил прямой ответ (потоково). Завершение цикла.");
        messageBuilder.addAssistantMessage(message["content"]);
        return {.text = "", .is_final = true, .conversation_history = messageBuilder.getMessages()};
    }
    // Fallback: Empty response, break loop to force final answer.
    else {
        SPDLOG_INFO("LLM вернул пустой ответ, завершаем цикл и переходим к подведению итогов.");
        return {.is_final = false, .should_break = true}; // Signal to break
    }
}

AssistantResponse QueryProcessor::handleContinuationAfterConfirmation(MessageBuilder& messageBuilder) {
    const auto& last_message = messageBuilder.getMessages().back();

    if (last_message.value("role", "") == "assistant" && last_message.contains("tool_calls")) {
        SPDLOG_INFO("Продолжение после подтверждения. Выполнение отложенного инструмента...");
        // Pass skip_danger_check = true because the user has already confirmed the action.
        return handleToolCalls(last_message, messageBuilder, true);
    }
    // Should not happen, but as a fallback, indicate success to continue to ReAct loop
    return {.is_final = false};
}

AssistantResponse QueryProcessor::processAdvanced(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    const nlohmann::json& continuation_history
) {
    SPDLOG_DEBUG("processAdvanced: начало обработки. userQuery: '{}'", userQuery);
    MessageBuilder messageBuilder(continuation_history, m_config);
    bool is_continuation = !continuation_history.empty() && userQuery.empty();

    if (!is_continuation) {
        // This is a new query from the user.
        messageBuilder.initialize(initialContext);
    } else {
        // This is a continuation. The history is already prepared by the caller.
        // We just need to load it into the builder.
        messageBuilder.initialize({}); 
    }

    // Main agent loop
    for (int i = 0; i < m_config.max_tool_calls; ++i) {
        SPDLOG_INFO("Итерация {}/{} цикла обработки запроса...", i + 1, m_config.max_tool_calls);

        if (m_is_interrupted.load()) {
            SPDLOG_INFO("Agent interruption detected. Stopping processing.");
            return { .text = "Задача прервана пользователем.", .is_final = true, .conversation_history = messageBuilder.getMessages() };
        }

        // If this is a continuation from a user confirmation, execute the pending tool call directly.
        if (i == 0 && is_continuation) {
            const auto& last_message = messageBuilder.getMessages().back();
            if (last_message.value("role", "") == "assistant" && last_message.contains("tool_calls")) {
                SPDLOG_INFO("Продолжение после подтверждения. Выполнение отложенного инструмента...");
                // User has already confirmed, so skip danger check.
                auto tool_response = handleToolCalls(last_message, messageBuilder, true);
                if (tool_response.requires_confirmation || tool_response.step_failed) {
                    return tool_response;
                }
                // After handling, we continue to the next LLM call within this loop.
            }
        }

        // Call LLM for the next step
        AssistantResponse llm_response = m_llmProvider->processChat(messageBuilder.getMessages(), m_toolManager.getToolsSpecification(), m_send_thought, m_send_stream_chunk);

        if (m_is_interrupted.load()) {
            SPDLOG_INFO("Agent interruption detected after LLM call. Stopping.");
            return { .text = "Задача прервана пользователем.", .is_final = true, .conversation_history = messageBuilder.getMessages() };
        }

        if (llm_response.step_failed) {
            llm_response.conversation_history = messageBuilder.getMessages();
            llm_response.recovery_options = {"retry"};
            return llm_response;
        }

        json response_json = llm_response.llm_response;
        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            SPDLOG_ERROR("Ответ LLM не содержит 'choices'. Тело: {}", response_json.dump(2));
            return { .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), .step_failed = true, .error_message = "Получен некорректный ответ от модели (отсутствует поле 'choices').", .recovery_options = {"retry"} };
        }

        const auto& message = response_json["choices"][0]["message"];

        // Case 2: Tool calls requested
        if (message.contains("tool_calls") && message["tool_calls"].is_array() && !message["tool_calls"].empty()) {
            SPDLOG_INFO("LLM запросил вызов инструментов.");
            messageBuilder.addToolCallMessage(message);
            
            auto tool_response = handleToolCalls(message, messageBuilder, false); // Not a continuation, so do danger check
            
            if (tool_response.requires_confirmation || tool_response.step_failed) {
                return tool_response;
            }

            int remaining_iterations = m_config.max_tool_calls - (i + 1);
            messageBuilder.setRemainingIterationsSystemNote(remaining_iterations);
            continue; // Continue to the next iteration of the loop
        }
        // Case 1: Final answer (content without tool calls)
        else if (message.contains("content") && message["content"].is_string()) {
            SPDLOG_INFO("LLM предоставил финальный ответ. Завершение цикла.");
            // The content was already streamed by processChat. We just need to update history and return.
            messageBuilder.addAssistantMessage(message["content"]);
            return {.text = "", .is_final = true, .conversation_history = messageBuilder.getMessages()};
        }
        // Case 3: Empty or unexpected response - THIS IS THE FIX FOR THE BUG
        else {
            SPDLOG_INFO("LLM вернул пустой ответ. Запрашиваем итоговый ответ.");
            return getSummaryAnswer(messageBuilder);
        }
    }

    // Loop finished, max_tool_calls reached
    return forceFinalAnswer(messageBuilder);
}

AssistantResponse QueryProcessor::handleToolCalls(const nlohmann::json& message, MessageBuilder& messageBuilder, bool skip_danger_check) {
    const auto& tool_calls = message["tool_calls"];
    for (const auto& call : tool_calls) {
        auto tool_response = executeToolCall(call, messageBuilder, skip_danger_check);
        // If any tool call requires confirmation or fails, we stop immediately and return that response.
        if (tool_response.requires_confirmation || tool_response.step_failed) {
            return tool_response;
        }
    }
    // All tools executed successfully, continue the loop
    return {.is_final = false};
}

AssistantResponse QueryProcessor::executeToolCall(const nlohmann::json& call, MessageBuilder& messageBuilder, bool skip_danger_check) {
    const auto& function_call = call["function"];
    std::string tool_name = function_call["name"];
    json tool_args;
    const std::string& tool_id = call["id"];

    // --- DANGEROUS TOOL CHECK ---
    if (!skip_danger_check) {
        const auto& dangerous_tools = m_config.dangerous_tools;
        if (std::find(dangerous_tools.begin(), dangerous_tools.end(), tool_name) != dangerous_tools.end()) {
            SPDLOG_WARN("Обнаружен опасный инструмент '{}', требующий подтверждения.", tool_name);
            return {
                .text = "Я собираюсь использовать инструмент '" + tool_name + "'. Разрешить выполнение?",
                .is_final = false, .conversation_history = messageBuilder.getMessages(), .requires_confirmation = true, .pending_tool_call = call
            };
        }
    }

    try {
        if (function_call["arguments"].is_string()) {
            tool_args = json::parse(function_call["arguments"].get<std::string>());
        } else {
            tool_args = function_call["arguments"];
        }
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
        messageBuilder.addToolResultError(tool_id, "Invalid JSON in arguments: " + std::string(e.what()));
        return {
            .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), .step_failed = true,
            .error_message = "Не удалось разобрать аргументы для инструмента '" + tool_name + "': " + e.what(),
            .recovery_options = {"retry", "skip"}
        };
    }

    if (m_send_thought) {
        m_send_thought("Выполняю инструмент: " + tool_name + "...");
    }
    std::string result = m_toolManager.executeTool(tool_name, tool_args, &m_indexer);
    
    try {
         json result_json = json::parse(result);
         if (result_json.contains("error")) {
             std::string error_msg = result_json["error"].get<std::string>();
             SPDLOG_ERROR("Инструмент '{}' завершился с ошибкой: {}", tool_name, error_msg);
             messageBuilder.addToolResultError(tool_id, error_msg);
             return {
                 .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), .step_failed = true,
                 .error_message = "Инструмент '" + tool_name + "' вернул ошибку: " + error_msg,
                 .recovery_options = {"retry", "skip"} // Позволяем пользователю повторить или пропустить
             };
         }
     } catch (const json::exception& e) {
         // Not a JSON error, which is fine. It's just a plain string result.
     }
    messageBuilder.addToolResultMessage(tool_id, result);

    return {.is_final = false}; // Success
}

AssistantResponse QueryProcessor::forceFinalAnswer(MessageBuilder& messageBuilder) {
    SPDLOG_WARN("Превышено максимальное количество итераций ({}) для вызова инструментов. Запрос финального ответа.", m_config.max_tool_calls);
    
    messageBuilder.addForcedFinalAnswerMessage();

    auto final_llm_response = m_llmProvider->processChat(messageBuilder.getMessages(), json::array(), m_send_thought, m_send_stream_chunk);

    if (!final_llm_response.step_failed && final_llm_response.llm_response.contains("choices") && !final_llm_response.llm_response["choices"].empty()) {
        std::string final_text = final_llm_response.llm_response["choices"][0]["message"]["content"];
        messageBuilder.addAssistantMessage(final_text);
        return { .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages() };
    }

    return {
        .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), .step_failed = true,
        .error_message = "Превышен лимит итераций для вызова инструментов (" + std::to_string(m_config.max_tool_calls) + ").",
        .recovery_options = {"re-plan"}
    };
}

AssistantResponse QueryProcessor::getSummaryAnswer(MessageBuilder& messageBuilder) {
    SPDLOG_INFO("Запрос итогового ответа на основе текущей истории.");
    
    auto messages = messageBuilder.getMessages();
    messages.push_back({
        {"role", "user"}, 
        {"content", "Подведи итог и дай финальный ответ пользователю на основе всей предыдущей переписки."}
    });

    auto final_llm_response = m_llmProvider->processChat(messages, json::array(), m_send_thought, m_send_stream_chunk);

    if (!final_llm_response.step_failed && final_llm_response.llm_response.contains("choices") && !final_llm_response.llm_response["choices"].empty()) {
        std::string final_text = final_llm_response.llm_response["choices"][0]["message"]["content"];
        messages.push_back({{"role", "assistant"}, {"content", final_text}});
        return { .text = "", .is_final = true, .conversation_history = messages };
    }

    return {
        .text = "", .is_final = true, .conversation_history = messageBuilder.getMessages(), // Return original history on failure
        .step_failed = true,
        .error_message = "Не удалось получить итоговый ответ от модели.",
        .recovery_options = {"retry"}
    };
}