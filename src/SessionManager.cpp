#include "SessionManager.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>

using json = nlohmann::json;

// Каждая сессия будет храниться в отдельном файле в этой директории.
const std::string sessions_dir = ".shdata/sessions";

SessionManager::SessionManager() {
    SPDLOG_INFO("SessionManager initialized.");
    // Убедимся, что директория для хранения сессий существует.
    try {
        std::filesystem::create_directories(sessions_dir);
        SPDLOG_INFO("Директория сессий '{}' готова.", sessions_dir);
    } catch (const std::filesystem::filesystem_error& e) {
        SPDLOG_ERROR("Не удалось создать директорию сессий '{}': {}", sessions_dir, e.what());
    }
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
    saveSession_nolock(*newSession); // Сохраняем новую сессию в ее собственный файл
    return newSession;
}

void SessionManager::interruptSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second->is_interrupted = true;
        SPDLOG_INFO("Сессия {} помечена для прерывания.", sessionId);
    } else {
        SPDLOG_WARN("Попытка прервать несуществующую сессию: {}", sessionId);
    }
}

void SessionManager::clearSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        SPDLOG_INFO("Очистка истории для сессии: {}", sessionId);
        auto& session = it->second;
        session->history = nlohmann::json::array();
        session->status = AgentStatus::IDLE;
        session->pending_tool_call = nullptr;
        saveSession_nolock(*session); // Сохраняем очищенную сессию
    }
}

// Приватный метод для сохранения истории сессии в читаемом Markdown формате.
// Вызывается из saveSession_nolock.
void SessionManager::saveSessionHistoryAsMarkdown_nolock(const UserSession& session) {
    std::filesystem::path md_path = std::filesystem::path(sessions_dir) / (session.id + ".md");
    std::stringstream md_content;
    md_content << "# История чата для сессии: " << session.id << "\n\n";

    try {
        for (const auto& message : session.history) {
            std::string role = message.value("role", "unknown");
            std::string content_str;

            // Содержимое может быть строкой или JSON-объектом/массивом (для tool_calls)
            if (message.contains("content") && message["content"].is_string()) {
                content_str = message["content"].get<std::string>();
            } else if (message.contains("content")) {
                // Красиво форматируем JSON для сложных типов контента
                content_str = "```json\n" + message["content"].dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n```";
            } else {
                content_str = "*(Нет содержимого)*";
            }

            md_content << "## " << role << "\n\n";
            md_content << content_str << "\n\n";
            md_content << "---\n\n";
        }

        std::ofstream f(md_path);
        f << md_content.str();
        SPDLOG_DEBUG("История сессии {} сохранена в '{}'.", session.id, md_path.string());

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Не удалось сохранить историю сессии {} в markdown-файл '{}': {}", session.id, md_path.string(), e.what());
    }
}

// Приватный метод для сохранения одной сессии в JSON-файл.
// Вызывается из публичных методов, которые уже захватили блокировку.
void SessionManager::saveSession_nolock(const UserSession& session) {
    std::filesystem::path json_path = std::filesystem::path(sessions_dir) / (session.id + ".json");
    json data = session;

    try {
        std::ofstream f(json_path);
        f << data.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
        SPDLOG_DEBUG("Сессия {} сохранена в '{}'.", session.id, json_path.string());
        // Также дублируем историю в Markdown
        saveSessionHistoryAsMarkdown_nolock(session);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Не удалось сохранить сессию '{}' в '{}': {}", session.id, json_path.string(), e.what());
    }
}

// Приватный метод, который выполняет сохранение всех сессий без блокировки мьютекса.
// Вызывается из публичных методов, которые уже захватили блокировку.
void SessionManager::saveSessions_nolock() {
    if (sessions_.empty()) {
        SPDLOG_INFO("Нет активных сессий для сохранения.");
        return;
    }
    
    SPDLOG_INFO("Сохранение {} сессий в директорию '{}'...", sessions_.size(), sessions_dir);
    for (const auto& [id, session_ptr] : sessions_) {
        saveSession_nolock(*session_ptr);
    }
    SPDLOG_INFO("Все сессии сохранены.");
}

void SessionManager::saveSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    saveSessions_nolock();
}

void SessionManager::loadSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SPDLOG_INFO("Загрузка сессий из директории '{}'...", sessions_dir);
    int loaded_count = 0;

    try {
        if (!std::filesystem::exists(sessions_dir) || !std::filesystem::is_directory(sessions_dir)) {
            SPDLOG_INFO("Директория сессий '{}' не найдена или не является директорией. Загрузка пропущена.", sessions_dir);
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(sessions_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                const auto& path = entry.path();
                std::string sessionId = path.stem().string();

                std::ifstream f(path);
                if (!f.is_open()) {
                    SPDLOG_ERROR("Не удалось открыть файл сессии '{}' для чтения.", path.string());
                    continue;
                }

                if (f.peek() == std::ifstream::traits_type::eof()) {
                    SPDLOG_WARN("Файл сессии '{}' пуст. Пропускаем.", path.string());
                    continue;
                }

                try {
                    json data = json::parse(f);
                    auto session = std::make_shared<UserSession>(data.get<UserSession>());
                    
                    if (session->id != sessionId) {
                        SPDLOG_WARN("Несоответствие ID сессии в файле '{}' (содержимое: '{}', имя файла: '{}'). Используется ID из имени файла.", path.string(), session->id, sessionId);
                        session->id = sessionId;
                    }

                    // Важно: после перезапуска ни один агент не может находиться в "занятом" состоянии.
                    // Сбрасываем статус в IDLE, чтобы избежать зависания.
                    session->status = AgentStatus::IDLE;
                    session->pending_tool_call = nullptr;
                    session->is_interrupted = false; // Также сбрасываем флаг прерывания

                    sessions_[sessionId] = session;
                    loaded_count++;
                    SPDLOG_DEBUG("Сессия '{}' успешно загружена.", sessionId);

                } catch (const json::exception& e) {
                    SPDLOG_ERROR("Ошибка парсинга файла сессии '{}': {}", path.string(), e.what());
                }
            }
        }

        if (loaded_count > 0) {
            SPDLOG_INFO("Загружено {} сессий из '{}'.", loaded_count, sessions_dir);
        } else {
            SPDLOG_INFO("В директории '{}' не найдено сессий для загрузки.", sessions_dir);
        }

    } catch (const std::filesystem::filesystem_error& e) {
        SPDLOG_ERROR("Ошибка при доступе к директории сессий '{}': {}", sessions_dir, e.what());
    }
}
