document.addEventListener('DOMContentLoaded', async () => {
    // --- Импорты модулей ---
    // Пути указаны относительно /js/
    const { SocketManager } = await import('./socket.js');
    const { StreamingManager } = await import('./streaming.js');
    // SessionManager заменен на локальное управление состоянием
    const { MessageRenderer } = await import('./message.js');
    const { StreamingIndicator } = await import('./streaming-indicator.js');

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
    const newChatButton = document.getElementById('new-chat-button');
    const chatList = document.getElementById('chat-list');

    // --- Состояние приложения ---
    let state = {
        sessions: [], // [{id: '...', title: '...'}, ...]
        activeSessionId: null,
        isAutoConfirmEnabled: false,
        isWatcherFrozen: false,
    };
    let streamingMessageElement = null; // Для хранения ссылки на DOM-элемент стриминга

    // --- Инициализация менеджеров ---
    const socketManager = new SocketManager(`ws://${window.location.host}/ws`, {
        reconnectAttempts: 5,
        reconnectDelay: 3000,
    });

    // StreamingManager будет обрабатывать llm_token и stream_end
    const streamingManager = new StreamingManager(socketManager);

    // MessageRenderer для отрисовки сообщений
    const messageRenderer = new MessageRenderer(messageList, { useMarkdown: true });

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
        switchChat(state.activeSessionId); // Синхронизируем активный чат при подключении
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
            // История управляется на сервере, здесь ничего делать не нужно.
        }
        // Сбрасываем состояние для следующего сообщения
        streamingMessageElement = null;
        enableInput();
    });

    // --- Логика отправки сообщения ---

    function sendMessage(forcePlan = false) {
        const text = messageInput.value.trim();
        if (!text || !socketManager.isConnected() || !state.activeSessionId) return;

        // Отображаем сообщение пользователя и обновляем историю
        messageRenderer.render(text, 'user');
        updateChatTitle(state.activeSessionId, text);
        
        messageInput.value = '';
        messageInput.style.height = 'auto';

        // Блокируем ввод и показываем индикаторы
        disableInput();
        streamingManager.startStreaming();
        streamingIndicator.show();

        // Отправляем запрос на сервер
        socketManager.send({
            type: 'query',
            session_id: state.activeSessionId,
            data: { text, force_plan }
        });
    }

    // --- Обработка состояния сессии ---

    function handleSessionState(data) {
        messageList.innerHTML = ''; // Очищаем чат перед отрисовкой истории

        if (data.history && data.history.length > 0) {
            const history = data.history;
            history.forEach(msg => {
                if (msg.role === 'user' || (msg.role === 'assistant' && msg.content)) {
                    messageRenderer.render(msg.content, msg.role);
                }
            });
            updateChatTitle(state.activeSessionId, history[0]?.content);
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
                session_id: state.activeSessionId,
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
    function renderPlanConfirmationWidget(steps) {
        const template = document.getElementById('plan-confirmation-widget-template');
        const widget = template.content.cloneNode(true).firstElementChild;
        const planList = widget.querySelector('[data-role="plan-editor-list"]');

        const createStepItem = (stepText) => {
            const stepTemplate = document.getElementById('plan-step-item-template');
            const stepItem = stepTemplate.content.cloneNode(true).firstElementChild;
            stepItem.querySelector('.plan-step-text').textContent = stepText;
            stepItem.querySelector('.btn-remove-step').addEventListener('click', () => stepItem.remove());
            return stepItem;
        };

        steps.forEach(step => {
            planList.appendChild(createStepItem(step));
        });

        if (typeof Sortable !== 'undefined') {
            new Sortable(planList, { animation: 150, handle: '.drag-handle' });
        }

        widget.querySelector('[data-role="add-step-button"]').addEventListener('click', () => {
            planList.appendChild(createStepItem('Новый шаг...'));
        });

        const handleConfirm = (confirmed) => {
            const updatedSteps = confirmed ? Array.from(planList.querySelectorAll('.plan-step-text')).map(el => el.textContent.trim()) : [];
            socketManager.send({
                type: 'confirm_plan',
                session_id: state.activeSessionId,
                data: { confirmed, steps: updatedSteps }
            });
            widget.remove();
        };

        widget.querySelector('[data-role="approve-plan-button"]').addEventListener('click', () => handleConfirm(true));
        widget.querySelector('[data-role="reject-plan-button"]').addEventListener('click', () => handleConfirm(false));

        messageList.appendChild(widget);
        messageList.scrollTop = messageList.scrollHeight;
    }
    function updatePlanProgress(currentStep, steps) { console.warn("updatePlanProgress not fully implemented"); }
    function renderErrorRecoveryWidget(errorMessage, recoveryOptions) {
        const template = document.getElementById('error-recovery-widget-template');
        const widget = template.content.cloneNode(true).firstElementChild;

        widget.querySelector('[data-role="error-message"]').textContent = errorMessage;
        const buttonContainer = widget.querySelector('[data-role="button-container"]');

        recoveryOptions.forEach(option => {
            const button = document.createElement('button');
            button.className = 'px-3 py-1.5 rounded-md text-white bg-sky-600 hover:bg-sky-700 text-sm cursor-pointer';
            button.textContent = option;
            button.addEventListener('click', () => {
                socketManager.send({
                    type: 'confirm_error_recovery',
                    session_id: state.activeSessionId,
                    data: { option }
                });
                widget.remove();
            });
            buttonContainer.appendChild(button);
        });

        messageList.appendChild(widget);
        messageList.scrollTop = messageList.scrollHeight;
    }

    // --- Настройка обработчиков событий ---

    function setupEventListeners() {
        newChatButton.addEventListener('click', () => createNewChat());

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
                socketManager.send({ type: 'clear_history', session_id: state.activeSessionId });
                messageList.innerHTML = '';
                messageRenderer.render('История очищена. Готов к новым задачам!', 'system');
            }
        });
    }

    // --- Логика управления чатами ---

    function loadSessions() {
        const savedSessions = localStorage.getItem('chatSessions');
        if (savedSessions) {
            state.sessions = JSON.parse(savedSessions);
            state.activeSessionId = localStorage.getItem('activeSessionId') || (state.sessions[0]?.id || null);
        }
        if (state.sessions.length === 0) {
            createNewChat(false); // Создаем первый чат без переключения
        }
    }

    function saveSessions() {
        localStorage.setItem('chatSessions', JSON.stringify(state.sessions));
        localStorage.setItem('activeSessionId', state.activeSessionId);
    }

    function createNewChat(shouldSwitch = true) {
        const newSessionId = 'sess_' + Math.random().toString(36).substr(2, 9);
        state.sessions.unshift({ id: newSessionId, title: 'Новый чат' });
        if (shouldSwitch) {
            switchChat(newSessionId);
        } else {
            state.activeSessionId = newSessionId;
        }
        renderChatList();
        saveSessions();
    }

    function switchChat(sessionId) {
        if (!sessionId || (state.activeSessionId === sessionId && messageList.innerHTML !== '')) return;

        state.activeSessionId = sessionId;
        messageList.innerHTML = '';
        streamingMessageElement = null;
        streamingManager.reset();
        enableInput();

        if (socketManager.isConnected()) {
            socketManager.send({ type: 'sync_session', session_id: sessionId });
        }
        
        renderChatList();
        saveSessions();
    }

    function deleteChat(sessionIdToDelete) {
        state.sessions = state.sessions.filter(s => s.id !== sessionIdToDelete);
        
        if (state.activeSessionId === sessionIdToDelete) {
            if (state.sessions.length > 0) {
                switchChat(state.sessions[0].id);
            } else {
                createNewChat();
            }
        }
        renderChatList();
        saveSessions();
    }

    function updateChatTitle(sessionId, firstMessage) {
        const session = state.sessions.find(s => s.id === sessionId);
        if (session && session.title === 'Новый чат' && firstMessage) {
            session.title = firstMessage.substring(0, 30) + (firstMessage.length > 30 ? '...' : '');
            renderChatList();
            saveSessions();
        }
    }

    function renderChatList() {
        chatList.innerHTML = '';
        state.sessions.forEach(session => {
            const template = document.getElementById('chat-list-item-template');
            const item = template.content.cloneNode(true).firstElementChild;
            item.dataset.sessionId = session.id;
            item.querySelector('.chat-title').textContent = session.title;

            if (session.id === state.activeSessionId) {
                item.classList.add('active'); // CSS-класс для активного чата
            }

            item.addEventListener('click', () => switchChat(session.id));
            item.querySelector('.delete-chat-button').addEventListener('click', (e) => {
                e.stopPropagation();
                if (confirm(`Удалить чат "${session.title}"?`)) {
                    deleteChat(session.id);
                }
            });

            chatList.appendChild(item);
        });
    }

    // --- Запуск приложения ---
    loadSessions();
    renderChatList();
    setupEventListeners();
    socketManager.connect().catch(err => console.error("Initial connection failed:", err));
});
