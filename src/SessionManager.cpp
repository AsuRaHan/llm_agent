#include "SessionManager.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

SessionManager::SessionManager() {
    SPDLOG_INFO("SessionManager initialized.");
    loadSessions();
}

SessionManager::~SessionManager() {
    // Сохраняем сессии при уничтожении объекта, чтобы гарантировать сохранение при выходе
    saveSessions();
    SPDLOG_INFO("SessionManager destroyed, sessions saved.");
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
        SPDLOG_INFO("Очистка истории для сессии: {}", sessionId);
        it->second->history = nlohmann::json::array();
        // Статус и другие поля сессии сбрасываются в WebSocketServer, чтобы корректно управлять состоянием (например, FileWatcher)
    }
}

void SessionManager::saveSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.empty()) {
        if (std::filesystem::exists(session_db_path)) {
            std::filesystem::remove(session_db_path);
            SPDLOG_INFO("Нет активных сессий, файл '{}' удален.", session_db_path);
        }
        return;
    }

    json data;
    for (const auto& [id, session_ptr] : sessions_) {
        data[id] = *session_ptr;
    }

    try {
        std::ofstream f(session_db_path);
        f << data.dump(4);
        SPDLOG_INFO("Сохранено {} сессий в '{}'.", sessions_.size(), session_db_path);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Не удалось сохранить сессии в '{}': {}", session_db_path, e.what());
    }
}

void SessionManager::loadSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream f(session_db_path);
    if (!f.is_open()) {
        SPDLOG_INFO("Файл сессий '{}' не найден. Будет создан новый при завершении работы.", session_db_path);
        return;
    }

    try {
        json data = json::parse(f);
        for (auto& [id, session_json] : data.items()) {
            auto session = std::make_shared<UserSession>(session_json.get<UserSession>());
            // Важно: после перезапуска ни один агент не может находиться в "занятом" состоянии.
            // Сбрасываем статус в IDLE, чтобы избежать зависания.
            session->status = AgentStatus::IDLE;
            session->pending_tool_call = nullptr;
            sessions_[id] = session;
        }
        SPDLOG_INFO("Загружено {} сессий из '{}'.", sessions_.size(), session_db_path);
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Ошибка парсинга файла сессий '{}': {}", session_db_path, e.what());
    }
}