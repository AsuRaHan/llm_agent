#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "Searcher.h" // For SearchResult
#include "Config.h"

class MessageBuilder {
public:
    MessageBuilder(const nlohmann::json& continuation_history, const Config& config);

    void initialize(const std::vector<SearchResult>& initialContext);
    
    void addToolCallMessage(const nlohmann::json& message);
    void addToolResultMessage(const std::string& tool_id, const std::string& result);
    void addToolResultError(const std::string& tool_id, const std::string& error_message);
    void addAssistantMessage(const std::string& content);
    void addForcedFinalAnswerMessage();
    void setRemainingIterationsSystemNote(int remaining_iterations);

    const nlohmann::json& getMessages() const;

private:
    void prepareNewQueryMessages(const std::vector<SearchResult>& initialContext);

    nlohmann::json m_messages;
    const nlohmann::json& m_continuation_history;
    const Config& m_config;
};