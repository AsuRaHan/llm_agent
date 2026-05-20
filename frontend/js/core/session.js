/**
 * SessionManager - Управление сессией
 * 
 * @module core/session
 */

class SessionManager {
    /**
     * Конструктор SessionManager
     * @param {string} storageKey - Ключ для localStorage
     */
    constructor(storageKey = 'sessionId') {
        this.storageKey = storageKey;
    }

    /**
     * Получение ID сессии
     * @returns {string} ID сессии
     */
    getSessionId() {
        const sessionId = localStorage.getItem(this.storageKey);
        if (!sessionId) {
            const newSessionId = 'sess_' + Math.random().toString(36).substr(2, 9);
            localStorage.setItem(this.storageKey, newSessionId);
            return newSessionId;
        }
        return sessionId;
    }

    /**
     * Сохранение сессии
     * @param {Object} sessionData - Данные сессии
     */
    saveSession(sessionData) {
        const sessionId = this.getSessionId();
        const session = {
            id: sessionId,
            ...sessionData,
            timestamp: new Date().toISOString()
        };
        localStorage.setItem(`session_${sessionId}`, JSON.stringify(session));
    }

    /**
     * Загрузка сессии
     * @param {string} sessionId - ID сессии
     * @returns {Object|null} Данные сессии или null
     */
    loadSession(sessionId) {
        const sessionData = localStorage.getItem(`session_${sessionId}`);
        if (!sessionData) return null;
        return JSON.parse(sessionData);
    }

    /**
     * Удаление сессии
     * @param {string} sessionId - ID сессии
     */
    deleteSession(sessionId) {
        localStorage.removeItem(`session_${sessionId}`);
    }

    /**
     * Очистка всех сессий
     */
    clearAllSessions() {
        const keys = Object.keys(localStorage);
        keys.forEach(key => {
            if (key.startsWith('session_')) {
                localStorage.removeItem(key);
            }
        });
    }

    /**
     * Получение всех сессий
     * @returns {Array} Массив сессий
     */
    getAllSessions() {
        const keys = Object.keys(localStorage);
        const sessions = [];
        
        keys.forEach(key => {
            if (key.startsWith('session_')) {
                try {
                    const session = JSON.parse(localStorage.getItem(key));
                    sessions.push(session);
                } catch (e) {
                    console.error('Error parsing session:', key, e);
                }
            }
        });
        
        return sessions;
    }
}

export { SessionManager };
