#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct AssistantResponse {
    std::string text;
    bool is_final;
    nlohmann::json conversation_history;
    bool requires_confirmation = false;
    nlohmann::json pending_tool_call = nullptr;
    bool plan_completed = true;
    bool step_failed = false; // true, если в процессе выполнения шага произошла ошибка

    // Поля для обработки ошибок
    std::string error_message = ""; // Детальное сообщение об ошибке
    std::vector<std::string> recovery_options; // Предлагаемые варианты восстановления (retry, skip, re-plan)

    // Raw response from LLM
    nlohmann::json llm_response;
};