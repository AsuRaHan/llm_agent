/**
 * Agent Controller
 * Основной контроллер AI-агента
 */

class Agent {
    constructor(wsManager, sessionManager, messageRenderer, widgetRenderer, inputHandler, storage) {
        this.wsManager = wsManager;
        this.sessionManager = sessionManager;
        this.messageRenderer = messageRenderer;
        this.widgetRenderer = widgetRenderer;
        this.inputHandler = inputHandler;
        this.storage = storage;
        
        this.isRunning = false;
        this.currentSession = null;
        this.pendingConfirmation = null;
        this.pendingError = null;
        
        // Инициализация событий WebSocket
        this.initWebSocketEvents();
        
        // Инициализация событий виджетов
        this.initWidgetEvents();
    }

    /**
     * Инициализация событий WebSocket
     */
    initWebSocketEvents() {
        // Событие подключения
        this.wsManager.on('connected', () => {
            console.log('Agent: WebSocket подключен');
            this.isRunning = true;
        });

        // Событие отключения
        this.wsManager.on('disconnected', () => {
            console.log('Agent: WebSocket отключен');
            this.isRunning = false;
        });

        // Событие ошибки
        this.wsManager.on('error', (error) => {
            console.error('Agent: WebSocket ошибка:', error);
            this.showErrorToUser('Ошибка подключения: ' + error.message);
        });

        // Событие получения сообщения
        this.wsManager.on('message', (data) => {
            this.handleWebSocketMessage(data);
        });
    }

    /**
     * Инициализация событий виджетов
     */
    initWidgetEvents() {
        // Подтверждение действия
        this.widgetRenderer.on('confirmation', (data) => {
            this.handleConfirmation(data);
        });

        // Восстановление после ошибки
        this.widgetRenderer.on('error_recovery', (data) => {
            this.handleRecovery(data);
        });

        // Утверждение плана
        this.widgetRenderer.on('plan_approved', (data) => {
            this.handlePlanApproved(data);
        });

        // Редактирование плана
        this.widgetRenderer.on('plan_edit', (data) => {
            this.handlePlanEdit(data);
        });
    }

    /**
     * Обработка сообщения от WebSocket
     */
    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'session_state':
                this.handleSessionState(data.data);
                break;
            case 'agent_thought':
                this.handleAgentThought(data.data);
                break;
            case 'llm_token':
                this.handleLlmToken(data.data);
                break;
            case 'stream_end':
                this.handleStreamEnd();
                break;
            case 'action_required':
                this.handleActionRequired(data.data);
                break;
            case 'step_error':
                this.handleStepError(data.data);
                break;
            case 'query_response':
                this.handleQueryResponse(data.data);
                break;
            case 'error':
                this.handleError(data.data);
                break;
            default:
                console.warn('Неизвестный тип сообщения:', data.type);
        }
    }

    /**
     * Обработка состояния сессии
     */
    handleSessionState(data) {
        if (data.history) {
            this.messageRenderer.clear();
            this.messageRenderer.renderHistory([data]);
        }
        
        if (data.greeting) {
            this.showWelcomeMessage(data.greeting);
        }
    }

    /**
     * Обработка мыслей агента
     */
    handleAgentThought(data) {
        const thoughtSection = document.getElementById('agent-thought-section');
        const thoughtText = document.getElementById('agent-thought-text');
        
        if (thoughtSection && thoughtText) {
            thoughtText.textContent = data.message;
            thoughtSection.classList.remove('hidden');
        }
    }

    /**
     * Обработка токенов LLM
     */
    handleLlmToken(data) {
        const lastMessage = this.messageRenderer.getLastMessage();
        if (lastMessage) {
            const messageElement = document.querySelector(`[data-message-id="${lastMessage}"]`);
            if (messageElement) {
                const contentDiv = messageElement.querySelector('.message-content');
                if (contentDiv) {
                    const token = data.token;
                    // Добавление токена (можно реализовать стриминг)
                    contentDiv.textContent += token;
                    // Авто-скролл
                    this.autoScroll();
                }
            }
        }
    }

    /**
     * Обработка конца потока
     */
    handleStreamEnd() {
        const thoughtSection = document.getElementById('agent-thought-section');
        if (thoughtSection) {
            thoughtSection.classList.add('hidden');
        }
        
        // Показать сообщение о завершении
        this.messageRenderer.renderAgentMessage('Ответ сгенерирован', 'stream_end');
    }

    /**
     * Обработка требования подтверждения
     */
    handleActionRequired(data) {
        this.pendingConfirmation = data;
        this.widgetRenderer.showConfirmation(data);
    }

    /**
     * Обработка ошибки шага
     */
    handleStepError(data) {
        this.pendingError = data;
        this.widgetRenderer.showError(data);
    }

    /**
     * Обработка ответа на запрос
     */
    handleQueryResponse(data) {
        this.messageRenderer.renderAgentMessage(data.answer, 'text');
    }

    /**
     * Обработка ошибки
     */
    handleError(data) {
        this.showErrorToUser(data.message);
    }

    /**
     * Обработка подтверждения
     */
    handleConfirmation(data) {
        if (data.confirmed) {
            // Пользователь подтвердил
            this.wsManager.send('confirm_action', {
                session_id: this.currentSession.id,
                data: { confirmed: true }
            });
        } else {
            // Пользователь отклонил
            this.wsManager.send('confirm_action', {
                session_id: this.currentSession.id,
                data: { confirmed: false }
            });
        }
    }

    /**
     * Обработка восстановления
     */
    handleRecovery(data) {
        if (data.option === 'retry') {
            this.wsManager.send('confirm_error_recovery', {
                session_id: this.currentSession.id,
                data: { option: 'retry' }
            });
        } else if (data.option === 'skip') {
            this.wsManager.send('confirm_error_recovery', {
                session_id: this.currentSession.id,
                data: { option: 'skip' }
            });
        } else {
            this.wsManager.send('confirm_error_recovery', {
                session_id: this.currentSession.id,
                data: { option: 'abort' }
            });
        }
    }

    /**
     * Обработка утверждения плана
     */
    handlePlanApproved(data) {
        this.wsManager.send('execute_plan', {
            session_id: this.currentSession.id,
            data: { plan: data.plan }
        });
    }

    /**
     * Обработка редактирования плана
     */
    handlePlanEdit(data) {
        // Открыть редактор плана
        console.log('Редактирование плана:', data.plan);
    }

    /**
     * Обработка подключения WebSocket
     */
    connect() {
        this.wsManager.connect();
    }

    /**
     * Отправка запроса
     */
    sendQuery(text, images = []) {
        if (!this.currentSession) {
            this.createSession();
        }

        // Отправка через WebSocket
        this.wsManager.send('query', {
            session_id: this.currentSession.id,
            data: {
                text: text,
                images: images
            }
        });

        // Добавление сообщения в историю
        this.messageRenderer.renderUserMessage(text, images);
    }

    /**
     * Создание новой сессии
     */
    createSession(title = 'Новый чат') {
        this.currentSession = this.sessionManager.createSession(title);
        this.storage.saveCurrentSession(this.currentSession);
        
        // Очистка сообщений
        this.messageRenderer.clear();
        
        // Приветственное сообщение
        this.showWelcomeMessage('Привет! Я готов помочь. Чем могу быть полезен?');
    }

    /**
     * Переключение на существующую сессию
     */
    loadSession(sessionId) {
        const session = this.sessionManager.getSession(sessionId);
        if (session) {
            this.currentSession = session;
            this.storage.saveCurrentSession(this.currentSession);
            
            // Рендеринг истории
            this.messageRenderer.clear();
            this.messageRenderer.renderHistory([session]);
        }
    }

    /**
     * Удаление сессии
     */
    deleteSession(sessionId) {
        if (confirm('Вы уверены, что хотите удалить этот чат?')) {
            this.sessionManager.deleteSession(sessionId);
            if (this.currentSession && this.currentSession.id === sessionId) {
                this.createSession();
            }
        }
    }

    /**
     * Очистка текущей сессии
     */
    clearSession() {
        if (this.currentSession) {
            this.sessionManager.clearSession(this.currentSession.id);
            this.messageRenderer.clear();
            this.showWelcomeMessage('Чат очищен. Чем могу помочь?');
        }
    }

    /**
     * Показ приветственного сообщения
     */
    showWelcomeMessage(text) {
        this.messageRenderer.renderAgentMessage(text, 'text');
    }

    /**
     * Отображение ошибки пользователю
     */
    showErrorToUser(message) {
        this.messageRenderer.renderAgentMessage(message, 'error');
    }

    /**
     * Авто-скролл к последнему сообщению
     */
    autoScroll() {
        const messagesContainer = document.getElementById('messages-container');
        if (messagesContainer) {
            messagesContainer.scrollTop = messagesContainer.scrollHeight;
        }
    }

    /**
     * Прерывание текущего процесса
     */
    interrupt() {
        if (this.currentSession) {
            this.sessionManager.setInterrupted(this.currentSession.id, true);
            this.wsManager.send('interrupt_agent', {
                session_id: this.currentSession.id
            });
        }
    }

    /**
     * Начало работы
     */
    start() {
        if (!this.isRunning) {
            this.connect();
            this.isRunning = true;
        }
    }

    /**
     * Остановка
     */
    stop() {
        this.isRunning = false;
        if (this.currentSession) {
            this.sessionManager.setInterrupted(this.currentSession.id, true);
        }
        this.wsManager.disconnect();
    }

    /**
     * Получение текущей сессии
     */
    getCurrentSession() {
        return this.currentSession;
    }

    /**
     * Проверка: агент запущен
     */
    isRunning() {
        return this.isRunning;
    }
}

// Экспорт для использования в других модулях
if (typeof module !== 'undefined' && module.exports) {
    module.exports = Agent;
}
