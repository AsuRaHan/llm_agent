#pragma once

#include "UserSession.h"
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

class SessionManager {
public:
    SessionManager();
    ~SessionManager();
    std::shared_ptr<UserSession> getSession(const std::string& sessionId);
    void clearSession(const std::string& sessionId);
    void saveSessions();
    void loadSessions();
private:


    const std::string session_db_path = ".shdata/sessions.json";
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<UserSession>> sessions_;
};