// === ОТЛАДКА ===
console.log('🚀 index.js загружен');

import { SocketManager } from './socket.js';
import { StreamingManager } from './streaming.js';
import { SessionManager } from './session.js';
import { TokenAccumulator } from './token-accumulator.js';
import { StreamingIndicator } from './streaming-indicator.js';
import { MessageRenderer } from './message.js';

// === ОТЛАДКА ===
console.log('📦 Импорты загружены');

// --- DOM Elements (согласно index.html) ---
const messageList = document.getElementById('message-list');
const messageInput = document.getElementById('message-input');
const sendButton = document.getElementById('send-button');
const planButton = document.getElementById('plan-button');
const connectionStatusSpan = document.getElementById('connection-status');
const thoughtContainer = document.getElementById('agent-thought-container');
const thoughtText = document.getElementById('agent-thought-text');
const autoConfirmButton = document.getElementById('auto-confirm-button');
const freezeWatcherButton = document.getElementById('freeze-watcher-button');
const clearHistoryButton = document.getElementById('clear-history-button');

// === ОТЛАДКА ===
console.log('✅ DOM элементы найдены:', {
    messageList: !!messageList,
    messageInput: !!messageInput,
    sendButton: !!sendButton,
    connectionStatusSpan: !!connectionStatusSpan
});

// --- Initialization ---
const socketManager = new SocketManager(`ws://${window.location.host}/ws`, {
    reconnectAttempts: 5,
    reconnectDelay: 3000,
    keepAliveInterval: 30000
});
console.log('🔌 SocketManager создан');

const streamingManager = new StreamingManager(socketManager);
console.log('🌊 StreamingManager создан');

// === ОТЛАДКА ===
console.log('🔌 Устанавливаем WebSocket соединение...');
await socketManager.connect();
console.log('✅ WebSocket подключен');

const messageRenderer = new MessageRenderer(messageList, { useMarkdown: true });
console.log('🎨 MessageRenderer создан');

const tokenAccumulator = new TokenAccumulator(thoughtContainer, {
    animate: true,
    highlightCurrent: false,
    useMarkdown: true
});
console.log('📦 TokenAccumulator создан');

const streamingIndicator = new StreamingIndicator(thoughtContainer, {
    defaultText: 'Агент генерирует ответ...'
});
console.log('⏳ StreamingIndicator создан');

// --- State ---
let sessionId = localStorage.getItem('llm_agent_session_id');
if (!sessionId) {
    sessionId = 'sess_' + Math.random().toString(36).substr(2, 9);
    localStorage.setItem('llm_agent_session_id', sessionId);
}

let isAutoConfirmEnabled = false;
let isWatcherFrozen = false;
// Escape HTML to prevent XSS
function escapeHTML(str) {
    const div = document.createElement('div');
    div.appendChild(document.createTextNode(str));
    return div.innerHTML;
}

// Parse Markdown with custom code block styling
function parseMarkdown(text) {
    if (!text) return '';
    
    let rawHtml = marked.parse(text);
    const tempDiv = document.createElement('div');
    tempDiv.innerHTML = rawHtml;

    const preBlocks = tempDiv.querySelectorAll('pre');
    preBlocks.forEach((pre, index) => {
        const codeElement = pre.querySelector('code');
        const cleanCode = codeElement ? codeElement.innerText : pre.innerText;

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

// === ОТЛАДКА ===
console.log('📡 WebSocket handlers настроены');

socketManager.onOpen(() => {
    console.log('🟢 WebSocket подключен');
    connectionStatusSpan.textContent = '🟢 Подключено';
    connectionStatusSpan.style.color = '#2ecc71';
    // При подключении синхронизируем сессию
    socketManager.send({
        type: 'sync_session',
        session_id: sessionManager.getSessionId()
    });
});

socketManager.onClose(() => {
    console.log('🔴 WebSocket отключен');
    connectionStatusSpan.textContent = '🔴 Отключено';
    connectionStatusSpan.style.color = '#e74c3c';
});

socketManager.onError((error) => {
    console.error('❌ Socket Error:', error);
    connectionStatusSpan.textContent = '🟠 Ошибка';
    connectionStatusSpan.style.color = '#e67e22';
    console.error('Socket Error:', error);
    messageRenderer.render(`Ошибка соединения: ${error.message}`, 'error');
});

socketManager.onMessage((message) => {
    console.log('📨 Received message:', message.type);
});

socketManager.onClose(() => {
    connectionStatusSpan.textContent = '🔴 Отключено';
    connectionStatusSpan.style.color = '#e74c3c';
});

socketManager.onError((error) => {
    connectionStatusSpan.textContent = '🟠 Ошибка';
    connectionStatusSpan.style.color = '#e67e22';
    console.error('Socket Error:', error);
    messageRenderer.render(`Ошибка соединения: ${error.message}`, 'error');
});

socketManager.onMessage((message) => {
    console.log('Received message:', message);
    
    // Hide thought container for most message types
    if (message.type !== 'agent_thought' && message.type !== 'llm_token') {
        hideAgentThought();
    }

    switch (message.type) {
        case 'session_state':
            handleSessionState(message.data);
            break;

        case 'llm_token':
            // StreamingManager handles this via onToken
            break;

        case 'stream_end':
            // Add accumulated text to chat history
            const accumulatedText = tokenAccumulator.getText();
            if (accumulatedText) {
                messageRenderer.render(accumulatedText, 'agent');
                sessionManager.updateSessionData('history', [
                    ...sessionManager.loadSession().history,
                    { role: 'assistant', content: accumulatedText }
                ]);
            }
            tokenAccumulator.clear();
            hideAgentThought();
            streamingIndicator.hide();
            break;

        case 'agent_thought':
            showAgentThought(message.data.message);
            break;

        case 'action_required':
            renderConfirmationWidget(message.data.message, message.data.tool_call);
            break;

        case 'plan_generated':
            renderPlanConfirmationWidget(message.data.steps);
            break;

        case 'plan_update':
            updatePlanProgress(message.data.current_step, message.data.steps);
            break;

        case 'plan_error':
            renderErrorRecoveryWidget(message.data.error_message, message.data.recovery_options);
            break;

        case 'error':
            messageRenderer.render(`Ошибка: ${message.data.message}`, 'error');
            streamingIndicator.hide();
            break;

        case 'pong':
            break;

        case 'file_watcher_status':
            const fileWatcherStatus = document.getElementById('file-watcher-status');
            if (fileWatcherStatus) {
                if (message.data.status === 'indexing') {
                    fileWatcherStatus.classList.remove('hidden');
                } else {
                    fileWatcherStatus.classList.add('hidden');
                }
            }
            break;

        default:
            console.warn('Unknown message type:', message.type);
    }
});

// Streaming token handler
streamingManager.onToken((token) => {
    tokenAccumulator.appendToken(token);
});

// --- Widget Renderers ---

function renderConfirmationWidget(promptText, toolCall) {
    const template = document.getElementById('confirmation-widget-template');
    const widget = template.content.cloneNode(true).firstElementChild;

    widget.querySelector('[data-role="prompt-text"]').textContent = promptText;
    widget.querySelector('[data-role="tool-call-json"]').textContent = JSON.stringify(toolCall.function, null, 2);

    const yesButton = widget.querySelector('[data-role="yes-button"]');
    const noButton = widget.querySelector('[data-role="no-button"]');
    let autoConfirmTimer = null;

    const executeYesAction = () => {
        if (autoConfirmTimer) {
            clearInterval(autoConfirmTimer);
        }
        socketManager.send({
            type: 'confirm_action',
            session_id: sessionManager.getSessionId(),
            data: { confirmed: true }
        });
        widget.remove();
        showAgentThought('Выполняю подтвержденное действие...');
    };

    if (isAutoConfirmEnabled) {
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
    }

    yesButton.addEventListener('click', () => {
        if (autoConfirmTimer) {
            clearInterval(autoConfirmTimer);
        }
        executeYesAction();
    });

    noButton.addEventListener('click', () => {
        if (autoConfirmTimer) {
            clearInterval(autoConfirmTimer);
        }
        socketManager.send({
            type: 'confirm_action',
            session_id: sessionManager.getSessionId(),
            data: { confirmed: false }
        });
        widget.remove();
    });

    messageList.appendChild(widget);
    messageList.scrollTop = messageList.scrollHeight;
}

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

    // Make list sortable
    if (typeof Sortable !== 'undefined') {
        new Sortable(planList, {
            animation: 150,
            handle: '.drag-handle',
        });
    }

    // Add step button
    widget.querySelector('[data-role="add-step-button"]').addEventListener('click', () => {
        planList.appendChild(createStepItem('Новый шаг...'));
    });

    // Confirmation buttons
    const approveButton = widget.querySelector('[data-role="approve-plan-button"]');
    const rejectButton = widget.querySelector('[data-role="reject-plan-button"]');

    const handleConfirm = (confirmed) => {
        const updatedSteps = confirmed ? Array.from(planList.querySelectorAll('.plan-step-text')).map(el => el.textContent.trim()) : [];
        socketManager.send({
            type: 'confirm_plan',
            session_id: sessionManager.getSessionId(),
            data: {
                confirmed,
                steps: updatedSteps
            }
        });
        widget.remove();
    };

    approveButton.addEventListener('click', () => handleConfirm(true));
    rejectButton.addEventListener('click', () => handleConfirm(false));

    messageList.appendChild(widget);
    messageList.scrollTop = messageList.scrollHeight;
}


function sendMessage(forcePlan = false) {
    console.log('📤 Отправка сообщения:', forcePlan ? 'с планированием' : 'обычно');
    const text = messageInput.value.trim();
    if (text && socketManager.isConnected()) {
        console.log('✅ Сообщение готово к отправке:', text);
        messageRenderer.render(text, 'user');
        sessionManager.updateSessionData('history', [
            ...sessionManager.loadSession().history,
            { role: 'user', content: text }
        ]);

        socketManager.send({
            type: 'query',
            session_id: sessionManager.getSessionId(),
            data: {
                text: text,
                force_plan: forcePlan
            }
        });
        console.log('🚀 Сообщение отправлено на сервер');

        messageInput.value = '';
        showAgentThought('Анализирую запрос...');
    } else {
        console.log('❌ Не отправлено:', text ? 'нет соединения' : 'нет текста');
    }
}

// --- Session State Handler ---
function handleSessionState(data) {
    messageList.innerHTML = '';

    if (data.history && data.history.length > 0) {
        data.history.forEach(msg => {
            if (msg.role === 'user') {
                messageRenderer.render(msg.content, 'user');
            } else if (msg.role === 'assistant' && msg.content) {
                messageRenderer.render(msg.content, 'agent');
            }
        });
    } else if (data.greeting) {
        messageRenderer.render(data.greeting, 'system');
    }

    if (data.status === 'AWAITING_CONFIRMATION') {
        renderConfirmationWidget(data.confirmation_data.message, data.confirmation_data.tool_call);
    } else {
        hideAgentThought();
    }
}

// --- Event Listeners ---

sendButton.addEventListener('click', () => sendMessage(false));

planButton.addEventListener('click', () => {
    if (messageInput.value.trim()) {
        sendMessage(true);
    } else {
        alert('Пожалуйста, введите задачу перед запуском планирования.');
    }
});

autoConfirmButton.addEventListener('click', () => {
    isAutoConfirmEnabled = !isAutoConfirmEnabled;
    if (isAutoConfirmEnabled) {
        autoConfirmButton.classList.add('active');
        autoConfirmButton.title = 'Авто-подтверждение включено';
    } else {
        autoConfirmButton.classList.remove('active');
        autoConfirmButton.title = 'Авто-подтверждение выключено';
    }
});

freezeWatcherButton.addEventListener('click', () => {
    isWatcherFrozen = !isWatcherFrozen;
    const command = isWatcherFrozen ? 'freeze' : 'unfreeze';

    if (socketManager.isConnected()) {
        socketManager.send({
            type: 'control_file_watcher',
            data: { command: command }
        });
    }

    if (isWatcherFrozen) {
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
    if (confirm('Вы уверены, что хотите очистить историю сессии?')) {
        if (socketManager.isConnected()) {
            socketManager.send({
                type: 'clear_history',
                session_id: sessionManager.getSessionId()
            });
            messageList.innerHTML = '';
            messageRenderer.render('История очищена. Готов к новым задачам!', 'agent');
        }
    }
});
