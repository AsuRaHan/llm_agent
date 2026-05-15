document.addEventListener('DOMContentLoaded', () => {
    const messageList = document.getElementById('message-list');
    const messageInput = document.getElementById('message-input');
    const sendButton = document.getElementById('send-button');
    const clearHistoryButton = document.getElementById('clear-history-button');
    const typingIndicator = document.getElementById('typing-indicator');
    const connectionStatus = document.getElementById('connection-status');

    let socket;
    let sessionId = localStorage.getItem('sessionId');
    if (!sessionId) {
        sessionId = 'sess_' + Math.random().toString(36).substr(2, 9);
        localStorage.setItem('sessionId', sessionId);
    }

    function connect() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsHost = window.location.hostname;
        const wsPort = 9000; // Порт WebSocket сервера
        socket = new WebSocket(`${wsProtocol}//${wsHost}:${wsPort}/ws`);

        socket.onopen = () => {
            console.log('WebSocket connection established.');
            connectionStatus.textContent = 'Соединение установлено';
            connectionStatus.style.color = '#2ecc71';
            // Запрашиваем состояние сессии при подключении
            socket.send(JSON.stringify({ type: 'sync_session', session_id: sessionId }));
        };

        socket.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            console.log('Received:', msg);

            switch (msg.type) {
                case 'session_state':
                    handleSessionState(msg.data);
                    break;
                case 'query_response':
                    // Убираем id, чтобы при следующем запросе создался новый блок прогресса
                    const oldProgress = document.getElementById('active-plan-progress');
                    if (oldProgress) oldProgress.removeAttribute('id');

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

        socket.onclose = () => {
            console.log('WebSocket connection closed. Reconnecting...');
            connectionStatus.textContent = 'Переподключение...';
            connectionStatus.style.color = '#e67e22';
            setTimeout(connect, 3000); // Попытка переподключения через 3 секунды
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            connectionStatus.textContent = 'Ошибка соединения';
            connectionStatus.style.color = '#e74c3c';
        };
    }

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
        
        // Using a simple regex to format code blocks
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
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content', 'confirmation');
        
        contentElement.innerHTML = `<p>${promptText}</p><pre>${JSON.stringify(toolCall.function, null, 2)}</pre>`;
        
        const buttonContainer = document.createElement('div');
        buttonContainer.classList.add('confirm-buttons');

        const yesButton = document.createElement('button');
        yesButton.textContent = 'Да';
        yesButton.onclick = () => {
            socket.send(JSON.stringify({ type: 'confirm_action', session_id: sessionId, data: { confirmed: true } }));
            setChatInputEnabled(false);
            buttonContainer.remove();
        };

        const noButton = document.createElement('button');
        noButton.textContent = 'Нет';
        noButton.onclick = () => {
            socket.send(JSON.stringify({ type: 'confirm_action', session_id: sessionId, data: { confirmed: false } }));
            setChatInputEnabled(true);
            buttonContainer.remove();
        };

        buttonContainer.appendChild(yesButton);
        buttonContainer.appendChild(noButton);
        contentElement.appendChild(buttonContainer);
        widgetElement.appendChild(contentElement);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;
        setChatInputEnabled(false);
    }

    function addPlanConfirmationWidget(steps) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.style.borderLeft = '4px solid #3498db'; // Выделяем синим цветом планирование

        let htmlContent = `<p><strong>📋 Я составил пошаговый план для выполнения вашей задачи:</strong></p>`;
        htmlContent += `<ol style="margin-left: 20px; padding: 0 0 0 10px;">`;
        steps.forEach((step) => {
            htmlContent += `<li style="margin-bottom: 6px; color: #2c3e50;">${step}</li>`;
        });
        htmlContent += `</ol>`;
        htmlContent += `
            <div class="confirm-buttons" style="display: flex; gap: 10px; margin-top: 12px;">
                <button class="btn-approve-plan" style="background-color: #2ecc71; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; font-weight: bold;">🚀 Утвердить и запустить</button>
                <button class="btn-reject-plan" style="background-color: #e74c3c; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer;">Отмена</button>
            </div>`;
        
        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        setChatInputEnabled(false);
        messageInput.placeholder = "Ожидается утверждение плана действий...";

        // Нажатие на "Утвердить"
        widgetElement.querySelector('.btn-approve-plan').addEventListener('click', () => {
            widgetElement.querySelector('.confirm-buttons')?.remove();
            typingIndicator.style.display = 'flex';
            messageInput.placeholder = "Выполнение плана автономным агентом...";
            
            socket.send(JSON.stringify({
                type: 'confirm_plan',
                session_id: sessionId,
                data: { confirmed: true }
            }));
        });

        // Нажатие на "Отмена"
        widgetElement.querySelector('.btn-reject-plan').addEventListener('click', () => {
            widgetElement.querySelector('.confirm-buttons')?.remove();
            addMessage('Вы отклонили план. Вы можете скорректировать задачу или задать другой вопрос.', 'agent');
            setChatInputEnabled(true);
            
            socket.send(JSON.stringify({
                type: 'confirm_plan',
                session_id: sessionId,
                data: { confirmed: false }
            }));
        });
    }

    function updatePlanProgress(currentStepIndex, steps) {
        // Пытаемся найти уже существующий на экране контейнер прогресса, либо создаем новый
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
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.style.borderLeft = '4px solid #e74c3c'; // Red for error

        let htmlContent = `<p><strong><span style="color: #e74c3c;">❗️</span> Ошибка выполнения плана:</strong></p>`;
        htmlContent += `<p style="background: #2d2d2d; padding: 8px; border-radius: 4px; font-family: monospace; font-size: 0.9em;">${errorMessage}</p>`;
        htmlContent += `<p>Что мне делать дальше?</p>`;
        
        const buttonContainer = document.createElement('div');
        buttonContainer.classList.add('confirm-buttons');
        buttonContainer.style.display = 'flex';
        buttonContainer.style.gap = '10px';
        buttonContainer.style.marginTop = '12px';

        const optionMap = {
            'retry': 'Повторить',
            'skip': 'Пропустить шаг',
            're-plan': 'Перепланировать',
            'abort': 'Отменить план'
        };

        // Ensure 'abort' is always an option
        if (!recoveryOptions.includes('abort')) {
            recoveryOptions.push('abort');
        }

        recoveryOptions.forEach(option => {
            const button = document.createElement('button');
            button.textContent = optionMap[option] || option;
            button.dataset.option = option;
            button.style.padding = '8px 16px';
            button.style.borderRadius = '4px';
            button.style.border = 'none';
            button.style.cursor = 'pointer';
            button.style.backgroundColor = (option === 'abort' || option === 'skip') ? '#c0392b' : '#2980b9';
            if (option === 'retry') button.style.backgroundColor = '#27ae60';

            button.addEventListener('click', () => {
                socket.send(JSON.stringify({
                    type: 'confirm_error_recovery',
                    session_id: sessionId,
                    data: { option: button.dataset.option }
                }));
                widgetElement.remove(); // Remove the widget after a choice is made
                setChatInputEnabled(false);
                messageInput.placeholder = "Агент обрабатывает ваше решение...";
            });
            buttonContainer.appendChild(button);
        });

        contentElement.innerHTML = htmlContent;
        contentElement.appendChild(buttonContainer);
        widgetElement.appendChild(contentElement);
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
        if (text && socket && socket.readyState === WebSocket.OPEN) {
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
            socket.send(JSON.stringify(message));
        }
    }

    sendButton.addEventListener('click', sendMessage);
    messageInput.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') {
            sendMessage();
        }
    });

    clearHistoryButton.addEventListener('click', () => {
        if (confirm('Вы уверены, что хотите очистить историю сессии?')) {
            if (socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({ type: 'clear_history', session_id: sessionId }));
                messageList.innerHTML = '';
                addMessage('История очищена. Готов к новым задачам!', 'agent');
            }
        }
    });

    // Keep-alive
    setInterval(() => {
        if (socket && socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({ type: 'ping' }));
        }
    }, 30000);

    connect();
});