document.addEventListener('DOMContentLoaded', () => {
    // --- DOM Elements ---
    const messageList = document.getElementById('message-list');
    const chatForm = document.getElementById('chat-form');
    const messageInput = document.getElementById('message-input');
    const sendButton = document.getElementById('send-button');
    const planButton = document.getElementById('plan-button');
    const clearHistoryButton = document.getElementById('clear-history-button');
    const autoConfirmButton = document.getElementById('auto-confirm-button');
    const connectionStatus = document.getElementById('connection-status');
    const fileWatcherStatus = document.getElementById('file-watcher-status');
    const freezeWatcherButton = document.getElementById('freeze-watcher-button');
    const thoughtContainer = document.getElementById('agent-thought-container');
    const thoughtText = document.getElementById('agent-thought-text');
    const newChatButton = document.getElementById('new-chat-button');
    const chatList = document.getElementById('chat-list');

    // --- State ---
    let socket;
    let state = {
        sessions: [], // [{id: '...', title: '...'}, ...]
        activeSessionId: null,
        isWatcherFrozen: false,
        isAutoConfirmEnabled: false,
    };
    let streamingMessageElement = null;
    let accumulatedStreamText = '';

    // --- Initialization ---

    marked.setOptions({
        breaks: true,   // Переносы строк \n превратятся в <br>
        gfm: true       // Включаем GitHub Flavored Markdown (таблицы, зачеркивания)
    });

    // Утилита для экранирования HTML, чтобы логи и символы не ломали верстку
    function escapeHTML(str) {
        return str?.replace(/&/g, '&amp;')
                  .replace(/</g, '&lt;')
                  .replace(/>/g, '&gt;')
                  .replace(/"/g, '&quot;')
                  .replace(/'/g, '&#039;');
    }

    // 2. Полностью заменяем старую функцию parseMarkdown на использование библиотеки:
    function renderMarkdown(text) {
        if (!text) return '';

        // Используем библиотеку marked для генерации HTML
        let rawHtml = marked.parse(text);

        // Добавляем красивую обертку с кнопкой «Копировать» для блоков кода <pre>
        const tempDiv = document.createElement('div');
        tempDiv.innerHTML = rawHtml;

        const preBlocks = tempDiv.querySelectorAll('pre');
        preBlocks.forEach((pre, index) => {
            const codeElement = pre.querySelector('code');
            const cleanCode = codeElement ? codeElement.innerText : pre.innerText;

            // Создаем контейнер по вашему CSS-дизайну
            const wrapper = document.createElement('div');
            wrapper.className = 'relative my-3 rounded-lg overflow-hidden border border-gray-700 bg-gray-950 font-mono text-sm';
            
            wrapper.innerHTML = `
                <div class="flex items-center justify-between px-4 py-1.5 bg-gray-900 text-xs text-gray-400 border-b border-gray-800">
                    <span>CODE / LOGS</span>
                    <button onclick="navigator.clipboard.writeText(this.parentElement.nextElementSibling.innerText); this.textContent='Скопировано!'; setTimeout(()=>this.textContent='Копировать', 2000)" class="hover:text-white transition-colors cursor-pointer">Копировать</button>
                </div>
                <pre class="p-4 overflow-x-auto text-gray-200"><code class="whitespace-pre">${cleanCode}</code></pre>
            `;
            
            pre.parentNode.replaceChild(wrapper, pre);
        });

        return tempDiv.innerHTML;
    }

    // Вспомогательные функции для показа/скрытия мыслей
    function showAgentThought(message) {
        thoughtText.textContent = message || 'Агент думает...';
        thoughtContainer.classList.remove('hidden');
        chatForm.classList.add('hidden');
    }

    function hideAgentThought() {
        thoughtContainer.classList.add('hidden');
        chatForm.classList.remove('hidden');
        messageInput.focus();
    }

    function connect() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsHost = window.location.hostname;
        const wsPort = 9000; 
        socket = new WebSocket(`${wsProtocol}//${wsHost}:${wsPort}/ws`); // Используем порт 9000

        socket.onopen = () => {
            console.log('WebSocket connection established.');
            connectionStatus.textContent = 'Соединение установлено';
            connectionStatus.style.color = '#2ecc71';
            switchChat(state.activeSessionId); // Sync the active session on connect
        };

        socket.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            console.log('Received:', msg);

            // Скрываем блок "мыслей" для большинства сообщений, кроме самих мыслей и токенов
            if (msg.type !== 'agent_thought' && msg.type !== 'llm_token') {
                hideAgentThought();
            }

            switch (msg.type) {
                case 'session_state':
                    handleSessionState(msg.data);
                    break;
                // `query_response` заменен на потоковую передачу
                case 'llm_token':
                    if (!streamingMessageElement) {
                        // Первый токен: создаем новый элемент сообщения для агента
                        hideAgentThought(); // Скрываем спиннер "Агент думает..."
                        streamingMessageElement = addMessage('', 'agent', state.activeSessionId);
                    }
                    accumulatedStreamText += msg.data.token;
                    const messageContent = streamingMessageElement.querySelector('.message-content');
                    if (messageContent) {
                        messageContent.innerHTML = renderMarkdown(accumulatedStreamText);
                    }
                    messageList.scrollTop = messageList.scrollHeight;
                    break;
                case 'stream_end':
                    // Поток завершен. Сбрасываем состояние и разблокируем ввод.
                    streamingMessageElement = null;
                    accumulatedStreamText = '';
                    hideAgentThought();
                    break;
                case 'agent_thought':
                    showAgentThought(msg.data.message);
                    break;
                case 'action_required':
                    addConfirmationWidget(msg.data.message, msg.data.tool_call);
                    break;
                case 'plan_generated':
                    addPlanConfirmationWidget(msg.data.steps);
                    break;
                case 'plan_update':
                    // Во время выполнения плана блок мыслей и так должен быть виден
                    updatePlanProgress(msg.data.current_step, msg.data.steps);
                    break;
                case 'plan_error':
                    addErrorRecoveryWidget(msg.data.error_message, msg.data.recovery_options);
                    break;
                case 'error':
                    addMessage(`Ошибка: ${msg.data.message}`, 'error', state.activeSessionId);
                    break;
                case 'pong':
                    break;
                case 'file_watcher_status':
                    if (msg.data.status === 'indexing') {
                        fileWatcherStatus.classList.remove('hidden');
                    } else { // 'idle' or other
                        fileWatcherStatus.classList.add('hidden');
                    }
                    break;
                default:
                    console.warn('Unknown message type:', msg.type);
            }
        };

        socket.onclose = () => {
            console.log('WebSocket connection closed. Reconnecting...');
            connectionStatus.textContent = 'Переподключение...';
            connectionStatus.style.color = '#e67e22';
            setTimeout(connect, 3000);
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            connectionStatus.textContent = 'Ошибка соединения';
            connectionStatus.style.color = '#e74c3c';
        };
    }

    function addMessage(text, sender, sessionIdForMessage) {
        if (sessionIdForMessage !== state.activeSessionId) return; // Don't render for inactive chats

        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        
        contentElement.innerHTML = renderMarkdown(text);

        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);
        messageList.scrollTop = messageList.scrollHeight;
        return messageElement;
    }

    function addConfirmationWidget(promptText, toolCall) {
        const template = document.getElementById('confirmation-widget-template');
        const widgetFragment = template.content.cloneNode(true);
        const widgetElement = widgetFragment.querySelector('.message');

        widgetElement.querySelector('[data-role="prompt-text"]').textContent = promptText;
        widgetElement.querySelector('[data-role="tool-call-json"]').textContent = JSON.stringify(toolCall.function, null, 2);

        const yesButton = widgetElement.querySelector('[data-role="yes-button"]');
        const noButton = widgetElement.querySelector('[data-role="no-button"]');
        let autoConfirmTimer = null;

        const executeYesAction = () => {
            if (autoConfirmTimer) {
                clearInterval(autoConfirmTimer);
            }
            socket.send(JSON.stringify({ type: 'confirm_action', session_id: state.activeSessionId, data: { confirmed: true } }));
            widgetElement.remove();
            showAgentThought('Выполняю подтвержденное действие...');
        };

        if (state.isAutoConfirmEnabled) {
            const countdownElement = document.createElement('span');
            countdownElement.className = 'text-xs text-gray-400 ml-3';
            let countdown = 3;
            countdownElement.textContent = `(авто-подтверждение через ${countdown}...)`;
            noButton.parentElement.appendChild(countdownElement);

            autoConfirmTimer = setInterval(() => {
                countdown--;
                countdownElement.textContent = `(авто-подтверждение через ${countdown}...)`;
                if (countdown <= 0) {
                    executeYesAction();
                }
            }, 1000);

            yesButton.onclick = executeYesAction;
            noButton.textContent = 'Отменить';
            noButton.onclick = () => {
                clearInterval(autoConfirmTimer);
                socket.send(JSON.stringify({ type: 'confirm_action', session_id: state.activeSessionId, data: { confirmed: false } }));
                widgetElement.remove();
            };

        } else {
            yesButton.onclick = executeYesAction;
            noButton.onclick = () => {
                socket.send(JSON.stringify({ type: 'confirm_action', session_id: state.activeSessionId, data: { confirmed: false } }));
                widgetElement.remove();
            };
        }

        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addPlanConfirmationWidget(steps) {
        const template = document.getElementById('plan-confirmation-widget-template');
        const widgetFragment = template.content.cloneNode(true);
        const widgetElement = widgetFragment.querySelector('.message');
        const planList = widgetElement.querySelector('[data-role="plan-editor-list"]');
        const stepTemplate = document.getElementById('plan-step-item-template');

        const createStepElement = (stepText) => {
            const stepFragment = stepTemplate.content.cloneNode(true);
            const stepLi = stepFragment.querySelector('li');
            const textDiv = stepLi.querySelector('.plan-step-text');
            textDiv.textContent = stepText;

            stepLi.querySelector('.btn-remove-step').onclick = () => stepLi.remove();
            
            return stepLi;
        };

        steps.forEach(step => {
            planList.appendChild(createStepElement(step));
        });

        // --- DRAG-AND-DROP ---
        new Sortable(planList, {
            animation: 150,
            handle: '.drag-handle', // Ограничиваем перетаскивание за "ручку"
            ghostClass: 'sortable-ghost' // Класс для элемента-призрака
        });

        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        widgetElement.querySelector('[data-role="add-step-button"]').addEventListener('click', () => {
            const newStep = createStepElement('Новый шаг плана...');
            planList.appendChild(newStep);
            newStep.querySelector('.plan-step-text').focus();
        });

        widgetElement.querySelector('[data-role="approve-plan-button"]').addEventListener('click', () => {
            const editedSteps = [];
            const stepElements = widgetElement.querySelectorAll('.plan-step-text');
            stepElements.forEach(el => {
                const txt = el.textContent.trim();
                if(txt) editedSteps.push(txt);
            });

            widgetElement.remove();
            showAgentThought('Приступаю к выполнению плана...');
            
            socket.send(JSON.stringify({
                type: 'confirm_plan',
                session_id: state.activeSessionId,
                data: { 
                    confirmed: true,
                    steps: editedSteps
                }
            }));
        });

        widgetElement.querySelector('[data-role="reject-plan-button"]').addEventListener('click', () => {
            widgetElement.remove();
            addMessage('Вы отклонили план. Вы можете скорректировать задачу или задать другой вопрос.', 'agent', state.activeSessionId);
            
            socket.send(JSON.stringify({
                type: 'confirm_plan',
                session_id: state.activeSessionId,
                data: { confirmed: false }
            }));
        });
    }

    function updatePlanProgress(currentStepIndex, steps) {
        let progressElement = document.getElementById('active-plan-progress');
        if (!progressElement) {
            progressElement = document.createElement('div');
            progressElement.id = 'active-plan-progress';
            progressElement.classList.add('message', 'agent');
            const template = document.getElementById('plan-progress-widget-template');
            progressElement.appendChild(template.content.cloneNode(true));
            messageList.appendChild(progressElement);
        }

        const progressList = progressElement.querySelector('[data-role="progress-list"]');
        progressList.innerHTML = ''; // Очищаем предыдущее состояние
        
        steps.forEach((step, index) => {
            let icon = '⏳'; 
            let style = 'color: #94a3b8; opacity: 0.6;'; 
            
            if (index < currentStepIndex) {
                icon = '✅'; 
                style = 'color: #10b981; text-decoration: line-through; opacity: 0.5; font-size: 0.95em;';
            } else if (index === currentStepIndex) {
                icon = '⚙️'; 
                style = 'color: #38bdf8; font-weight: bold; background: rgba(56, 189, 248, 0.08); padding: 4px 8px; border-radius: 4px; display: inline-block; width: 100%;';
            }
            
            const li = document.createElement('li');
            li.style.cssText = style;
            li.className = 'flex items-start gap-2';
            li.innerHTML = `<span>${icon}</span> <span>${escapeHTML(step)}</span>`;
            progressList.appendChild(li);
        });
        
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addErrorRecoveryWidget(errorMessage, recoveryOptions) {
        const template = document.getElementById('error-recovery-widget-template');
        const widgetFragment = template.content.cloneNode(true);
        const widgetElement = widgetFragment.querySelector('.message');

        widgetElement.querySelector('[data-role="error-message"]').textContent = errorMessage;
        const buttonContainer = widgetElement.querySelector('[data-role="button-container"]');

        const optionMap = {
            'retry': '🔄 Повторить шаг',
            'skip': '⏭ Пропустить шаг',
            're-plan': '🗺 Перепланировать',
            'abort': '🛑 Остановить всё'
        };

        const finalOptions = recoveryOptions.includes('abort') ? recoveryOptions : [...recoveryOptions, 'abort'];

        finalOptions.forEach(option => {
            const button = document.createElement('button');
            button.className = 'px-3 py-1.5 rounded text-white text-xs font-bold cursor-pointer transition-colors';
            button.textContent = optionMap[option] || option;
            button.dataset.option = option;

            if (option === 'retry') {
                button.classList.add('bg-green-600', 'hover:bg-green-700');
            } else if (option === 'abort' || option === 'skip') {
                button.classList.add('bg-red-600', 'hover:bg-red-700');
            } else {
                button.classList.add('bg-blue-600', 'hover:bg-blue-700');
            }

            button.addEventListener('click', () => {
                socket.send(JSON.stringify({
                    type: 'confirm_error_recovery',
                    session_id: state.activeSessionId,
                    data: { option: button.dataset.option }
                }));
                widgetElement.remove();
                showAgentThought('Обрабатываю решение по ошибке...');
            });
            buttonContainer.appendChild(button);
        });

        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function handleSessionState(data) {
        messageList.innerHTML = ''; 
        
        if (data.history && data.history.length > 0) {
            data.history.forEach(msg => {
                if (msg.role === 'user' || (msg.role === 'assistant' && msg.content)) {
                    addMessage(msg.content, msg.role, state.activeSessionId);
                }
            });
            // Update chat title with the first user message if it's a "New Chat"
            updateChatTitle(state.activeSessionId, data.history[0]?.content);
        } else if (data.greeting) {
            addMessage(data.greeting, 'agent', state.activeSessionId);
        }

        if (data.status === 'AWAITING_CONFIRMATION') {
            addConfirmationWidget(data.confirmation_data.message, data.confirmation_data.tool_call);
        } else {
            hideAgentThought();
        }
    }

    function sendMessage(forcePlan = false) {
        const text = messageInput.value.trim();
        if (text && socket && socket.readyState === WebSocket.OPEN) {
            addMessage(text, 'user', state.activeSessionId);
            messageInput.value = '';
            messageInput.style.height = 'auto'; // Сброс высоты

            const message = {
                type: 'query',
                session_id: state.activeSessionId,
                data: { 
                    text: text,
                    force_plan: forcePlan
                }
            };
            socket.send(JSON.stringify(message));
            
            // Update chat title if it's a new chat
            updateChatTitle(state.activeSessionId, text);

            // Сразу после отправки показываем блок "мыслей"
            // и сбрасываем состояние предыдущего стриминга
            streamingMessageElement = null;
            accumulatedStreamText = '';
            showAgentThought('Анализирую запрос...');
        }
    }

    // --- Multi-Chat Logic ---

    function loadSessions() {
        const savedSessions = localStorage.getItem('chatSessions');
        if (savedSessions) {
            state.sessions = JSON.parse(savedSessions);
            state.activeSessionId = localStorage.getItem('activeSessionId') || (state.sessions[0]?.id || null);
        }
        if (state.sessions.length === 0) {
            createNewChat(false); // Create initial chat without switching
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
        accumulatedStreamText = '';
        hideAgentThought();

        if (socket && socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({ type: 'sync_session', session_id: sessionId }));
        }
        
        renderChatList(); // To update the active highlight
        saveSessions();
        messageInput.focus();
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
                item.classList.add('active');
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

    // --- Event Listeners Setup ---
    function setupEventListeners() {
        sendButton.addEventListener('click', () => sendMessage(false));
        planButton.addEventListener('click', () => {
            if (messageInput.value.trim()) {
                sendMessage(true);
            } else {
                alert('Пожалуйста, введите задачу перед запуском планирования.');
            }
        });
        newChatButton.addEventListener('click', () => createNewChat());
        autoConfirmButton.addEventListener('click', () => {
            state.isAutoConfirmEnabled = !state.isAutoConfirmEnabled;
            if (state.isAutoConfirmEnabled) {
                autoConfirmButton.classList.add('active');
                autoConfirmButton.title = 'Авто-подтверждение включено';
            } else {
                autoConfirmButton.classList.remove('active');
                autoConfirmButton.title = 'Авто-подтверждение выключено';
            }
        });
        freezeWatcherButton.addEventListener('click', () => {
            state.isWatcherFrozen = !state.isWatcherFrozen;
            const command = state.isWatcherFrozen ? 'freeze' : 'unfreeze';
            
            if (socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({
                    type: 'control_file_watcher',
                    data: { command: command }
                }));
            }
            
            // Обновляем внешний вид и подсказку кнопки
            if (state.isWatcherFrozen) {
                freezeWatcherButton.classList.add('frozen');
                freezeWatcherButton.title = 'Возобновить индексацию (заморожено)';
            } else {
                freezeWatcherButton.classList.remove('frozen');
                freezeWatcherButton.title = 'Приостановить индексацию';
            }
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
                if (socket && socket.readyState === WebSocket.OPEN) {
                    socket.send(JSON.stringify({ type: 'clear_history', session_id: state.activeSessionId }));
                    messageList.innerHTML = '';
                    addMessage('История очищена. Готов к новым задачам!', 'agent', state.activeSessionId);
                }
            }
        });
    }

    // --- App Start ---
    loadSessions();
    renderChatList();
    setupEventListeners();
    connect(); // Connect WebSocket
});