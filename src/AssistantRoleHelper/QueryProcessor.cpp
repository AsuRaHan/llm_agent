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

    // Case 1: Direct answer
    if (message.contains("content") && message["content"].is_string() && !message.value("tool_calls", json::array()).is_array()) {
        SPDLOG_INFO("LLM предоставил прямой ответ (потоково). Завершение цикла.");
        messageBuilder.addAssistantMessage(message["content"]);
        return {.text = "", .is_final = true, .conversation_history = messageBuilder.getMessages()};
    }

    // Case 2: Tool calls
    if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
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

    // Fallback: Empty response, break loop to force final answer.
    SPDLOG_INFO("LLM вернул пустой ответ, завершаем цикл и переходим к подведению итогов.");
    return {.is_final = false, .should_break = true}; // Signal to break
}

AssistantResponse QueryProcessor::runThinkingPhase(MessageBuilder& messageBuilder, int iteration, const std::string& userQuery, const std::vector<SearchResult>& initialContext) {
    SPDLOG_INFO("=== Фаза мышления модели ===");

    if (m_is_interrupted.load()) {
        SPDLOG_INFO("Agent interruption detected during thinking. Stopping.");
        return { .text = "Задача прервана пользователем.", .is_final = true, .conversation_history = messageBuilder.getMessages() };
    }

    // === ШАГ 1: Анализ запроса и контекста ===
    // Создаем временное сообщение для LLM, чтобы она "подумала"
    json thinking_messages = messageBuilder.getMessages();
    std::string thinking_prompt = "You are an expert AI assistant. Your goal is to answer the user's request. Analyze the request and context. "
                                  "Decide if you can answer directly without tools, or if you need to use tools. "
                                  "If you can provide a complete answer without tools (e.g., the context is sufficient, or the user asks for something you know, like how to use `git`), "
                                  "then provide the complete, final answer directly in Markdown. Your response should be ONLY the markdown answer. "
                                  "If you need tools, respond with a JSON object `{\"plan\": [\"step 1\", \"step 2\"]}` detailing the steps. Your response should be ONLY the JSON plan.\n\n"
                                  "Запрос: " + userQuery + "\n"
                                  "Контекст: " + std::to_string(initialContext.size()) + " источников";
    thinking_messages.push_back({{"role", "user"}, {"content", thinking_prompt}});


    if (m_send_thought) {
        m_send_thought("Анализирую и планирую шаги...");
    }

    // Call LLM without tools to get a plan or a direct answer.
    auto thinking_response = m_llmProvider->processChat(
        thinking_messages,
        json::array(), // No tools for this thinking step
        nullptr, // No thought streaming for this internal step
        nullptr  // No token streaming for this internal step
    );

    if (thinking_response.step_failed || !thinking_response.llm_response.contains("choices") || thinking_response.llm_response["choices"].empty()) {
        SPDLOG_WARN("Ошибка на этапе мышления: {}", thinking_response.error_message);
        // Let the main loop try to handle it.
        return { .is_final = false };
    }

    const auto& thinking_message = thinking_response.llm_response["choices"][0]["message"];
    std::string thinking_content = thinking_message.value("content", "");
    SPDLOG_INFO("Мысль модели: {}", thinking_content);

    // Check if the response is a plan (JSON) or a direct answer (Markdown).
    try {
        json content_json = json::parse(thinking_content);
        if (content_json.is_object() && content_json.contains("plan")) {
            SPDLOG_INFO("Модель сгенерировала план. Продолжаем с циклом ReAct.");
            messageBuilder.addAssistantMessage("[Внутренний план]:\n" + thinking_content);
            return { .is_final = false }; // It's a plan, so continue to ReAct loop.
        }
    } catch (const json::exception& e) {
        // Not a JSON, so it's a direct answer.
    }

    // It's a direct answer. Stream it and finish.
    SPDLOG_INFO("Фаза мышления дала прямой ответ. Отправка пользователю.");
    m_send_stream_chunk(thinking_content);
    messageBuilder.addAssistantMessage(thinking_content);
    return { .text = thinking_content, .is_final = true, .conversation_history = messageBuilder.getMessages() };
}

AssistantResponse QueryProcessor::processAdvanced(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    const nlohmann::json& continuation_history
) {
    MessageBuilder messageBuilder(continuation_history, m_config);
    bool is_continuation = !continuation_history.empty() && userQuery.empty();

    if (is_continuation) {
        messageBuilder.initialize(initialContext);
        auto response = handleContinuationAfterConfirmation(messageBuilder);
        if (response.step_failed) {
            response.conversation_history = messageBuilder.getMessages();
            return response;
        }
    }

    for (int i = 0; i < m_config.max_tool_calls; ++i) {
        if (i == 0 && !is_continuation) {
            messageBuilder.initialize(initialContext);
            auto thinking_response = runThinkingPhase(messageBuilder, i, userQuery, initialContext);
            if (thinking_response.is_final) return thinking_response;
        }
        
        auto action_response = runReActIteration(messageBuilder, i);
        if (action_response.should_break) break;
        if (action_response.is_final || action_response.requires_confirmation || action_response.step_failed) return action_response;
    }

    return forceFinalAnswer(messageBuilder);
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