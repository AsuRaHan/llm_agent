document.addEventListener('DOMContentLoaded', () => {
    const chatForm = document.getElementById('chat-form');
    const messageInput = document.getElementById('message-input');
    const messageList = document.getElementById('message-list');
    const typingIndicator = document.getElementById('typing-indicator');
    const clearChatBtn = document.getElementById('clear-chat');
    const connectionStatus = document.getElementById('connection-status');
    let socket;

    function connect() {
        // Create WebSocket connection.
        socket = new WebSocket(`ws://${window.location.host}/ws`);

        socket.onopen = () => {
            console.log('WebSocket connection established');
            connectionStatus.textContent = '● Онлайн';
            connectionStatus.style.color = '#4caf50';
        };

        socket.onmessage = (event) => {
            typingIndicator.style.display = 'none';
            const msg = JSON.parse(event.data);

            if (msg.type === 'query_response') {
                addMessage(msg.data.answer, 'agent');
                saveChatHistory();
            } else if (msg.type === 'error') {
                addMessage(`⚠️ Ошибка от сервера: ${msg.data.message}`, 'error');
            } else {
                console.log('Received unknown message type:', msg.type);
            }
        };

        socket.onclose = (event) => {
            console.log('WebSocket connection closed. Reconnecting in 3 seconds...');
            connectionStatus.textContent = '● Переподключение...';
            connectionStatus.style.color = '#f39c12';
            setTimeout(connect, 3000); // Attempt to reconnect
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            connectionStatus.textContent = '● Ошибка';
            connectionStatus.style.color = '#ff6b6b';
            socket.close(); // This will trigger onclose and the reconnect logic
        };
    }

    clearChatBtn.addEventListener('click', () => {
        if (confirm('Очистить историю чата?')) {
            localStorage.removeItem('chat_history');
            addWelcomeMessage();
        }
    });

    chatForm.addEventListener('submit', (event) => {
        event.preventDefault();

        const userMessage = messageInput.value.trim();
        if (!userMessage) return;

        // Display user message
        addMessage(userMessage, 'user');
        messageInput.value = '';

        // Show typing indicator
        typingIndicator.style.display = 'flex';

        if (socket.readyState !== WebSocket.OPEN) {
            addMessage('⚠️ Не удалось отправить сообщение. Соединение не установлено.', 'error');
            typingIndicator.style.display = 'none';
            return;
        }

        const payload = {
            type: 'query',
            data: { text: userMessage }
        };
        socket.send(JSON.stringify(payload));
        saveChatHistory();
    });

    function addMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        messageElement.setAttribute('data-timestamp', new Date().toLocaleTimeString());

        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        
        // Use marked.js for Markdown rendering
        const formattedText = marked.parse(text);
        
        contentElement.innerHTML = formattedText;
        
        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);

        // Scroll to the bottom with animation
        messageList.scrollTo({
            top: messageList.scrollHeight,
            behavior: 'smooth'
        });
    }

    function addWelcomeMessage() {
        messageList.innerHTML = '';
        const welcomeMessage = document.createElement('div');
        welcomeMessage.classList.add('message', 'agent');
        welcomeMessage.innerHTML = `
            <div class="message-content">
                <p>Привет! Я Smart Hammer, ваш AI-ассистент. Чем могу помочь с вашим проектом?</p>
            </div>
        `;
        messageList.appendChild(welcomeMessage);
    }

    function saveChatHistory() {
        // Don't save the initial welcome message
        const messagesToSave = Array.from(document.querySelectorAll('.message')).slice(1);
        const messages = messagesToSave.map(msg => {
            const sender = msg.classList.contains('user') ? 'user' : (msg.classList.contains('error') ? 'error' : 'agent');
            // Save raw text content to re-render with marked on load
            const rawText = msg.querySelector('.message-content').__rawText || msg.querySelector('.message-content').innerText;
            return { sender, text: rawText };
        });
        localStorage.setItem('chat_history', JSON.stringify(messages));
    }

    function loadChatHistory() {
        const history = localStorage.getItem('chat_history');
        if (history) {
            try {
                const messages = JSON.parse(history);
                messages.forEach(msg => {
                    addMessage(msg.text, msg.sender);
                });
                messageList.scrollTop = messageList.scrollHeight;
            } catch (e) {
                console.error('Error loading chat history:', e);
                localStorage.removeItem('chat_history'); // Clear corrupted history
                addWelcomeMessage();
            }
        } else {
            addWelcomeMessage();
        }
    }

    // Initial setup
    loadChatHistory();
    connect();
    messageInput.focus();
});