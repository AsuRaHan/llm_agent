#include "AssistantRoleHelper/MessageBuilder.h"
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

MessageBuilder::MessageBuilder(const nlohmann::json& continuation_history, const Config& config)
    : m_continuation_history(continuation_history), m_config(config) {}

void MessageBuilder::initialize(const std::vector<SearchResult>& initialContext) {
    if (m_continuation_history.empty() || (m_continuation_history.size() > 0 && m_continuation_history[0].value("role", "") != "system")) {
         prepareNewQueryMessages(initialContext);
    } else {
        m_messages = m_continuation_history;
    }
}

void MessageBuilder::prepareNewQueryMessages(const std::vector<SearchResult>& initialContext) {
    m_messages = json::array();

    std::string system_prompt_content;
    auto system_msg_it = std::find_if(m_continuation_history.begin(), m_continuation_history.end(), [](const json& msg){
        return msg.value("role", "") == "system";
    });

    if (system_msg_it != m_continuation_history.end()) {
        system_prompt_content = system_msg_it->value("content", "");
    } else {
        system_prompt_content = "Ты — эксперт-программист и AI-ассистент. Твоя задача — отвечать на вопросы пользователя о кодовой базе.\n"
                                "У тебя есть лимит на вызов инструментов: " + std::to_string(m_config.max_tool_calls) + " раз на один запрос.\n\n"
                                "Твой план действий:\n"
                                "1.  **Проанализируй контекст.** Тебе предоставлен релевантный контекст, найденный по вопросу пользователя. Внимательно изучи его.\n"
                                "2.  **Оцени достаточность контекста.** Если предоставленные фрагменты кода полностью отвечают на вопрос, или ты собрал достаточно информации, используй инструмент `final_answer` для предоставления полного ответа.\n"
                                "3.  **Используй инструменты, если необходимо.** Если контекст неполный, или тебе нужно увидеть файл целиком, чтобы понять общую картину, используй другие инструменты.\n"
                                "4.  **Думай по шагам.** После каждого шага (вызова инструмента) анализируй полученную информацию и решай, что делать дальше, пока не соберешь достаточно данных для исчерпывающего ответа.\n"
                                "5.  **Дай точный ответ.** В конце, ссылаясь на собранную информацию, используй инструмент `final_answer` с полным ответом.";
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

    m_messages.push_back({{"role", "system"}, {"content", system_prompt_content}});
    for (const auto& msg : m_continuation_history) {
        if (msg.value("role", "") != "system") {
            m_messages.push_back(msg);
        }
    }
}

void MessageBuilder::addToolCallMessage(const nlohmann::json& message) {
    m_messages.push_back(message);
}

void MessageBuilder::addToolResultMessage(const std::string& tool_id, const std::string& result) {
    m_messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", result}});
}

void MessageBuilder::addToolResultError(const std::string& tool_id, const std::string& error_message) {
    std::string error_content = "{\"error\": \"" + error_message + "\"}";
    m_messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", error_content}});
}

void MessageBuilder::addAssistantMessage(const std::string& content) {
    m_messages.push_back({{"role", "assistant"}, {"content", content}});
}

void MessageBuilder::addForcedFinalAnswerMessage() {
    m_messages.push_back({{"role", "user"}, {"content", 
        "Лимит вызовов инструментов исчерпан. Проанализируй всю историю диалога, "
        "включая результат последнего вызова инструмента, и сформируй исчерпывающий финальный ответ для пользователя."
    }});
}

void MessageBuilder::setRemainingIterationsSystemNote(int remaining_iterations) {
    if (remaining_iterations > 0 && !m_messages.empty() && m_messages.back()["role"] == "tool") {
        std::string current_content = m_messages.back()["content"];
        m_messages.back()["content"] = current_content + "\n\n[SYSTEM_NOTE]: Инструменты выполнены. У тебя осталось " + std::to_string(remaining_iterations) + " итераций для вызова инструментов.";
    }
}

const nlohmann::json& MessageBuilder::getMessages() const {
    return m_messages;
}