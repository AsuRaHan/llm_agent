/**
 * main.js - Основная логика чата
 * 
 * @module chat/main
 */

document.addEventListener('DOMContentLoaded', () => {
    const messageList = document.getElementById('message-list');
    const messageInput = document.getElementById('message-input');
    const sendButton = document.getElementById('send-button');
    const clearHistoryButton = document.getElementById('clear-history-button');
    const typingIndicator = document.getElementById('typing-indicator');
    const connectionStatus = document.getElementById('connection-status');

    // Инициализация модулей
    const sessionId = localStorage.getItem('sessionId') || 'sess_' + Math.random().toString(36).substr(2, 9);
    localStorage.setItem('sessionId', sessionId);

    const socketManager = new SocketManager(sessionId);
    const sessionManager = new SessionManager();
    const messageRenderer = new MessageRenderer();
    const widgetFactory = new WidgetFactory();

    // Обработчик сообщений WebSocket
    socketManager.onMessage = (msg) => {
        console.log('Received:', msg);

        switch (msg.type) {
            case 'session_state':
                handleSessionState(msg.data);
                break;
            case 'query_response':
                addMessage(msg.data.answer, 'agent');
                setChatInputEnabled(true);
                break;
            case 'agent_thought':
                addThoughtMessage(msg.data.message);
                break;
            case 'action_required':
                addConfirmationWidget(msg.data.message, msg.data.tool_call);
                break;
            case 'plan_generated':
                addPlanConfirmationWidget(msg.data.steps);
                break;
            case 'plan_update':
                updatePlanProgress(msg.data.current_step, msg.data.steps);
                break;
            case 'plan_error':
                addErrorRecoveryWidget(msg.data.error_message, msg.data.recovery_options);
                break;
            case 'error':
                addMessage(`Ошибка: ${msg.data.message}`, 'error');
                setChatInputEnabled(true);
                break;
            case 'pong':
                // Handled by keep-alive interval
                break;
            default:
                console.warn('Unknown message type:', msg.type);
        }
    };

    // Функции UI
    function setChatInputEnabled(enabled) {
        messageInput.disabled = !enabled;
        sendButton.disabled = !enabled;
        typingIndicator.style.display = enabled ? 'none' : 'flex';
        if (enabled) {
            messageInput.placeholder = "Спросите что-нибудь о проекте...";
            messageInput.focus();
        } else {
            messageInput.placeholder = "Агент думает...";
        }
    }

    function addMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        
        // Простой парсинг блоков кода
        const formattedText = text.replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>');
        contentElement.innerHTML = formattedText;

        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addThoughtMessage(text) {
        const thoughtElement = document.createElement('div');
        thoughtElement.classList.add('message', 'thought');
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.textContent = `🤔 ${text}`;
        thoughtElement.appendChild(contentElement);
        messageList.appendChild(thoughtElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addConfirmationWidget(promptText, toolCall) {
        const widgetElement = widgetFactory.createConfirmationWidget(promptText, toolCall);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;
        setChatInputEnabled(false);
    }

    function addPlanConfirmationWidget(steps) {
        const widgetElement = widgetFactory.createPlanWidget(steps);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        setChatInputEnabled(false);
        messageInput.placeholder = "Ожидается утверждение плана действий...";

        // Нажатие на "Утвердить"
        widgetElement.querySelector('.btn-approve-plan').addEventListener('click', () => {
            widgetElement.querySelector('.confirm-buttons')?.remove();
            typingIndicator.style.display = 'flex';
            messageInput.placeholder = "Выполнение плана автономным агентом...";
            
            socketManager.send('confirm_plan', {
                session_id: sessionId,
                data: { confirmed: true }
            });
        });

        // Нажатие на "Отмена"
        widgetElement.querySelector('.btn-reject-plan').addEventListener('click', () => {
            widgetElement.querySelector('.confirm-buttons')?.remove();
            addMessage('Вы отклонили план. Вы можете скорректировать задачу или задать другой вопрос.', 'agent');
            setChatInputEnabled(true);
            
            socketManager.send('confirm_plan', {
                session_id: sessionId,
                data: { confirmed: false }
            });
        });
    }

    function updatePlanProgress(currentStepIndex, steps) {
        let progressElement = document.getElementById('active-plan-progress');
        if (!progressElement) {
            progressElement = document.createElement('div');
            progressElement.id = 'active-plan-progress';
            progressElement.classList.add('message', 'agent');
            messageList.appendChild(progressElement);
        }

        let htmlContent = `<div class="message-content" style="border-left: 4px solid #2ecc71; background: #f4fbf7; padding: 12px; border-radius: 4px; box-shadow: 0 1px 3px rgba(0,0,0,0.05);">`;
        htmlContent += `<p style="margin-top: 0; color: #27ae60;"><strong>⚡ Выполнение автономной кампании:</strong></p>`;
        htmlContent += `<ul style="list-style: none; padding-left: 0; margin-bottom: 0;">`;
        
        steps.forEach((step, index) => {
            let icon = '⏳'; 
            let style = 'color: #7f8c8d;';
            
            if (index < currentStepIndex) {
                icon = '✅'; 
                style = 'color: #27ae60; text-decoration: line-through; opacity: 0.7;';
            } else if (index === currentStepIndex) {
                icon = '⚙️'; 
                style = 'color: #2980b9; font-weight: bold; animation: pulse 2s infinite;';
            }
            
            htmlContent += `<li style="margin-bottom: 8px; ${style}">${icon} ${step}</li>`;
        });
        
        htmlContent += `</ul></div>`;
        progressElement.innerHTML = htmlContent;
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addErrorRecoveryWidget(errorMessage, recoveryOptions) {
        const widgetElement = widgetFactory.createErrorWidget(errorMessage, recoveryOptions);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        setChatInputEnabled(false);
        messageInput.placeholder = "Ожидается решение по ошибке...";
    }

    function handleSessionState(data) {
        messageList.innerHTML = ''; // Очищаем чат
        
        if (data.history && data.history.length > 0) {
            data.history.forEach(msg => {
                if (msg.role === 'user') {
                    addMessage(msg.content, 'user');
                } else if (msg.role === 'assistant' && msg.content) {
                    addMessage(msg.content, 'agent');
                }
            });
        } else if (data.greeting) {
            addMessage(data.greeting, 'agent');
        }

        if (data.status === 'AWAITING_CONFIRMATION') {
            addConfirmationWidget(data.confirmation_data.message, data.confirmation_data.tool_call);
        } else {
            setChatInputEnabled(true);
        }
    }

    function sendMessage() {
        const text = messageInput.value.trim();
        if (text && socketManager.isConnected()) {
            addMessage(text, 'user');
            messageInput.value = '';
            setChatInputEnabled(false);

            const message = {
                type: 'query',
                session_id: sessionId,
                data: {
                    text: text
                }
            };
            socketManager.send('query', message.data);
        }
    }

    // Event listeners
    sendButton.addEventListener('click', sendMessage);
    messageInput.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') {
            sendMessage();
        }
    });

    clearHistoryButton.addEventListener('click', () => {
        if (confirm('Вы уверены, что хотите очистить историю сессии?')) {
            if (socketManager.isConnected()) {
                socketManager.send('clear_history', { session_id: sessionId });
                messageList.innerHTML = '';
                addMessage('История очищена. Готов к новым задачам!', 'agent');
            }
        }
    });

    // Start application
    socketManager.connect();
    socketManager.startKeepAlive(30000);
});
