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
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;

        socket = new WebSocket(wsUrl);

        socket.onopen = () => {
            console.log('WebSocket connection established');
            connectionStatus.textContent = '● Онлайн';
            connectionStatus.classList.remove('offline');
            // Request full session state from backend
            socket.send(JSON.stringify({ type: 'sync_session', session_id: sessionId }));
        };

        socket.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            typingIndicator.style.display = 'none';

            switch (msg.type) {
                case 'session_state':
                    handleSessionState(msg.data);
                    break;
                case 'agent_thought':
                    addThoughtMessage(msg.data.message);
                    break;
                case 'action_required':
                    addConfirmationWidget(msg.data.message, msg.data.tool_call);
                    break;
                case 'query_response':
                    addMessage(msg.data.answer, 'agent');
                    setChatInputEnabled(true);
                    break;
                case 'error':
                    addMessage(`⚠️ Ошибка от сервера: ${msg.data.message}`, 'error');
                    setChatInputEnabled(true);
                    break;
                case 'pong':
                    break;
                default:
                    console.log('Received unknown message type:', msg.type, msg.data);
                    setChatInputEnabled(true);
            }
        };

        socket.onclose = () => {
            console.log('WebSocket connection closed. Reconnecting...');
            connectionStatus.textContent = '● Оффлайн';
            connectionStatus.classList.add('offline');
            setTimeout(connect, 3000);
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            socket.close();
        };
    }

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
        if (!userMessage || socket.readyState !== WebSocket.OPEN) return;

        addMessage(userMessage, 'user');
        typingIndicator.style.display = 'flex';
        setChatInputEnabled(false);

        socket.send(JSON.stringify({
            type: 'query',
            session_id: sessionId,
            data: { text: userMessage }
        }));

        messageInput.value = '';
    });

    clearChatBtn.addEventListener('click', () => {
        if (confirm('Вы уверены, что хотите очистить историю чата?')) {
            messageList.innerHTML = '';
            addMessage('Привет! Я Smart Hammer, ваш AI-ассистент. Чем могу помочь с вашим проектом?', 'agent');
            if (socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({ type: 'clear_history', session_id: sessionId }));
            }
        }
    });

    function handleSessionState(data) {
        messageList.innerHTML = '';

        if (data.history && Array.isArray(data.history)) {
            data.history.forEach(msg => {
                if (msg.role === 'user' || msg.role === 'assistant') {
                    addMessage(msg.content, msg.role === 'user' ? 'user' : 'agent');
                }
            });
        }

        if (messageList.children.length === 0 && data.greeting) {
            addMessage(data.greeting, 'agent');
        }

        switch (data.status) {
            case 'thinking':
                setChatInputEnabled(false);
                typingIndicator.style.display = 'flex';
                break;
            case 'awaiting_confirmation':
                if (data.confirmation_data) {
                    addConfirmationWidget(data.confirmation_data.message, data.confirmation_data.tool_call);
                }
                break;
            case 'idle':
            default:
                setChatInputEnabled(true);
                break;
        }
    }

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
        contentElement.innerHTML = sender === 'agent' ? marked.parse(text) : text;
        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addThoughtMessage(text) {
        const thoughtElement = document.createElement('div');
        thoughtElement.classList.add('message', 'agent', 'thought');
        const sanitizedText = text.replace(/</g, "&lt;").replace(/>/g, "&gt;");
        thoughtElement.innerHTML = `<div class="message-content">⚙️ <i></i></div>`;
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
            htmlContent += `<pre><code>${JSON.stringify(toolCall, null, 2)}</code></pre>`;
        }
        htmlContent += `
            <div class="confirm-buttons" style="display: flex; gap: 10px; margin-top: 12px;">
                <button class="btn-approve">Да, разрешить</button>
                <button class="btn-deny">Отмена</button>
            </div>`;
        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        setChatInputEnabled(false);
        messageInput.placeholder = "Ожидается подтверждение...";

        widgetElement.querySelector('.btn-approve').addEventListener('click', () => sendConfirmation(true, widgetElement));
        widgetElement.querySelector('.btn-deny').addEventListener('click', () => sendConfirmation(false, widgetElement));
    }

    function sendConfirmation(isConfirmed, widgetElement) {
        widgetElement.querySelector('.confirm-buttons')?.remove();
        typingIndicator.style.display = 'flex';
        messageInput.placeholder = "Агент думает...";
        socket.send(JSON.stringify({
            type: 'confirm_action',
            session_id: sessionId,
            data: { confirmed: isConfirmed }
        }));
    }
});
