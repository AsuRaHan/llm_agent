/**
 * SocketManager - Управление WebSocket соединением
 * 
 * @module core/socket
 */

class SocketManager {
    /**
     * Конструктор SocketManager
     * @param {string} sessionId - ID сессии
     */
    constructor(sessionId) {
        this.socket = null;
        this.sessionId = sessionId;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.keepAliveInterval = null;
    }

    /**
     * Подключение к WebSocket серверу
     * @returns {Promise<void>}
     */
    connect() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsHost = window.location.hostname;
        const wsPort = 9000;
        
        this.socket = new WebSocket(`${wsProtocol}//${wsHost}:${wsPort}/ws`);

        this.socket.onopen = () => {
            console.log('WebSocket connection established.');
            this.send({ type: 'sync_session', session_id: this.sessionId });
        };

        this.socket.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            console.log('Received:', msg);
            
            // Передаем сообщение обработчику
            if (typeof this.onMessage === 'function') {
                this.onMessage(msg);
            }
        };

        this.socket.onclose = () => {
            console.log('WebSocket connection closed. Reconnecting...');
            
            if (this.reconnectAttempts < this.maxReconnectAttempts) {
                this.reconnectAttempts++;
                setTimeout(() => this.connect(), 3000);
            }
        };

        this.socket.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
    }

    /**
     * Отправка сообщения через WebSocket
     * @param {string} type - Тип сообщения
     * @param {Object} data - Данные сообщения
     */
    send(type, data = {}) {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify({ type, ...data }));
        }
    }

    /**
     * Обработчик сообщений
     * @param {Object} msg - Полученное сообщение
     */
    onMessage(msg) {
        // По умолчанию ничего не делаем
    }

    /**
     * Отправка ping для keep-alive
     */
    ping() {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify({ type: 'ping' }));
        }
    }

    /**
     * Запуск keep-alive интервала
     * @param {number} interval - Интервал в миллисекундах
     */
    startKeepAlive(interval = 30000) {
        if (this.keepAliveInterval) {
            clearInterval(this.keepAliveInterval);
        }
        this.keepAliveInterval = setInterval(() => this.ping(), interval);
    }

    /**
     * Остановка keep-alive интервала
     */
    stopKeepAlive() {
        if (this.keepAliveInterval) {
            clearInterval(this.keepAliveInterval);
            this.keepAliveInterval = null;
        }
    }

    /**
     * Отключение от WebSocket сервера
     */
    disconnect() {
        this.stopKeepAlive();
        if (this.socket) {
            this.socket.close();
            this.socket = null;
        }
    }

    /**
     * Проверка состояния соединения
     * @returns {boolean} true если соединение активно
     */
    isConnected() {
        return this.socket && this.socket.readyState === WebSocket.OPEN;
    }
}

export { SocketManager };
