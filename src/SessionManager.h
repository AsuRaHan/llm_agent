#pragma once

#include "UserSession.h"
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

class SessionManager {
public:
    SessionManager();
    std::shared_ptr<UserSession> getSession(const std::string& sessionId);
    void clearSession(const std::string& sessionId);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<UserSession>> sessions_;
};