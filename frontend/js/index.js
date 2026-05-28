/**
 * Smart Hammer - Main Entry Point
 * Точка входа для фронтенда
 */

// Глобальный объект для доступа к модулям
window.smartHammer = {
    wsManager: null,
    sessionManager: null,
    messageRenderer: null,
    widgetRenderer: null,
    inputHandler: null,
    storage: null,
    agent: null,
    initialized: false
};

/**
 * Инициализация приложения
 */
function initApp() {
    console.log('Smart Hammer: Инициализация приложения...');
    
    // Проверка, инициализировано ли уже
    if (window.smartHammer.initialized) {
        console.log('Smart Hammer: Приложение уже инициализировано');
        return;
    }
    
    try {
        // Инициализация модулей
        initWebSocket();
        initSessionManager();
        initMessageRenderer();
        initWidgetRenderer();
        initInputHandler();
        initStorage();
        initAgent();
        
        // Устанавливаем флаг инициализации
        window.smartHammer.initialized = true;
        
        console.log('Smart Hammer: Приложение успешно инициализировано');
    } catch (error) {
        console.error('Smart Hammer: Ошибка инициализации:', error);
        showError('Ошибка инициализации приложения');
    }
}

/**
 * Инициализация WebSocket
 */
function initWebSocket() {
    const wsUrl = 'ws://localhost:9000/ws';
    console.log('Smart Hammer: Подключение к WebSocket:', wsUrl);
    
    window.smartHammer.wsManager = new WebSocketManager(wsUrl);
    
    // Слушатели событий WebSocket
    window.smartHammer.wsManager.on('connected', () => {
        console.log('Smart Hammer: WebSocket подключен');
        updateConnectionStatus('connected');
    });
    
    window.smartHammer.wsManager.on('disconnected', () => {
        console.log('Smart Hammer: WebSocket отключен');
        updateConnectionStatus('disconnected');
    });
    
    window.smartHammer.wsManager.on('error', (error) => {
        console.error('Smart Hammer: WebSocket ошибка:', error);
        updateConnectionStatus('disconnected');
    });
    
    window.smartHammer.wsManager.on('message', (data) => {
        console.log('Smart Hammer: Получено сообщение:', data.type);
        handleWebSocketMessage(data);
    });
    
    // Подключение
    window.smartHammer.wsManager.connect();
}

/**
 * Инициализация Session Manager
 */
function initSessionManager() {
    console.log('Smart Hammer: Инициализация Session Manager');
    window.smartHammer.sessionManager = new SessionManager();
    
    // Загрузка сессий
    const sessions = window.smartHammer.sessionManager.loadSessions();
    console.log('Smart Hammer: Загружено сессий:', sessions.length);
}

/**
 * Инициализация Message Renderer
 */
function initMessageRenderer() {
    console.log('Smart Hammer: Инициализация Message Renderer');
    const container = document.getElementById('messages-container');
    if (container) {
        window.smartHammer.messageRenderer = new MessageRenderer(container);
    }
}

/**
 * Инициализация Widget Renderer
 */
function initWidgetRenderer() {
    console.log('Smart Hammer: Инициализация Widget Renderer');
    const container = document.getElementById('messages-container');
    if (container) {
        window.smartHammer.widgetRenderer = new WidgetRenderer(container);
    }
}

/**
 * Инициализация Input Handler
 */
function initInputHandler() {
    console.log('Smart Hammer: Инициализация Input Handler');
    
    const inputElement = document.getElementById('message-input');
    const sendButtonElement = document.getElementById('send-btn');
    const previewContainerElement = document.getElementById('image-preview-container');
    
    if (inputElement && sendButtonElement && previewContainerElement) {
        window.smartHammer.inputHandler = new InputHandler(
            inputElement,
            sendButtonElement,
            previewContainerElement
        );
        
        // Инициализация file input
        const fileInput = document.getElementById('file-input');
        if (fileInput) {
            window.smartHammer.inputHandler.initFileInput(fileInput);
        }
        
        // Обработчики событий
        setupInputHandlers();
    }
}

/**
 * Инициализация Storage
 */
function initStorage() {
    console.log('Smart Hammer: Инициализация Storage');
    window.smartHammer.storage = new StorageManager();
}

/**
 * Инициализация Agent
 */
function initAgent() {
    console.log('Smart Hammer: Инициализация Agent');
    
    if (window.smartHammer.wsManager &&
        window.smartHammer.sessionManager &&
        window.smartHammer.messageRenderer &&
        window.smartHammer.widgetRenderer &&
        window.smartHammer.inputHandler &&
        window.smartHammer.storage) {
        
        window.smartHammer.agent = new Agent(
            window.smartHammer.wsManager,
            window.smartHammer.sessionManager,
            window.smartHammer.messageRenderer,
            window.smartHammer.widgetRenderer,
            window.smartHammer.inputHandler,
            window.smartHammer.storage
        );
        
        // Начало работы
        window.smartHammer.agent.start();
    }
}

/**
 * Настройка обработчиков событий ввода
 */
function setupInputHandlers() {
    const inputHandler = window.smartHammer.inputHandler;
    const inputElement = document.getElementById('message-input');
    const sendButton = document.getElementById('send-btn');
    
    // Авто-расширение
    inputHandler.on('input', (e) => {
        inputElement.style.height = 'auto';
        inputElement.style.height = Math.min(inputElement.scrollHeight, 200) + 'px';
    });
    
    // Отправка по Enter (Shift+Enter для новой строки)
    inputHandler.on('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            if (inputElement.value.trim()) {
                sendButton.click();
            }
        }
    });
    
    // Клик по кнопке отправки
    if (sendButton) {
        sendButton.addEventListener('click', () => {
            if (inputElement.value.trim()) {
                const data = inputHandler.send();
                sendQuery(data);
            }
        });
    }
    
    // Очистка ввода
    const clearInputBtn = document.getElementById('clear-input-btn');
    if (clearInputBtn) {
        clearInputBtn.addEventListener('click', () => {
            inputHandler.clear();
        });
    }
    
    // Прикрепление файла
    const attachFileBtn = document.getElementById('attach-file-btn');
    if (attachFileBtn) {
        attachFileBtn.addEventListener('click', () => {
            const fileInput = document.getElementById('file-input');
            if (fileInput) {
                fileInput.click();
            }
        });
    }
}

/**
 * Отправка запроса
 */
function sendQuery(data) {
    const text = data.text || '';
    const images = data.images || [];
    
    if (!text && images.length === 0) {
        console.warn('Smart Hammer: Пустой запрос');
        return;
    }
    
    // Отправка через WebSocket
    if (window.smartHammer.wsManager) {
        window.smartHammer.wsManager.send('query', {
            session_id: window.smartHammer.agent.getCurrentSession().id,
            data: {
                text: text,
                images: images
            }
        });
    }
    
    // Добавление сообщения в UI
    if (window.smartHammer.messageRenderer) {
        window.smartHammer.messageRenderer.renderUserMessage(text, images);
    }
}

/**
 * Обработка сообщений от WebSocket
 */
function handleWebSocketMessage(data) {
    console.log('Smart Hammer: Обработка сообщения:', data.type);
    
    switch (data.type) {
        case 'session_state':
            handleSessionState(data.data);
            break;
        case 'agent_thought':
            handleAgentThought(data.data);
            break;
        case 'llm_token':
            handleLlmToken(data.data);
            break;
        case 'stream_end':
            handleStreamEnd();
            break;
        case 'action_required':
            handleActionRequired(data.data);
            break;
        case 'step_error':
            handleStepError(data.data);
            break;
        case 'query_response':
            handleQueryResponse(data.data);
            break;
        case 'error':
            handleError(data.data);
            break;
        default:
            console.warn('Smart Hammer: Неизвестный тип сообщения:', data.type);
    }
}

/**
 * Обработка состояния сессии
 */
function handleSessionState(data) {
    if (data.history) {
        if (window.smartHammer.messageRenderer) {
            window.smartHammer.messageRenderer.clear();
            window.smartHammer.messageRenderer.renderHistory([data]);
        }
    }
    
    if (data.greeting) {
        if (window.smartHammer.messageRenderer) {
            window.smartHammer.messageRenderer.renderAgentMessage(data.greeting, 'text');
        }
    }
}

/**
 * Обработка мыслей агента
 */
function handleAgentThought(data) {
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
function handleLlmToken(data) {
    const lastMessage = window.smartHammer.messageRenderer ? 
        window.smartHammer.messageRenderer.getLastMessage() : null;
    
    if (lastMessage) {
        const messageElement = document.querySelector(`[data-message-id="${lastMessage}"]`);
        if (messageElement) {
            const contentDiv = messageElement.querySelector('.message-content');
            if (contentDiv) {
                contentDiv.textContent += data.token;
                autoScroll();
            }
        }
    }
}

/**
 * Обработка конца потока
 */
function handleStreamEnd() {
    const thoughtSection = document.getElementById('agent-thought-section');
    if (thoughtSection) {
        thoughtSection.classList.add('hidden');
    }
}

/**
 * Обработка требования подтверждения
 */
function handleActionRequired(data) {
    if (window.smartHammer.widgetRenderer) {
        window.smartHammer.widgetRenderer.showConfirmation(data);
    }
}

/**
 * Обработка ошибки шага
 */
function handleStepError(data) {
    if (window.smartHammer.widgetRenderer) {
        window.smartHammer.widgetRenderer.showError(data);
    }
}

/**
 * Обработка ответа на запрос
 */
function handleQueryResponse(data) {
    if (window.smartHammer.messageRenderer) {
        window.smartHammer.messageRenderer.renderAgentMessage(data.answer, 'text');
    }
}

/**
 * Обработка ошибки
 */
function handleError(data) {
    showError(data.message || 'Неизвестная ошибка');
}

/**
 * Обновление статуса подключения
 */
function updateConnectionStatus(status) {
    const statusDot = document.getElementById('connection-status-dot');
    const statusText = document.getElementById('connection-status-text');
    
    if (!statusDot || !statusText) return;
    
    switch (status) {
        case 'connected':
            statusDot.className = 'status-dot status-connected';
            statusText.textContent = 'Подключено';
            break;
        case 'disconnected':
            statusDot.className = 'status-dot status-disconnected';
            statusText.textContent = 'Отключено';
            break;
        case 'connecting':
            statusDot.className = 'status-dot status-connecting';
            statusText.textContent = 'Подключение...';
            break;
    }
}

/**
 * Авто-скролл к последнему сообщению
 */
function autoScroll() {
    const messagesContainer = document.getElementById('messages-container');
    if (messagesContainer) {
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }
}

/**
 * Отображение ошибки
 */
function showError(message) {
    const messagesContainer = document.getElementById('messages-container');
    if (messagesContainer) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'message agent';
        errorDiv.innerHTML = `
            <div class="message-content border-l-4 border-red-500 bg-red-500/10 p-4 rounded-r-lg">
                <p class="font-semibold text-red-400 mb-2">❌ Ошибка</p>
                <p class="text-sm text-gray-200">${message}</p>
            </div>
        `;
        messagesContainer.appendChild(errorDiv);
        autoScroll();
    }
}

/**
 * Обработчики кнопок
 */
function setupButtonHandlers() {
    // Новый чат
    const newChatBtn = document.getElementById('new-chat-btn');
    if (newChatBtn) {
        newChatBtn.addEventListener('click', () => {
            if (window.smartHammer.agent) {
                window.smartHammer.agent.createSession();
            }
        });
    }
    
    // Очистить историю
    const clearHistoryBtn = document.getElementById('clear-history-btn');
    if (clearHistoryBtn) {
        clearHistoryBtn.addEventListener('click', () => {
            if (window.smartHammer.agent && window.smartHammer.agent.getCurrentSession()) {
                if (confirm('Вы уверены, что хотите очистить всю историю чатов?')) {
                    window.smartHammer.agent.clearSession();
                }
            }
        });
    }
    
    // Очистить сессию
    const clearSessionBtn = document.getElementById('clear-session-btn');
    if (clearSessionBtn) {
        clearSessionBtn.addEventListener('click', () => {
            if (window.smartHammer.agent && window.smartHammer.agent.getCurrentSession()) {
                if (confirm('Вы уверены, что хотите очистить текущую сессию?')) {
                    window.smartHammer.agent.clearSession();
                }
            }
        });
    }
    
    // Авто-подтверждение
    const autoConfirmBtn = document.getElementById('auto-confirm-btn');
    if (autoConfirmBtn) {
        autoConfirmBtn.addEventListener('click', () => {
            console.log('Smart Hammer: Авто-подтверждение переключено');
        });
    }
    
    // Заморозка watcher
    const freezeWatcherBtn = document.getElementById('freeze-watcher-btn');
    if (freezeWatcherBtn) {
        freezeWatcherBtn.addEventListener('click', () => {
            console.log('Smart Hammer: Заморозка watcher переключена');
        });
    }
    
    // Прервать агент
    const interruptBtn = document.getElementById('interrupt-btn');
    if (interruptBtn) {
        interruptBtn.addEventListener('click', () => {
            if (window.smartHammer.agent) {
                window.smartHammer.agent.interrupt();
            }
        });
    }
    
    // Удаление чата
    const chatList = document.getElementById('chat-list');
    if (chatList) {
        chatList.addEventListener('click', (e) => {
            const deleteBtn = e.target.closest('.delete-chat-btn');
            if (deleteBtn) {
                const chatItem = deleteBtn.closest('.chat-item');
                const sessionId = chatItem.dataset.sessionId;
                if (sessionId) {
                    if (confirm('Вы уверены, что хотите удалить этот чат?')) {
                        window.smartHammer.agent.deleteSession(sessionId);
                    }
                }
            }
        });
    }
}

/**
 * Инициализация при загрузке страницы
 */
document.addEventListener('DOMContentLoaded', () => {
    console.log('Smart Hammer: DOM загружен');
    
    // Обновление статуса подключения
    updateConnectionStatus('disconnected');
    
    // Настройка обработчиков кнопок
    setupButtonHandlers();
    
    // Инициализация приложения
    initApp();
});

// Экспорт для использования в других модулях
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        initApp,
        WebSocketManager,
        SessionManager,
        MessageRenderer,
        WidgetRenderer,
        InputHandler,
        StorageManager,
        Agent
    };
}
