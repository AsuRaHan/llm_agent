/**
 * WebSocket Connection Manager
 * Управление подключением к WebSocket серверу
 * Vanilla JS + CommonJS
 */

class WebSocketManager {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 1000;
        this.listeners = new Map();
        this.isConnecting = false;
    }

    /**
     * Подключиться к WebSocket серверу
     */
    connect() {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            console.log('WebSocket уже подключен');
            return;
        }

        this.isConnecting = true;
        updateConnectionStatus('connecting');

        try {
            this.ws = new WebSocket(this.url);

            this.ws.onopen = () => {
                console.log('WebSocket подключен');
                this.isConnecting = false;
                this.reconnectAttempts = 0;
                updateConnectionStatus('connected');
                this.emit('connected');
            };

            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.emit('message', data);
                } catch (e) {
                    console.error('Ошибка парсинга JSON:', e);
                }
            };

            this.ws.onclose = () => {
                console.log('WebSocket закрыт');
                this.isConnecting = false;
                updateConnectionStatus('disconnected');
                this.emit('disconnected');

                // Попытка переподключения
                if (this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.reconnectAttempts++;
                    console.log(`Переподключение (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`);
                    setTimeout(() => this.connect(), this.reconnectDelay * this.reconnectAttempts);
                }
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket ошибка:', error);
                this.isConnecting = false;
                updateConnectionStatus('disconnected');
                this.emit('error', error);
            };
        } catch (e) {
            console.error('Ошибка подключения WebSocket:', e);
            this.isConnecting = false;
            updateConnectionStatus('disconnected');
            this.emit('error', e);
        }
    }

    /**
     * Отправить сообщение
     */
    send(type, data) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            console.warn('WebSocket не подключен, сообщение не отправлено');
            return false;
        }

        const message = {
            type,
            session_id: getSessionId(),
            data
        };

        try {
            this.ws.send(JSON.stringify(message));
            return true;
        } catch (e) {
            console.error('Ошибка отправки сообщения:', e);
            return false;
        }
    }

    /**
     * Подписаться на события
     */
    on(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        this.listeners.get(event).push(callback);
    }

    /**
     * Отписаться от событий
     */
    off(event, callback) {
        const callbacks = this.listeners.get(event);
        if (callbacks) {
            const index = callbacks.indexOf(callback);
            if (index > -1) {
                callbacks.splice(index, 1);
            }
        }
    }

    /**
     * Эмитить событие
     */
    emit(event, data) {
        const callbacks = this.listeners.get(event);
        if (callbacks) {
            callbacks.forEach(callback => callback(data));
        }
    }

    /**
     * Отключиться
     */
    disconnect() {
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        this.isConnecting = false;
        updateConnectionStatus('disconnected');
    }

    /**
     * Проверка статуса подключения
     */
    isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }

    /**
     * Получить количество попыток переподключения
     */
    getAttempts() {
        return this.reconnectAttempts;
    }

    /**
     * Установить количество попыток переподключения
     */
    setAttempts(value) {
        this.maxReconnectAttempts = value;
    }

    /**
     * Переподключиться
     */
    reconnect() {
        this.connect();
    }

    /**
     * Закрыть соединение
     */
    close() {
        this.disconnect();
    }
}

// Экспорт через CommonJS
module.exports = WebSocketManager;
