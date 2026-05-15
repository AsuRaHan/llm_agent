#pragma once

#include "nlohmann/json.hpp"
#include <string>
#include <vector>

// Представляет состояние агента для сессии
enum class AgentStatus {
    IDLE,
    THINKING,
    AWAITING_CONFIRMATION
};

// Конвертирует AgentStatus в строку для JSON
inline std::string to_string(AgentStatus status) {
    switch (status) {
        case AgentStatus::IDLE: return "idle";
        case AgentStatus::THINKING: return "thinking";
        case AgentStatus::AWAITING_CONFIRMATION: return "awaiting_confirmation";
        default: return "idle";
    }
}

struct UserSession {
    std::string id;
    nlohmann::json history = nlohmann::json::array();
    AgentStatus status = AgentStatus::IDLE;
    nlohmann::json pending_tool_call = nullptr; // Хранит вызов инструмента, который требует подтверждения
};