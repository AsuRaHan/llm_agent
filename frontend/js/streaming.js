/**
 * Управляет потоковой передачей токенов от LLM.
 */
class StreamingManager {
    /**
     * @param {SocketManager} socketManager Экземпляр SocketManager для взаимодействия с WebSocket.
     */
    constructor(socketManager) {
        this.socketManager = socketManager;
        this.isStreaming = false;
        this.accumulatedText = '';
        this.tokenHandlers = [];
        this.completeHandlers = [];
        this.errorHandlers = [];

        // Подписываемся на сообщения от SocketManager
        this.socketManager.onMessage(this._handleSocketMessage.bind(this));
    }

    /**
     * Обрабатывает входящие сообщения от WebSocket.
     * @param {object} message Сообщение от WebSocket.
     * @private
     */
    _handleSocketMessage(message) {
        switch (message.type) {
            case 'llm_token':
                if (this.isStreaming) {
                    const token = message.data.token;
                    this.accumulatedText += token;
                    this.tokenHandlers.forEach(handler => handler(token));
                }
                break;
            case 'stream_end':
                if (this.isStreaming) {
                    this.stopStreaming();
                    this.completeHandlers.forEach(handler => handler());
                }
                break;
            case 'error': // Предполагаем, что ошибки стриминга могут приходить как общий тип 'error'
                if (this.isStreaming) {
                    this.stopStreaming();
                    const error = new Error(message.data.message || 'Ошибка стриминга');
                    this.errorHandlers.forEach(handler => handler(error));
                }
                break;
            // Другие типы сообщений могут быть проигнорированы или обработаны в другом месте
        }
    }

    /**
     * Начинает процесс стриминга.
     */
    startStreaming() {
        this.isStreaming = true;
        this.clear();
        console.log('Стриминг начат.');
    }

    /**
     * Останавливает процесс стриминга.
     */
    stopStreaming() {
        this.isStreaming = false;
        console.log('Стриминг остановлен.');
    }

    /**
     * Регистрирует обработчик для каждого полученного токена.
     * @param {(token: string) => void} callback Функция-обработчик.
     */
    onToken(callback) { this.tokenHandlers.push(callback); }
    offToken(callback) { this.tokenHandlers = this.tokenHandlers.filter(h => h !== callback); }

    onComplete(callback) { this.completeHandlers.push(callback); }
    onError(callback) { this.errorHandlers.push(callback); }

    getAccumulatedText() { return this.accumulatedText; }
    clear() { this.accumulatedText = ''; }
    reset() { this.clear(); this.isStreaming = false; }
}

export { StreamingManager };