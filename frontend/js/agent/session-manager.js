/**
 * Session Manager
 * Управление сессиями чатов и их состоянием
 * Vanilla JS + CommonJS
 */

class SessionManager {
    constructor() {
        this.storageKey = 'smart_hammer_sessions';
        this.sessions = new Map();
        this.currentSessionId = null;
    }

    /**
     * Загрузить все сессии из localStorage
     */
    loadSessions() {
        try {
            const data = localStorage.getItem(this.storageKey);
            if (data) {
                const sessions = JSON.parse(data);
                sessions.forEach(session => {
                    this.sessions.set(session.id, session);
                });
            }
        } catch (e) {
            console.error('Ошибка загрузки сессий:', e);
        }
        return Array.from(this.sessions.values());
    }

    /**
     * Сохранить все сессии в localStorage
     */
    saveSessions() {
        try {
            const data = Array.from(this.sessions.values());
            localStorage.setItem(this.storageKey, JSON.stringify(data));
        } catch (e) {
            console.error('Ошибка сохранения сессий:', e);
        }
    }

    /**
     * Создать новую сессию
     */
    createSession(title = 'Новый чат') {
        const sessionId = `session-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
        const session = {
            id: sessionId,
            title: title,
            history: [],
            status: 'IDLE',
            createdAt: new Date().toISOString(),
            updatedAt: new Date().toISOString(),
            isInterrupted: false
        };
        this.sessions.set(sessionId, session);
        this.currentSessionId = sessionId;
        this.saveSessions();
        return session;
    }

    /**
     * Получить сессию по ID
     */
    getSession(sessionId) {
        return this.sessions.get(sessionId) || null;
    }

    /**
     * Обновить сессию
     */
    updateSession(sessionId, updates) {
        const session = this.sessions.get(sessionId);
        if (session) {
            Object.assign(session, updates);
            session.updatedAt = new Date().toISOString();
            this.saveSessions();
            return session;
        }
        return null;
    }

    /**
     * Удалить сессию
     */
    deleteSession(sessionId) {
        this.sessions.delete(sessionId);
        if (this.currentSessionId === sessionId) {
            this.currentSessionId = null;
        }
        this.saveSessions();
        return true;
    }

    /**
     * Очистить сессию (оставить только заголовок)
     */
    clearSession(sessionId) {
        const session = this.sessions.get(sessionId);
        if (session) {
            session.history = [];
            session.status = 'IDLE';
            session.updatedAt = new Date().toISOString();
            this.saveSessions();
        }
    }

    /**
     * Получить текущую сессию
     */
    getCurrentSession() {
        return this.sessions.get(this.currentSessionId) || null;
    }

    /**
     * Установить текущую сессию
     */
    setCurrentSession(sessionId) {
        this.currentSessionId = sessionId;
    }

    /**
     * Получить список всех сессий
     */
    getAllSessions() {
        return Array.from(this.sessions.values());
    }

    /**
     * Получить сессии, отсортированные по дате (новые первыми)
     */
    getSortedSessions() {
        return Array.from(this.sessions.values())
            .sort((a, b) => new Date(b.updatedAt) - new Date(a.updatedAt));
    }

    /**
     * Обновить статус сессии
     */
    updateSessionStatus(sessionId, status) {
        return this.updateSession(sessionId, { status });
    }

    /**
     * Установить pending tool call
     */
    setPendingToolCall(sessionId, toolCall) {
        return this.updateSession(sessionId, {
            status: 'AWAITING_CONFIRMATION',
            pending_tool_call: toolCall
        });
    }

    /**
     * Удалить pending tool call
     */
    clearPendingToolCall(sessionId) {
        return this.updateSession(sessionId, {
            pending_tool_call: null
        });
    }

    /**
     * Установить флаг прерывания
     */
    setInterrupted(sessionId, isInterrupted) {
        return this.updateSession(sessionId, { isInterrupted });
    }

    /**
     * Добавить сообщение в историю
     */
    addMessage(sessionId, message) {
        const session = this.sessions.get(sessionId);
        if (session) {
            session.history.push(message);
            session.updatedAt = new Date().toISOString();
            this.saveSessions();
            return session;
        }
        return null;
    }

    /**
     * Получить историю сообщений
     */
    getHistory(sessionId) {
        const session = this.sessions.get(sessionId);
        return session ? session.history : [];
    }

    /**
     * Очистить историю
     */
    clearHistory(sessionId) {
        this.clearSession(sessionId);
    }

    /**
     * Получить количество сессий
     */
    getSessionsCount() {
        return this.sessions.size;
    }

    /**
     * Проверка: существует ли сессия
     */
    hasSession(sessionId) {
        return this.sessions.has(sessionId);
    }
}

// Экспорт через CommonJS
module.exports = SessionManager;
