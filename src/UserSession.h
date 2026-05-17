#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

enum class AgentStatus {
    IDLE,
    THINKING,
    AWAITING_CONFIRMATION,
    GENERATING_PLAN,          // Модель составляет план
    AWAITING_PLAN_CONFIRMATION, // Ждем подтверждения плана от пользователя
    EXECUTING_PLAN,            // Агент выполняет утвержденный план
    AWAITING_ERROR_RECOVERY_DECISION // Ждем решения пользователя по ошибке
};

// Helper to convert enum to string for serialization/logging
inline std::string to_string(AgentStatus status) {
    switch (status) {
        case AgentStatus::IDLE: return "IDLE";
        case AgentStatus::THINKING: return "THINKING";
        case AgentStatus::AWAITING_CONFIRMATION: return "AWAITING_CONFIRMATION";
        case AgentStatus::GENERATING_PLAN: return "GENERATING_PLAN";
        case AgentStatus::AWAITING_PLAN_CONFIRMATION: return "AWAITING_PLAN_CONFIRMATION";
        case AgentStatus::EXECUTING_PLAN: return "EXECUTING_PLAN";
        case AgentStatus::AWAITING_ERROR_RECOVERY_DECISION: return "AWAITING_ERROR_RECOVERY_DECISION";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to enum
inline AgentStatus status_from_string(const std::string& s) {
    if (s == "THINKING") return AgentStatus::THINKING;
    if (s == "AWAITING_CONFIRMATION") return AgentStatus::AWAITING_CONFIRMATION;
    if (s == "GENERATING_PLAN") return AgentStatus::GENERATING_PLAN;
    if (s == "AWAITING_PLAN_CONFIRMATION") return AgentStatus::AWAITING_PLAN_CONFIRMATION;
    if (s == "EXECUTING_PLAN") return AgentStatus::EXECUTING_PLAN;
    if (s == "AWAITING_ERROR_RECOVERY_DECISION") return AgentStatus::AWAITING_ERROR_RECOVERY_DECISION;
    // IDLE is the safe default
    return AgentStatus::IDLE;
};

struct UserSession {
    std::string id;
    nlohmann::json history = nlohmann::json::array();
    AgentStatus status = AgentStatus::IDLE;
    nlohmann::json pending_tool_call = nullptr;

    // --- Поля для планирования ---
    nlohmann::json plan_steps = nlohmann::json::array(); // JSON-массив шагов
    int current_plan_step = -1;          // Номер текущей задачи (-1 — план еще не создан)
    std::string original_user_query;     // Глобальная цель пользователя
};

// --- JSON Serialization ---

inline void to_json(nlohmann::json& j, const UserSession& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"history", s.history},
        {"status", to_string(s.status)}, // serialize as string
        {"pending_tool_call", s.pending_tool_call},
        {"plan_steps", s.plan_steps},
        {"current_plan_step", s.current_plan_step},
        {"original_user_query", s.original_user_query}
    };
}

inline void from_json(const nlohmann::json& j, UserSession& s) {
    j.at("id").get_to(s.id);
    j.at("history").get_to(s.history);
    s.status = status_from_string(j.value("status", "IDLE"));
    // pending_tool_call can be null
    s.pending_tool_call = j.value("pending_tool_call", nullptr);
    s.plan_steps = j.value("plan_steps", nlohmann::json::array());
    s.current_plan_step = j.value("current_plan_step", -1);
    s.original_user_query = j.value("original_user_query", "");
}