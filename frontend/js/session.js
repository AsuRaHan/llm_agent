/**
 * Управляет сессиями пользователя, включая сохранение/загрузку состояния в localStorage.
 */
class SessionManager {
    /**
     * @param {SocketManager} socketManager Экземпляр SocketManager.
     * @param {string} [prefix='llm_agent_'] Префикс для ключей localStorage.
     * @param {number} [expiresAfter=86400000] Время жизни сессии в мс (по умолчанию 24 часа).
     */
    constructor(socketManager, prefix = 'llm_agent_', expiresAfter = 86400000) {
        this.socketManager = socketManager;
        this.prefix = prefix;
        this.expiresAfter = expiresAfter;
        this.sessionId = null;
        this.sessionData = {}; // Хранит текущие данные сессии в памяти

        this._init();
    }

    /**
     * Инициализирует SessionManager, пытаясь загрузить существующую сессию или создать новую.
     * @private
     */
    _init() {
        this.sessionId = this._getStoredSessionId();
        if (this.sessionId && !this._isSessionExpired(this.sessionId)) {
            this.sessionData = this._loadSessionData(this.sessionId);
            console.log('Сессия восстановлена:', this.sessionId);
        } else {
            this.createSession();
        }
    }

    /**
     * Генерирует новый UUID v4.
     * @returns {string} Новый UUID.
     * @private
     */
    _generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }

    /**
     * Получает ID сессии из localStorage.
     * @returns {string|null} ID сессии или null, если нет.
     * @private
     */
    _getStoredSessionId() {
        return localStorage.getItem(`${this.prefix}session_id`);
    }

    /**
     * Проверяет, истек ли срок действия сессии.
     * @param {string} sessionId ID сессии.
     * @returns {boolean} True, если сессия истекла.
     * @private
     */
    _isSessionExpired(sessionId) {
        const storedData = localStorage.getItem(`${this.prefix}session_data_${sessionId}`);
        if (!storedData) return true;
        try {
            const { expiresAt } = JSON.parse(storedData);
            return Date.now() > expiresAt;
        } catch (e) {
            console.error('Ошибка парсинга данных сессии из localStorage:', e);
            return true; // Считаем истекшей при ошибке парсинга
        }
    }

    /**
     * Загружает данные сессии из localStorage.
     * @param {string} sessionId ID сессии.
     * @returns {object} Данные сессии.
     * @private
     */
    _loadSessionData(sessionId) {
        const storedData = localStorage.getItem(`${this.prefix}session_data_${sessionId}`);
        return storedData ? JSON.parse(storedData) : { history: [], status: 'IDLE' };
    }

    /**
     * Создает новую сессию.
     */
    createSession() {
        this.sessionId = this._generateUUID();
        localStorage.setItem(`${this.prefix}session_id`, this.sessionId);
        this.sessionData = {
            history: [],
            status: 'IDLE',
            createdAt: Date.now(),
            expiresAt: Date.now() + this.expiresAfter
        };
        this.saveSession();
        console.log('Новая сессия создана:', this.sessionId);
    }

    /**
     * Сохраняет текущее состояние сессии в localStorage.
     */
    saveSession() {
        this.sessionData.expiresAt = Date.now() + this.expiresAfter; // Обновляем срок действия при каждом сохранении
        localStorage.setItem(`${this.prefix}session_data_${this.sessionId}`, JSON.stringify(this.sessionData));
    }

    /**
     * Загружает состояние сессии (из текущего this.sessionData).
     * @returns {object} Текущие данные сессии.
     */
    loadSession() {
        return this.sessionData;
    }

    /**
     * Очищает текущую сессию из localStorage и сбрасывает состояние.
     */
    clearSession() {
        localStorage.removeItem(`${this.prefix}session_id`);
        localStorage.removeItem(`${this.prefix}session_data_${this.sessionId}`);
        this.sessionId = null;
        this.sessionData = {};
        this.createSession(); // Создаем новую сессию после очистки
        console.log('Сессия очищена.');
    }

    /**
     * Обновляет часть данных сессии и сохраняет её.
     * @param {string} key Ключ для обновления.
     * @param {*} value Новое значение.
     */
    updateSessionData(key, value) {
        this.sessionData[key] = value;
        this.saveSession();
    }

    /**
     * Получает ID текущей сессии.
     * @returns {string|null}
     */
    getSessionId() {
        return this.sessionId;
    }

    /**
     * Очищает состояние стриминга (например, накопленный текст).
     * В данном контексте это просто сброс accumulatedText, если он хранится в SessionManager.
     * Если StreamingManager управляет этим, то этот метод может быть пустым или вызывать метод StreamingManager.
     */
    clearStreamingState() {
        // Если StreamingManager управляет accumulatedText, то здесь ничего не делаем.
        // Если бы SessionManager хранил его, то: this.sessionData.accumulatedText = '';
    }
}

export { SessionManager };