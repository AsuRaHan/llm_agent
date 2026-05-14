document.addEventListener('DOMContentLoaded', () => {
    const messageList = document.getElementById('message-list');
    const chatForm = document.getElementById('chat-form');
    const messageInput = document.getElementById('message-input');
    const typingIndicator = document.getElementById('typing-indicator');
    const connectionStatus = document.getElementById('connection-status');
    const clearChatBtn = document.getElementById('clear-chat');
    const sendButton = chatForm.querySelector('button');

    let socket;

    // Generate or retrieve a unique session ID for this browser tab
    const sessionId = localStorage.getItem('agent_session_id') || 'sess_' + Math.random().toString(36).substr(2, 9);
    localStorage.setItem('agent_session_id', sessionId);

    function connect() {
        // Use wss:// for secure connections if the page is loaded via https
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;

        socket = new WebSocket(wsUrl);

        socket.onopen = () => {
            console.log('WebSocket connection established');
            connectionStatus.textContent = '● Онлайн';
            connectionStatus.classList.remove('offline');
            // Request project info on connect
            socket.send(JSON.stringify({ type: 'get_project_info', session_id: sessionId }));
            loadChatHistory();
        };

        socket.onmessage = (event) => {
            typingIndicator.style.display = 'none';
            // Re-enable input when a response is received
            setChatInputEnabled(true);
            const msg = JSON.parse(event.data);

            switch (msg.type) {
                case 'agent_thought':
                    addThoughtMessage(msg.data.message);
                    break;
                case 'action_required':
                    addConfirmationWidget(msg.data.message, msg.data.tool_call);
                    break;
                case 'query_response':
                    addMessage(msg.data.answer, 'agent');
                    saveChatHistory();
                    break;
                case 'project_info_response':
                    // Replace the initial greeting with the one from the server
                    const firstMessage = messageList.querySelector('.message.agent .message-content');
                    if (firstMessage && firstMessage.innerText.includes('Чем могу помочь')) {
                        firstMessage.innerHTML = marked.parse(msg.data.greeting);
                    } else {
                        addMessage(msg.data.greeting, 'agent');
                    }
                    saveChatHistory();
                    break;
                case 'error':
                    addMessage(`⚠️ Ошибка от сервера: ${msg.data.message}`, 'error');
                    break;
                case 'pong':
                    // console.log('Pong received');
                    break;
                default:
                    console.log('Received unknown message type:', msg.type, msg.data);
            }
        };

        socket.onclose = () => {
            console.log('WebSocket connection closed. Reconnecting...');
            connectionStatus.textContent = '● Оффлайн';
            connectionStatus.classList.add('offline');
            // Attempt to reconnect every 3 seconds
            setTimeout(connect, 3000);
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            socket.close();
        };
    }

    // Initial connection
    connect();

    // Keep connection alive
    setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({ type: 'ping' }));
        }
    }, 30000);

    chatForm.addEventListener('submit', (e) => {
        e.preventDefault();
        const userMessage = messageInput.value.trim();
        if (!userMessage || socket.readyState !== WebSocket.OPEN) {
            return;
        }

        addMessage(userMessage, 'user');
        saveChatHistory();

        typingIndicator.style.display = 'flex';

        // Disable input while waiting for a response
        setChatInputEnabled(false);

        const payload = {
            type: 'query',
            session_id: sessionId,
            data: { text: userMessage }
        };
        socket.send(JSON.stringify(payload));

        messageInput.value = '';
    });

    clearChatBtn.addEventListener('click', () => {
        if (confirm('Вы уверены, что хотите очистить историю чата?')) {
            // Clear UI and local storage first for immediate feedback
            messageList.innerHTML = '';
            localStorage.removeItem(`chat_history_${sessionId}`);

            // Add back the initial greeting
            addMessage('Привет! Я Smart Hammer, ваш AI-ассистент. Чем могу помочь с вашим проектом?', 'agent');
            
            // Save the new "clean" state to local storage
            saveChatHistory();

            // Send a message to the backend to clear its session history
            if (socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({ type: 'clear_history', session_id: sessionId }));
            }
        }
    });

    function setChatInputEnabled(enabled) {
        messageInput.disabled = !enabled;
        sendButton.disabled = !enabled;
        messageInput.placeholder = enabled ? "Введите ваш вопрос..." : "Агент думает...";
    }

    function addMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);

        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        // Use marked.parse to render markdown from agent
        contentElement.innerHTML = sender === 'agent' ? marked.parse(text) : text;

        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addThoughtMessage(text) {
        const thoughtElement = document.createElement('div');
        thoughtElement.classList.add('message', 'agent');
        thoughtElement.style.opacity = '0.6';

        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        // Sanitize text before inserting as HTML
        const sanitizedText = text.replace(/</g, "&lt;").replace(/>/g, "&gt;");
        contentElement.innerHTML = `⚙️ <i>${sanitizedText}</i>`;

        thoughtElement.appendChild(contentElement);
        messageList.appendChild(thoughtElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addConfirmationWidget(message, toolCall) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');

        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.style.borderLeft = '4px solid #f39c12';

        let htmlContent = `<p><strong>${marked.parse(message)}</strong></p>`;

        if (toolCall) {
            const toolCallString = JSON.stringify(toolCall, null, 2);
            htmlContent += `<pre><code>${toolCallString}</code></pre>`;
        }

        htmlContent += `
            <div class="confirm-buttons" style="display: flex; gap: 10px; margin-top: 12px;">
                <button id="btn-approve" style="background-color: #388e3c; padding: 6px 16px; border-radius: 4px; font-size: 0.9em;">Да, разрешить</button>
                <button id="btn-deny" style="background-color: #d32f2f; padding: 6px 16px; border-radius: 4px; font-size: 0.9em;">Отмена</button>
            </div>
        `;

        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        // Disable input, but with a specific message for confirmation
        setChatInputEnabled(false);
        messageInput.placeholder = "Ожидается подтверждение...";

        widgetElement.querySelector('#btn-approve').addEventListener('click', () => {
            sendConfirmation(true, widgetElement);
        });

        widgetElement.querySelector('#btn-deny').addEventListener('click', () => {
            sendConfirmation(false, widgetElement);
        });
    }

    function sendConfirmation(isConfirmed, widgetElement) {
        const btnBlock = widgetElement.querySelector('.confirm-buttons');
        if (btnBlock) btnBlock.remove();

        typingIndicator.style.display = 'flex';

        const payload = {
            type: 'confirm_action',
            session_id: sessionId,
            data: { confirmed: isConfirmed }
        };

        socket.send(JSON.stringify(payload));
    }

    function saveChatHistory() {
        localStorage.setItem(`chat_history_${sessionId}`, messageList.innerHTML);
    }

    function loadChatHistory() {
        const history = localStorage.getItem(`chat_history_${sessionId}`);
        if (history) {
            messageList.innerHTML = history;
            // Re-attach event listeners for confirmation buttons if any exist
            messageList.querySelectorAll('.confirm-buttons').forEach(btnBlock => {
                const widgetElement = btnBlock.closest('.message.agent');
                if (widgetElement) {
                     widgetElement.querySelector('#btn-approve')?.addEventListener('click', () => {
                        sendConfirmation(true, widgetElement);
                    });
                    widgetElement.querySelector('#btn-deny')?.addEventListener('click', () => {
                        sendConfirmation(false, widgetElement);
                    });
                }
            });
            messageList.scrollTop = messageList.scrollHeight;
        }
    }
});