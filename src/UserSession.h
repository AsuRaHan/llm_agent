#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

enum class AgentStatus {
    IDLE,
    THINKING, // General "busy" state
    AWAITING_CONFIRMATION, // Waiting for user to confirm a dangerous tool
    AWAITING_ERROR_RECOVERY_DECISION // Waiting for user to decide how to handle a step failure
};

// Helper to convert enum to string for serialization/logging
inline std::string to_string(AgentStatus status) {
    switch (status) {
        case AgentStatus::IDLE: return "IDLE";
        case AgentStatus::THINKING: return "THINKING";
        case AgentStatus::AWAITING_CONFIRMATION: return "AWAITING_CONFIRMATION";
        case AgentStatus::AWAITING_ERROR_RECOVERY_DECISION: return "AWAITING_ERROR_RECOVERY_DECISION";
        default: return "UNKNOWN";
    }
}

// Helper to convert string to enum
inline AgentStatus status_from_string(const std::string& s) {
    if (s == "THINKING") return AgentStatus::THINKING;
    if (s == "AWAITING_CONFIRMATION") return AgentStatus::AWAITING_CONFIRMATION;
    if (s == "AWAITING_ERROR_RECOVERY_DECISION") return AgentStatus::AWAITING_ERROR_RECOVERY_DECISION;
    // IDLE is the safe default
    return AgentStatus::IDLE;
};

struct UserSession {
    std::string id;
    nlohmann::json history = nlohmann::json::array();
    AgentStatus status = AgentStatus::IDLE;
    nlohmann::json pending_tool_call = nullptr;
    std::atomic<bool> is_interrupted{false};

    // Default constructor
    UserSession() = default;

    // Custom copy constructor to handle the non-copyable std::atomic member.
    // This is required for nlohmann::json deserialization (e.g., .get<UserSession>()).
    // The is_interrupted flag is a transient runtime state and should not be copied.
    // It will be default-initialized to 'false' by its member initializer.
    UserSession(const UserSession& other)
        : id(other.id),
          history(other.history),
          status(other.status), // status is reset on load anyway
          pending_tool_call(other.pending_tool_call)
    {}
};

// --- JSON Serialization ---

inline void to_json(nlohmann::json& j, const UserSession& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"history", s.history},
        {"status", to_string(s.status)}, // serialize as string
        {"pending_tool_call", s.pending_tool_call}
    };
}

inline void from_json(const nlohmann::json& j, UserSession& s) {
    j.at("id").get_to(s.id);
    j.at("history").get_to(s.history);
    s.status = status_from_string(j.value("status", "IDLE"));
    // pending_tool_call can be null
    s.pending_tool_call = j.value("pending_tool_call", nullptr);
}