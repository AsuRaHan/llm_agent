#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <atomic>

enum class AgentStatus {
    IDLE,
    THINKING,
    AWAITING_CONFIRMATION,
    AWAITING_ERROR_RECOVERY_DECISION
};

// Helper to convert enum to string for JSON
inline std::string to_string(AgentStatus status) {
    switch (status) {
        case AgentStatus::IDLE: return "IDLE";
        case AgentStatus::THINKING: return "THINKING";
        case AgentStatus::AWAITING_CONFIRMATION: return "AWAITING_CONFIRMATION";
        case AgentStatus::AWAITING_ERROR_RECOVERY_DECISION: return "AWAITING_ERROR_RECOVERY_DECISION";
        default: return "UNKNOWN";
    }
}

struct UserSession {
    std::string id;
    nlohmann::json history = nlohmann::json::array();
    AgentStatus status = AgentStatus::IDLE;
    nlohmann::json pending_tool_call = nullptr;
    std::atomic<bool> is_interrupted{false};
    std::string last_error_message; // NEW: To store the error for the retry logic

    // Default constructor
    UserSession() = default;

    // Custom copy constructor to handle std::atomic
    UserSession(const UserSession& other)
        : id(other.id),
          history(other.history),
          status(other.status),
          pending_tool_call(other.pending_tool_call),
          is_interrupted(other.is_interrupted.load()),
          last_error_message(other.last_error_message)
    {}

    // Custom copy assignment operator
    UserSession& operator=(const UserSession& other) {
        if (this != &other) {
            id = other.id;
            history = other.history;
            status = other.status;
            pending_tool_call = other.pending_tool_call;
            is_interrupted.store(other.is_interrupted.load());
            last_error_message = other.last_error_message;
        }
        return *this;
    }
};

// Serialization/Deserialization for UserSession
// We only save/load persistent data. Transient state is reset.
inline void to_json(nlohmann::json& j, const UserSession& s) {
    j = {
        {"id", s.id},
        {"history", s.history}
    };
}

inline void from_json(const nlohmann::json& j, UserSession& s) {
    s.id = j.value("id", "");
    s.history = j.value("history", nlohmann::json::array());
    // Reset transient state on load
    s.status = AgentStatus::IDLE;
    s.pending_tool_call = nullptr;
    s.is_interrupted = false;
    s.last_error_message = "";
}