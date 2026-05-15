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