#pragma once

#include "UserSession.h"
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

class SessionManager {
public:
    explicit SessionManager(const std::string& sessions_path);
    ~SessionManager();
    std::shared_ptr<UserSession> getSession(const std::string& sessionId);
    void interruptSession(const std::string& sessionId);
    void clearSession(const std::string& sessionId);
    void saveSessions();
    void loadSessions();
private:
    // Сохраняет одну сессию в .json и .md файлы без блокировки мьютекса
    void saveSession_nolock(const UserSession& session);
    void saveSessionHistoryAsMarkdown_nolock(const UserSession& session);
    void saveSessions_nolock();

    std::string sessions_dir_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<UserSession>> sessions_;
};