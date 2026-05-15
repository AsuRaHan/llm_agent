#include "SessionManager.h"
#include "Logger.h"

SessionManager::SessionManager() {
    SPDLOG_INFO("SessionManager initialized.");
    // В будущем здесь можно будет загружать сохраненные сессии из файла
}

std::shared_ptr<UserSession> SessionManager::getSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        SPDLOG_DEBUG("Найдена существующая сессия: {}", sessionId);
        return it->second;
    }

    SPDLOG_INFO("Создание новой сессии: {}", sessionId);
    auto newSession = std::make_shared<UserSession>();
    newSession->id = sessionId;
    sessions_[sessionId] = newSession;
    return newSession;
}

void SessionManager::clearSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        SPDLOG_INFO("Очистка сессии: {}", sessionId);
        it->second->history = nlohmann::json::array();
        it->second->status = AgentStatus::IDLE;
        it->second->pending_tool_call = nullptr;
    }
}