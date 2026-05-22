document.addEventListener('DOMContentLoaded', async () => {
    // --- Импорты модулей ---
    // Пути указаны относительно /js/
    const { SocketManager } = await import('./socket.js');
    const { StreamingManager } = await import('./streaming.js');
    const { SessionManager } = await import('./session.js');
    const { MessageRenderer } = await import('./message.js');
    const { StreamingIndicator } = await import('./streaming-indicator.js');
    const { TokenAccumulator } = await import('./token-accumulator.js');

    // --- DOM Элементы ---
    const messageList = document.getElementById('message-list');
    const messageInput = document.getElementById('message-input');
    const sendButton = document.getElementById('send-button');
    const planButton = document.getElementById('plan-button');
    const connectionStatusSpan = document.getElementById('connection-status');
    const thoughtContainer = document.getElementById('agent-thought-container');
    const thoughtText = document.getElementById('agent-thought-text');
    const indicatorContainer = document.getElementById('indicator-container');
    const chatForm = document.getElementById('chat-form');
    const autoConfirmButton = document.getElementById('auto-confirm-button');
    const freezeWatcherButton = document.getElementById('freeze-watcher-button');
    const clearHistoryButton = document.getElementById('clear-history-button');

    // --- Состояние приложения ---
    let state = {
        isAutoConfirmEnabled: false,
        isWatcherFrozen: false,
    };
    let streamingMessageElement = null; // Для хранения ссылки на DOM-элемент стриминга

    // --- Инициализация менеджеров ---
    const socketManager = new SocketManager(`ws://${window.location.host}/ws`, {
        reconnectAttempts: 5,
        reconnectDelay: 3000,
    });

    // Используем ваш SessionManager для управления ID сессии
    const sessionManager = new SessionManager(socketManager, 'llm_agent_');

    // StreamingManager будет обрабатывать llm_token и stream_end
    const streamingManager = new StreamingManager(socketManager);

    // MessageRenderer для отрисовки сообщений
    const messageRenderer = new MessageRenderer(messageList, { useMarkdown: true, marked: window.marked });

    // StreamingIndicator для индикации загрузки
    const streamingIndicator = new StreamingIndicator(indicatorContainer);

    // --- Вспомогательные UI функции ---

    function showThought(message) {
        thoughtText.textContent = message || 'Агент думает...';
        thoughtContainer.classList.remove('hidden');
        chatForm.classList.add('hidden');
    }

    function hideThought() {
        thoughtContainer.classList.add('hidden');
        chatForm.classList.remove('hidden');
        messageInput.focus();
    }

    function disableInput() {
        messageInput.disabled = true;
        sendButton.disabled = true;
        planButton.disabled = true;
    }

    function enableInput() {
        messageInput.disabled = false;
        sendButton.disabled = false;
        planButton.disabled = false;
        hideThought();
    }

    // --- Обработчики WebSocket ---

    socketManager.onOpen(() => {
        connectionStatusSpan.textContent = '🟢 Подключено';
        connectionStatusSpan.style.color = '#2ecc71';
        socketManager.send({ type: 'sync_session', session_id: sessionManager.getSessionId() });
    });

    socketManager.onClose(() => {
        connectionStatusSpan.textContent = '🔴 Отключено';
        connectionStatusSpan.style.color = '#e74c3c';
        enableInput();
    });

    socketManager.onError((error) => {
        connectionStatusSpan.textContent = '🟠 Ошибка';
        connectionStatusSpan.style.color = '#e67e22';
        console.error('Socket Error:', error);
        enableInput();
    });

    // Главный обработчик сообщений от сервера
    socketManager.onMessage((msg) => {
        console.log('Received:', msg);

        // Скрываем общие индикаторы, если пришло нерелевантное сообщение
        if (msg.type !== 'agent_thought' && msg.type !== 'llm_token') {
            hideThought();
            streamingIndicator.hide();
        }

        switch (msg.type) {
            case 'session_state':
                handleSessionState(msg.data);
                break;

            case 'llm_token':
                // Этот кейс обрабатывается StreamingManager'ом, который вызывает onToken
                break;

            case 'stream_end':
                // StreamingManager вызывает onComplete, где мы финализируем сообщение
                break;

            case 'agent_thought':
                showThought(msg.data.message);
                break;

            case 'action_required':
                renderConfirmationWidget(msg.data.message, msg.data.tool_call);
                break;

            case 'plan_generated':
                renderPlanConfirmationWidget(msg.data.steps);
                break;

            case 'plan_update':
                updatePlanProgress(msg.data.current_step, msg.data.steps);
                break;

            case 'plan_error':
                renderErrorRecoveryWidget(msg.data.error_message, msg.data.recovery_options);
                break;

            case 'error':
                messageRenderer.render(`Ошибка: ${msg.data.message}`, 'error');
                enableInput();
                break;

            case 'file_watcher_status':
                const fileWatcherStatus = document.getElementById('file-watcher-status');
                if (fileWatcherStatus) {
                    fileWatcherStatus.classList.toggle('hidden', msg.data.status !== 'indexing');
                }
                break;

            default:
                console.warn('Unknown message type:', msg.type);
        }
    });

    // --- Обработчики StreamingManager ---

    streamingManager.onToken((token) => {
        if (!streamingMessageElement) {
            // Первый токен: создаем новый элемент сообщения и скрываем индикатор
            streamingIndicator.hide();
            hideThought();
            streamingMessageElement = messageRenderer.render('', 'agent');
        }
        // Обновляем содержимое существующего элемента
        const messageBody = streamingMessageElement.querySelector('.message-body');
        if (messageBody) {
            // Накапливаем текст и перерисовываем Markdown для корректного отображения
            const fullText = streamingManager.getAccumulatedText();
            messageBody.innerHTML = messageRenderer.renderMarkdown(fullText);
            messageList.scrollTop = messageList.scrollHeight;
        }
    });

    streamingManager.onComplete(() => {
        // Поток завершен, сохраняем финальный текст в историю
        const finalText = streamingManager.getAccumulatedText();
        if (finalText) {
            const history = sessionManager.loadSession().history || [];
            sessionManager.updateSessionData('history', [...history, { role: 'assistant', content: finalText }]);
        }
        // Сбрасываем состояние для следующего сообщения
        streamingMessageElement = null;
        enableInput();
    });

    // --- Логика отправки сообщения ---

    function sendMessage(forcePlan = false) {
        const text = messageInput.value.trim();
        if (!text || !socketManager.isConnected()) return;

        // Отображаем сообщение пользователя и обновляем историю
        messageRenderer.render(text, 'user');
        const history = sessionManager.loadSession().history || [];
        sessionManager.updateSessionData('history', [...history, { role: 'user', content: text }]);
        
        messageInput.value = '';
        messageInput.style.height = 'auto';

        // Блокируем ввод и показываем индикаторы
        disableInput();
        streamingManager.startStreaming();
        streamingIndicator.show();

        // Отправляем запрос на сервер
        socketManager.send({
            type: 'query',
            session_id: sessionManager.getSessionId(),
            data: { text, force_plan }
        });
    }

    // --- Обработка состояния сессии ---

    function handleSessionState(data) {
        messageList.innerHTML = ''; // Очищаем чат перед отрисовкой истории

        const history = data.history || [];
        sessionManager.updateSessionData('history', history);

        if (history.length > 0) {
            history.forEach(msg => {
                if (msg.role === 'user' || (msg.role === 'assistant' && msg.content)) {
                    messageRenderer.render(msg.content, msg.role);
                }
            });
        } else if (data.greeting) {
            messageRenderer.render(data.greeting, 'system');
        }

        if (data.status === 'AWAITING_CONFIRMATION') {
            renderConfirmationWidget(data.confirmation_data.message, data.confirmation_data.tool_call);
        } else {
            enableInput();
        }
    }

    // --- Отрисовка виджетов (логика скопирована из вашего chat.js и адаптирована) ---

    function renderConfirmationWidget(promptText, toolCall) {
        const template = document.getElementById('confirmation-widget-template');
        const widget = template.content.cloneNode(true).firstElementChild;

        widget.querySelector('[data-role="prompt-text"]').textContent = promptText;
        widget.querySelector('[data-role="tool-call-json"]').textContent = JSON.stringify(toolCall.function, null, 2);

        const yesButton = widget.querySelector('[data-role="yes-button"]');
        const noButton = widget.querySelector('[data-role="no-button"]');

        const handleConfirm = (confirmed) => {
            socketManager.send({
                type: 'confirm_action',
                session_id: sessionManager.getSessionId(),
                data: { confirmed }
            });
            widget.remove();
            if (confirmed) showThought('Выполняю подтвержденное действие...');
        };

        yesButton.addEventListener('click', () => handleConfirm(true));
        noButton.addEventListener('click', () => handleConfirm(false));

        messageList.appendChild(widget);
        messageList.scrollTop = messageList.scrollHeight;
    }

    // Функции renderPlanConfirmationWidget, updatePlanProgress, renderErrorRecoveryWidget
    // должны быть реализованы здесь аналогично, если они вам нужны.
    // Я оставлю их заглушками, чтобы не перегружать ответ.
    function renderPlanConfirmationWidget(steps) { console.warn("renderPlanConfirmationWidget not fully implemented"); }
    function updatePlanProgress(currentStep, steps) { console.warn("updatePlanProgress not fully implemented"); }
    function renderErrorRecoveryWidget(errorMessage, recoveryOptions) { console.warn("renderErrorRecoveryWidget not fully implemented"); }


    // --- Настройка обработчиков событий ---

    function setupEventListeners() {
        sendButton.addEventListener('click', () => sendMessage(false));
        planButton.addEventListener('click', () => {
            if (messageInput.value.trim()) {
                sendMessage(true);
            } else {
                alert('Пожалуйста, введите задачу перед запуском планирования.');
            }
        });

        autoConfirmButton.addEventListener('click', () => {
            state.isAutoConfirmEnabled = !state.isAutoConfirmEnabled;
            autoConfirmButton.classList.toggle('active', state.isAutoConfirmEnabled);
            autoConfirmButton.title = state.isAutoConfirmEnabled ? 'Авто-подтверждение включено' : 'Авто-подтверждение выключено';
        });

        freezeWatcherButton.addEventListener('click', () => {
            state.isWatcherFrozen = !state.isWatcherFrozen;
            const command = state.isWatcherFrozen ? 'freeze' : 'unfreeze';
            socketManager.send({ type: 'control_file_watcher', data: { command } });
            freezeWatcherButton.classList.toggle('frozen', state.isWatcherFrozen);
            freezeWatcherButton.title = state.isWatcherFrozen ? 'Возобновить индексацию (заморожено)' : 'Приостановить индексацию';
        });

        messageInput.addEventListener('keydown', (event) => {
            if (event.key === 'Enter' && !event.shiftKey) {
                event.preventDefault();
                sendMessage(false);
            }
        });

        messageInput.addEventListener('input', () => {
            messageInput.style.height = 'auto';
            messageInput.style.height = (messageInput.scrollHeight) + 'px';
        });

        clearHistoryButton.addEventListener('click', () => {
            if (confirm('Вы уверены, что хотите очистить историю текущего чата?')) {
                socketManager.send({ type: 'clear_history', session_id: sessionManager.getSessionId() });
                messageList.innerHTML = '';
                messageRenderer.render('История очищена. Готов к новым задачам!', 'system');
            }
        });
    }

    // --- Запуск приложения ---
    setupEventListeners();
    socketManager.connect().catch(err => console.error("Initial connection failed:", err));
});
