/**
 * Smart Hammer Agent
 * Главный класс, связывающий все модули вместе
 * Vanilla JS + CommonJS
 */

class Agent {
    constructor(wsManager, sessionManager, messageRenderer, widgetRenderer, inputHandler, storage) {
        this.wsManager = wsManager;
        this.sessionManager = sessionManager;
        this.messageRenderer = messageRenderer;
        this.widgetRenderer = widgetRenderer;
        this.inputHandler = inputHandler;
        this.storage = storage;
        
        this.currentSession = null;
        this.isRunning = false;
        this.thoughtText = '';
        this.thoughtSection = null;
        this.thoughtTimer = null;
    }

    /**
     * Начало работы агента
     */
    start() {
        console.log('🚀 Smart Hammer Agent starting...');

        // Загрузка сессий
        this.loadSessions();

        // Создание новой сессии
        this.currentSession = this.sessionManager.createSession('Новый чат');

        // Инициализация input handler
        this.initInputHandler();

        // Подписка на события WebSocket
        this.subscribeToWebSocketEvents();

        // Обновление статуса подключения
        this.updateConnectionStatus();

        // Обработка событий виджетов
        this.setupWidgetEventHandlers();

        // Обработка кликов по кнопкам
        this.setupButtonHandlers();

        // Обработка загрузки сессий из localStorage
        this.setupSessionHandlers();

        console.log('✅ Smart Hammer Agent started successfully');
    }

    /**
     * Инициализация input handler
     */
    initInputHandler() {
        const inputElement = document.getElementById('message-input');
        const fileInput = document.getElementById('file-input');
        const imageInput = document.createElement('input');
        imageInput.type = 'file';
        imageInput.accept = 'image/*';
        imageInput.multiple = true;
        imageInput.id = 'image-input';
        document.querySelector('.toolbar').appendChild(imageInput);

        this.inputHandler.init(inputElement, fileInput, imageInput);
    }

    /**
     * Подписка на события WebSocket
     */
    subscribeToWebSocketEvents() {
        this.wsManager.on('connected', () => {
            console.log('✅ WebSocket connected');
            this.updateConnectionStatus(true);
            this.showAgentMessage('🎉 Подключено к серверу! Готов к работе.', 'text');
        });

        this.wsManager.on('disconnected', () => {
            console.log('⚠️ WebSocket disconnected');
            this.updateConnectionStatus(false);
            this.showAgentMessage('⚠️ Потеряно соединение. Попробуйте перезагрузить страницу.', 'error');
        });

        this.wsManager.on('error', (error) => {
            console.error('❌ WebSocket error:', error);
            this.updateConnectionStatus(false);
            this.showAgentMessage('❌ Ошибка соединения: ' + error.message, 'error');
        });

        this.wsManager.on('message', (data) => {
            console.log('📨 Получено сообщение от сервера:', data);
            this.handleServerMessage(data);
        });
    }

    /**
     * Обновление статуса подключения
     */
    updateConnectionStatus(isConnected) {
        const dot = document.getElementById('connection-status-dot');
        const text = document.getElementById('connection-status-text');

        if (isConnected) {
            dot.className = 'status-dot status-connected';
            text.textContent = 'Подключено';
        } else {
            dot.className = 'status-dot status-disconnected';
            text.textContent = 'Подключение...';
        }
    }

    /**
     * Обработка сообщений от сервера
     */
    handleServerMessage(data) {
        switch (data.type) {
            case 'query_response':
                this.showAgentMessage(data.content, data.type || 'text');
                break;

            case 'confirmation':
                this.handleConfirmation(data);
                break;

            case 'error_recovery':
                this.handleErrorRecovery(data);
                break;

            case 'plan_approved':
                this.executePlan(data.plan);
                break;

            case 'plan_edit':
                this.editPlan(data.plan);
                break;

            case 'error':
                this.showError(data);
                break;

            case 'progress':
                this.showProgress(data.steps);
                break;

            case 'stream_end':
                this.showStreamEnd();
                break;

            default:
                console.log('Unknown message type:', data.type);
        }
    }

    /**
     * Обработка подтверждения действия
     */
    handleConfirmation(data) {
        if (data.confirmed) {
            console.log('✅ Действие подтверждено:', data.data);
            this.wsManager.send('confirm_action', data.data);
        } else {
            console.log('❌ Действие отклонено:', data.data);
        }
    }

    /**
     * Обработка восстановления после ошибки
     */
    handleErrorRecovery(data) {
        if (data.recovered) {
            console.log('✅ Ошибка восстановлена:', data.data);
            this.wsManager.send('recover_error', data.data);
        }
    }

    /**
     * Выполнение плана
     */
    executePlan(plan) {
        console.log('🚀 Выполнение плана:', plan);
        this.isRunning = true;
        this.showAgentMessage('🚀 Запуск плана...', 'text');
        this.startThought('Выполнение плана...');

        this.wsManager.send('execute_plan', plan);
    }

    /**
     * Редактирование плана
     */
    editPlan(plan) {
        console.log('✏️ Редактирование плана:', plan);
        this.showAgentMessage('✏️ План отредактирован. Отправлен на сервер.', 'text');
        this.wsManager.send('edit_plan', plan);
    }

    /**
     * Показ сообщения агента
     */
    showAgentMessage(content, type = 'text') {
        this.messageRenderer.renderAgentMessage(content, type);
        this.messageRenderer.scrollToBottom();
    }

    /**
     * Показ ошибки
     */
    showError(data) {
        this.widgetRenderer.showError(data);
    }

    /**
     * Показ прогресса
     */
    showProgress(steps) {
        this.widgetRenderer.showProgress(steps);
    }

    /**
     * Показ конца потока
     */
    showStreamEnd() {
        const template = document.getElementById('stream-end-widget-template');
        if (template) {
            const widget = template.content.cloneNode(true);
            this.messagesContainer.appendChild(widget);
            this.messageRenderer.scrollToBottom();
        }
    }

    /**
     * Показ мыслей агента
     */
    startThought(text) {
        this.thoughtText = text;
        if (this.thoughtSection) {
            document.getElementById('agent-thought-text').textContent = text;
        } else {
            this.thoughtSection = document.getElementById('agent-thought-section');
            this.thoughtSection.classList.remove('hidden');
        }

        // Обновление текста мыслей
        this.thoughtTimer = setInterval(() => {
            if (this.thoughtText) {
                document.getElementById('agent-thought-text').textContent = this.thoughtText;
            }
        }, 1000);
    }

    /**
     * Прерывание мыслей агента
     */
    interruptThought() {
        if (this.thoughtTimer) {
            clearInterval(this.thoughtTimer);
            this.thoughtTimer = null;
        }
        this.startThought('⏸️ Агент прерван пользователем');
    }

    /**
     * Инициализация обработчиков кнопок
     */
    setupButtonHandlers() {
        // Кнопка отправки
        document.getElementById('send-btn').addEventListener('click', () => {
            this.handleSendClick();
        });

        // Кнопка очистки ввода
        document.getElementById('clear-input-btn').addEventListener('click', () => {
            this.inputHandler.handleClearClick();
        });

        // Кнопка прикрепления файла
        document.getElementById('attach-file-btn').addEventListener('click', () => {
            document.getElementById('file-input').click();
        });

        // Кнопка нового чата
        document.getElementById('new-chat-btn').addEventListener('click', () => {
            this.createNewChat();
        });

        // Кнопка очистки истории
        document.getElementById('clear-history-btn').addEventListener('click', () => {
            this.clearHistory();
        });

        // Кнопка очистки сессии
        document.getElementById('clear-session-btn').addEventListener('click', () => {
            this.clearSession();
        });

        // Кнопка прерывания
        document.getElementById('interrupt-btn').addEventListener('click', () => {
            this.interruptThought();
        });

        // Обработчик кликов по виджетам
        document.addEventListener('click', (event) => {
            this.inputHandler.handleWidgetClick(event);
        });

        // Обработчик Enter
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Enter' && event.target === document.getElementById('message-input')) {
                event.preventDefault();
                this.handleSendClick();
            }
        });
    }

    /**
     * Обработка клика по кнопке отправки
     */
    handleSendClick() {
        const inputElement = document.getElementById('message-input');
        const text = inputElement.value.trim();

        if (!text) return;

        // Получение изображений
        const images = this.inputHandler.getImageFiles();

        // Создание сообщения
        const message = {
            role: 'user',
            content: text,
            images: images
        };

        // Отправка сообщения
        this.wsManager.send('query', message);

        // Очистка ввода
        inputElement.value = '';
        inputElement.style.height = 'auto';
        document.getElementById('send-btn').disabled = true;

        // Скрытие изображений
        document.getElementById('image-preview-container').classList.add('hidden');
    }

    /**
     * Создание нового чата
     */
    createNewChat() {
        this.sessionManager.deleteSession(this.currentSession.id);
        this.currentSession = this.sessionManager.createSession('Новый чат');
        this.loadSessions();
        this.showAgentMessage('📝 Новый чат создан', 'text');
    }

    /**
     * Очистка истории
     */
    clearHistory() {
        this.sessionManager.clearAllSessions();
        this.loadSessions();
        this.showAgentMessage('🗑️ История очищена', 'text');
    }

    /**
     * Очистка сессии
     */
    clearSession() {
        if (this.currentSession) {
            this.sessionManager.clearSession(this.currentSession.id);
            this.currentSession = null;
            this.loadSessions();
            this.showAgentMessage('🗑️ Сессия очищена', 'text');
        }
    }

    /**
     * Загрузка сессий
     */
    loadSessions() {
        const sessions = this.sessionManager.loadSessions();
        this.renderChatList(sessions);
    }

    /**
     * Рендеринг списка чатов
     */
    renderChatList(sessions) {
        const chatList = document.getElementById('chat-list');
        chatList.innerHTML = '';

        sessions.forEach(session => {
            const li = document.createElement('li');
            li.className = 'chat-item group flex items-center justify-between p-2.5 rounded-lg cursor-pointer hover:bg-white/5 transition-all';
            li.dataset.sessionId = session.id;

            li.innerHTML = `
                <div class="flex items-center gap-3 flex-grow">
                    <div class="w-8 h-8 rounded-lg bg-gradient-to-br from-blue-500 to-indigo-600 flex items-center justify-center flex-shrink-0">
                        <i class="fas fa-robot text-white text-sm"></i>
                    </div>
                    <span class="chat-title truncate text-sm font-medium">${session.title || 'Без названия'}</span>
                </div>
                <button class="delete-chat-btn group-hover:block text-gray-500 hover:text-red-500 text-xs p-1" title="Удалить чат">
                    <i class="fas fa-trash"></i>
                </button>
            `;

            li.addEventListener('click', () => {
                this.loadSession(session.id);
            });

            li.querySelector('.delete-chat-btn').addEventListener('click', () => {
                this.deleteChat(session.id);
            });

            chatList.appendChild(li);
        });
    }

    /**
     * Загрузка сессии
     */
    loadSession(sessionId) {
        const session = this.sessionManager.getSession(sessionId);
        if (session) {
            this.currentSession = session;
            this.loadMessages(session);
            this.updateChatList();
        }
    }

    /**
     * Загрузка сообщений сессии
     */
    loadMessages(session) {
        const messages = this.sessionManager.getHistory(session.id);
        this.messagesContainer.innerHTML = '';

        messages.forEach(msg => {
            if (msg.role === 'user') {
                this.messageRenderer.renderUserMessage(msg.content, msg.images || []);
            } else if (msg.role === 'agent') {
                this.messageRenderer.renderAgentMessage(msg.content, msg.type || 'text');
            }
        });

        this.messageRenderer.scrollToBottom();
    }

    /**
     * Обновление списка чатов
     */
    updateChatList() {
        this.loadSessions();
    }

    /**
     * Удаление чата
     */
    deleteChat(sessionId) {
        this.sessionManager.deleteSession(sessionId);
        this.loadSessions();
        this.showAgentMessage('🗑️ Чат удалён', 'text');
    }

    /**
     * Установка текущей сессии
     */
    setCurrentSession(sessionId) {
        this.sessionManager.setCurrentSession(sessionId);
        this.loadSession(sessionId);
    }

    /**
     * Добавление сообщения в сессию
     */
    addMessageToSession(role, content, images = []) {
        if (this.currentSession) {
            this.sessionManager.addMessage(this.currentSession.id, {
                role,
                content,
                images
            });
        }
    }

    /**
     * Отправка сообщения на сервер
     */
    sendMessage(message) {
        this.wsManager.send('query', message);
    }

    /**
     * Подтверждение действия
     */
    confirmAction(data) {
        this.wsManager.send('confirm_action', data);
    }

    /**
     * Отклонение действия
     */
    rejectAction(data) {
        this.wsManager.send('reject_action', data);
    }

    /**
     * Выполнение плана
     */
    executePlan(plan) {
        this.wsManager.send('execute_plan', plan);
    }

    /**
     * Редактирование плана
     */
    editPlan(plan) {
        this.wsManager.send('edit_plan', plan);
    }

    /**
     * Восстановление после ошибки
     */
    recoverFromError(data) {
        this.wsManager.send('recover_error', data);
    }

    /**
     * Получение текущей сессии
     */
    getCurrentSession() {
        return this.currentSession;
    }

    /**
     * Получение истории сообщений
     */
    getHistory() {
        if (this.currentSession) {
            return this.sessionManager.getHistory(this.currentSession.id);
        }
        return [];
    }

    /**
     * Консольный логгер
     */
    log(message) {
        console.log(`[Smart Hammer Agent] ${message}`);
    }
}

// Экспорт через CommonJS
module.exports = Agent;
