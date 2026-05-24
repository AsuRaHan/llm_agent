#include "AssistantRoleHelper/QueryProcessor.h"
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
    const std::function<void(const std::string&)>& send_stream_chunk
) : m_llmProvider(llmProvider),
    m_toolManager(toolManager),
    m_config(config),
    m_indexer(indexer),
    m_send_thought(send_thought),
    m_send_stream_chunk(send_stream_chunk)
{}

AssistantResponse QueryProcessor::process(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    const nlohmann::json& continuation_history
) {
    bool is_continuation = initializeMessages(userQuery, initialContext, continuation_history);

    if (is_continuation) {
        auto response = handleContinuationAfterConfirmation();
        // If the tool execution failed, return immediately.
        if (response.step_failed) {
            return response;
        }
    }

    // ReAct Loop
    for (int i = 0; i < m_config.max_tool_calls; ++i) {
        SPDLOG_INFO("Итерация {}/{} цикла обработки запроса...", i + 1, m_config.max_tool_calls);
        
        AssistantResponse llm_response = m_llmProvider->processChat(m_messages, m_toolManager.getToolsSpecification(), m_send_thought, m_send_stream_chunk);

        if (llm_response.step_failed) {
            return { .text = "", .is_final = true, .conversation_history = m_messages, .step_failed = true, .error_message = llm_response.error_message, .recovery_options = {"retry"} };
        }

        json response_json = llm_response.llm_response;
        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            SPDLOG_ERROR("Ответ LLM не содержит 'choices'. Тело: {}", response_json.dump(2));
            return { .text = "", .is_final = true, .conversation_history = m_messages, .step_failed = true, .error_message = "Получен некорректный ответ от модели (отсутствует поле 'choices').", .recovery_options = {"retry"} };
        }

        const auto& message = response_json["choices"][0]["message"];

        // Case 1: Direct answer
        if (message.contains("content") && message["content"].is_string() && !message.value("tool_calls", json::array()).is_array()) {
            SPDLOG_INFO("LLM предоставил прямой ответ (потоково). Завершение цикла.");
            m_messages.push_back({{"role", "assistant"}, {"content", message["content"]}});
            return {.text = "", .is_final = true, .conversation_history = m_messages};
        }

        // Case 2: Tool calls
        if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
            SPDLOG_INFO("LLM запросил вызов инструментов.");
            m_messages.push_back(message); // Add assistant's turn with tool_calls to history
            
            auto tool_response = handleToolCalls(message);
            // If a tool requires confirmation or failed, stop and return that response.
            if (tool_response.requires_confirmation || tool_response.step_failed) {
                return tool_response;
            }

            // All tools executed, continue the loop.
            int remaining_iterations = m_config.max_tool_calls - (i + 1);
            if (remaining_iterations > 0 && !m_messages.empty() && m_messages.back()["role"] == "tool") {
                std::string current_content = m_messages.back()["content"];
                m_messages.back()["content"] = current_content + "\n\n[SYSTEM_NOTE]: Инструменты выполнены. У тебя осталось " + std::to_string(remaining_iterations) + " итераций для вызова инструментов.";
            }
            continue;
        }

        // Fallback: Empty response, break loop to force final answer.
        SPDLOG_INFO("LLM вернул пустой ответ, завершаем цикл и переходим к подведению итогов.");
        break;
    }

    return forceFinalAnswer();
}

bool QueryProcessor::initializeMessages(const std::string& userQuery, const std::vector<SearchResult>& initialContext, const nlohmann::json& continuation_history) {
    bool is_continuation_after_confirmation = !continuation_history.empty() && userQuery.empty();

    if (is_continuation_after_confirmation) {
        m_messages = continuation_history;
    } else {
        prepareNewQueryMessages(initialContext, continuation_history);
    }
    return is_continuation_after_confirmation;
}

void QueryProcessor::prepareNewQueryMessages(const std::vector<SearchResult>& initialContext, const nlohmann::json& continuation_history) {
    json messages_for_llm = json::array();

    std::string system_prompt_content;
    auto system_msg_it = std::find_if(continuation_history.begin(), continuation_history.end(), [](const json& msg){
        return msg.value("role", "") == "system";
    });

    if (system_msg_it != continuation_history.end()) {
        system_prompt_content = system_msg_it->value("content", "");
    } else {
        system_prompt_content = "Ты — эксперт-программист и AI-ассистент. Твоя задача — отвечать на вопросы пользователя о кодовой базе.\n"
                                "У тебя есть лимит на вызов инструментов: " + std::to_string(m_config.max_tool_calls) + " раз на один запрос.\n\n"
                                "Твой план действий:\n"
                                "1.  **Проанализируй контекст.** Тебе предоставлен релевантный контекст, найденный по вопросу пользователя. Внимательно изучи его.\n"
                                "2.  **Оцени достаточность контекста.** Если предоставленные фрагменты кода полностью отвечают на вопрос, сформируй ответ на их основе.\n"
                                "3.  **Используй инструменты, если необходимо.** Если контекст неполный, или тебе нужно увидеть файл целиком, чтобы понять общую картину, используй инструменты.\n"
                                "4.  **Думай по шагам.** После каждого шага (вызова инструмента) анализируй полученную информацию и решай, что делать дальше, пока не соберешь достаточно данных для исчерпывающего ответа.\n"
                                "5.  **Дай точный ответ.** В конце, ссылаясь на собранную информацию, дай пользователю точный и подробный ответ.";
    }

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

    messages_for_llm.push_back({{"role", "system"}, {"content", system_prompt_content}});
    for (const auto& msg : continuation_history) {
        if (msg.value("role", "") != "system") {
            messages_for_llm.push_back(msg);
        }
    }
    m_messages = messages_for_llm;
}

AssistantResponse QueryProcessor::handleContinuationAfterConfirmation() {
    const auto& last_message = m_messages.back();

    if (last_message.value("role", "") == "assistant" && last_message.contains("tool_calls")) {
        SPDLOG_INFO("Продолжение после подтверждения. Выполнение отложенного инструмента...");
        return handleToolCalls(last_message);
    }
    // Should not happen, but as a fallback, indicate success to continue to ReAct loop
    return {.is_final = false};
}

AssistantResponse QueryProcessor::handleToolCalls(const nlohmann::json& message) {
    const auto& tool_calls = message["tool_calls"];
    for (const auto& call : tool_calls) {
        auto tool_response = executeToolCall(call);
        // If any tool call requires confirmation or fails, we stop immediately and return that response.
        if (tool_response.requires_confirmation || tool_response.step_failed) {
            return tool_response;
        }
    }
    // All tools executed successfully, continue the loop
    return {.is_final = false};
}

AssistantResponse QueryProcessor::executeToolCall(const nlohmann::json& call) {
    const auto& function_call = call["function"];
    std::string tool_name = function_call["name"];
    json tool_args;
    const std::string& tool_id = call["id"];

    // --- DANGEROUS TOOL CHECK ---
    const auto& dangerous_tools = m_config.dangerous_tools;
    if (std::find(dangerous_tools.begin(), dangerous_tools.end(), tool_name) != dangerous_tools.end()) {
        SPDLOG_WARN("Обнаружен опасный инструмент '{}', требующий подтверждения.", tool_name);
        return {
            .text = "Я собираюсь использовать инструмент '" + tool_name + "'. Разрешить выполнение?",
            .is_final = false, .conversation_history = m_messages, .requires_confirmation = true, .pending_tool_call = call
        };
    }

    try {
        if (function_call["arguments"].is_string()) {
            tool_args = json::parse(function_call["arguments"].get<std::string>());
        } else {
            tool_args = function_call["arguments"];
        }
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
        std::string error_content = "{\"error\": \"Invalid JSON in arguments: " + std::string(e.what()) + "\"}";
        m_messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", error_content}});
        return {
            .text = "", .is_final = true, .conversation_history = m_messages, .step_failed = true,
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
             SPDLOG_ERROR("Инструмент '{}' завершился с ошибкой: {}", tool_name, result_json["error"].get<std::string>());
         }
     } catch (const json::exception& e) {
         // Not a JSON error, which is fine. It's just a string result.
     }
    m_messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", result}});

    return {.is_final = false}; // Success
}

AssistantResponse QueryProcessor::forceFinalAnswer() {
    SPDLOG_WARN("Превышено максимальное количество итераций ({}) для вызова инструментов. Запрос финального ответа.", m_config.max_tool_calls);
    
    m_messages.push_back({
        {"role", "user"},
        {"content", "Ты достиг максимального количества итераций для вызова инструментов. Предоставь пользователю окончательный ответ, основываясь на уже собранной информации."}
    });

    auto final_llm_response = m_llmProvider->processChat(m_messages, json::array(), m_send_thought, m_send_stream_chunk);

    if (!final_llm_response.step_failed && final_llm_response.llm_response.contains("choices") && !final_llm_response.llm_response["choices"].empty()) {
        std::string final_text = final_llm_response.llm_response["choices"][0]["message"]["content"];
        m_messages.push_back({{"role", "assistant"}, {"content", final_text}});
        return {
            .text = "", .is_final = true, .conversation_history = m_messages
        };
    }

    return {
        .text = "", .is_final = true, .conversation_history = m_messages, .step_failed = true,
        .error_message = "Превышен лимит итераций для вызова инструментов (" + std::to_string(m_config.max_tool_calls) + ").",
        .recovery_options = {"re-plan"}
    };
}