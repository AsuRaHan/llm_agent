/**
 * @typedef {object} SocketOptions
 * @property {number} [reconnectAttempts=5] Максимальное количество попыток переподключения.
 * @property {number} [reconnectDelay=3000] Задержка перед переподключением в мс.
 * @property {number} [keepAliveInterval=30000] Интервал отправки ping-сообщений для поддержания соединения в мс.
 */

/**
 * Управляет WebSocket-соединением, включая переподключение и keep-alive.
 */
class SocketManager {
    /**
     * @param {string} url URL WebSocket-сервера.
     * @param {SocketOptions} [options] Опции для SocketManager.
     */
    constructor(url, options = {}) {
        this.url = url;
        this.options = {
            reconnectAttempts: 5,
            reconnectDelay: 3000,
            keepAliveInterval: 30000, // 30 секунд
            ...options
        };

        this.ws = null;
        this.reconnectCount = 0;
        this.messageHandlers = [];
        this.errorHandlers = [];
        this.closeHandlers = [];
        this.openHandlers = [];
        this.keepAliveTimer = null;
        this.isConnecting = false;
        this.shouldReconnect = true; // Флаг для управления автоматическим переподключением
    }

    /**
     * Устанавливает WebSocket-соединение.
     * @returns {Promise<void>} Промис, который разрешается при успешном подключении.
     */
    connect() {
        if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
            console.warn('WebSocket уже подключен или находится в процессе подключения.');
            return Promise.resolve();
        }

        this.isConnecting = true;
        this.shouldReconnect = true;
        console.log(`Попытка подключения к WebSocket: ${this.url}`);

        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);

            this.ws.onopen = () => {
                console.log('WebSocket подключен.');
                this.isConnecting = false;
                this.reconnectCount = 0;
                this.startKeepAlive();
                this.openHandlers.forEach(handler => handler());
                resolve();
            };

            this.ws.onmessage = (event) => {
                try {
                    const message = JSON.parse(event.data);
                    this.messageHandlers.forEach(handler => handler(message));
                } catch (e) {
                    console.error('Ошибка парсинга сообщения WebSocket:', e, event.data);
                    this.errorHandlers.forEach(handler => handler(e));
                }
            };

            this.ws.onclose = (event) => {
                console.warn('WebSocket закрыт:', event.code, event.reason);
                this.isConnecting = false;
                this.stopKeepAlive();
                this.closeHandlers.forEach(handler => handler(event));
                if (this.shouldReconnect && this.reconnectCount < this.options.reconnectAttempts) {
                    this.reconnectCount++;
                    const delay = this.options.reconnectDelay * Math.pow(2, this.reconnectCount - 1); // Экспоненциальная задержка
                    console.log(`Попытка переподключения через ${delay} мс (попытка ${this.reconnectCount}/${this.options.reconnectAttempts})...`);
                    setTimeout(() => this.connect().then(resolve).catch(reject), delay);
                } else if (this.shouldReconnect) {
                    console.error('Достигнут максимальный лимит попыток переподключения.');
                    const error = new Error('Достигнут максимальный лимит попыток переподключения.');
                    this.errorHandlers.forEach(handler => handler(error));
                    reject(error);
                }
            };

            this.ws.onerror = (event) => {
                console.error('Ошибка WebSocket:', event);
                this.isConnecting = false;
                const error = new Error('Ошибка WebSocket');
                this.errorHandlers.forEach(handler => handler(error));
                reject(error);
            };
        });
    }

    /**
     * Отключает WebSocket-соединение.
     */
    disconnect() {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.shouldReconnect = false; // Отключаем автоматическое переподключение при явном отключении
            this.ws.close();
            console.log('WebSocket отключен.');
        }
    }

    /**
     * Отправляет сообщение через WebSocket.
     * @param {object} message Сообщение для отправки.
     */
    send(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        } else {
            console.warn('WebSocket не подключен. Сообщение не отправлено:', message);
            // Можно добавить логику для буферизации сообщений
        }
    }

    /**
     * Регистрирует обработчик для входящих сообщений.
     * @param {(message: object) => void} callback Функция-обработчик.
     */
    onMessage(callback) {
        this.messageHandlers.push(callback);
    }

    /**
     * Удаляет обработчик входящих сообщений.
     * @param {(message: object) => void} callback Функция-обработчик.
     */
    offMessage(callback) {
        this.messageHandlers = this.messageHandlers.filter(handler => handler !== callback);
    }

    /**
     * Регистрирует обработчик для ошибок соединения.
     * @param {(error: Error) => void} callback Функция-обработчик.
     */
    onError(callback) {
        this.errorHandlers.push(callback);
    }

    /**
     * Регистрирует обработчик для события открытия соединения.
     * @param {() => void} callback Функция-обработчик.
     */
    onOpen(callback) {
        this.openHandlers.push(callback);
    }

    /**
     * Регистрирует обработчик для события закрытия соединения.
     * @param {(event: CloseEvent) => void} callback Функция-обработчик.
     */
    onClose(callback) {
        this.closeHandlers.push(callback);
    }

    /**
     * Запускает механизм keep-alive для поддержания соединения.
     */
    startKeepAlive() {
        if (this.options.keepAliveInterval > 0) {
            this.stopKeepAlive(); // Очищаем предыдущий таймер, если есть
            this.keepAliveTimer = setInterval(() => {
                if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                    this.send({ type: 'ping' });
                }
            }, this.options.keepAliveInterval);
        }
    }

    /**
     * Останавливает механизм keep-alive.
     */
    stopKeepAlive() {
        if (this.keepAliveTimer) {
            clearInterval(this.keepAliveTimer);
            this.keepAliveTimer = null;
        }
    }

    /**
     * Проверяет, активно ли соединение.
     * @returns {boolean} True, если соединение открыто.
     */
    isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }

    /**
     * Возвращает текущее количество попыток переподключения.
     * @returns {number}
     */
    getReconnectAttempts() {
        return this.reconnectCount;
    }

    /**
     * Устанавливает максимальное количество попыток переподключения.
     * @param {number} max
     */
    setReconnectAttempts(max) {
        this.options.reconnectAttempts = max;
    }
}

export { SocketManager };