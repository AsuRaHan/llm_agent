document.addEventListener('DOMContentLoaded', () => {
    const chatForm = document.getElementById('chat-form');
    const messageInput = document.getElementById('message-input');
    const messageList = document.getElementById('message-list');
    const typingIndicator = document.getElementById('typing-indicator');

    chatForm.addEventListener('submit', async (event) => {
        event.preventDefault();

        const userMessage = messageInput.value.trim();
        if (!userMessage) return;

        // Display user message
        addMessage(userMessage, 'user');
        messageInput.value = '';

        // Show typing indicator
        typingIndicator.style.display = 'flex';

        try {
            // Send message to the agent's API
            const response = await fetch('/api/query', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ text: userMessage }),
            });

            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.message || `HTTP error! status: ${response.status}`);
            }

            const data = await response.json();
            
            // Display agent's response
            addMessage(data.answer, 'agent');

        } catch (error) {
            console.error('Error communicating with agent:', error);
            addMessage(`Ошибка: ${error.message}`, 'error');
        } finally {
            // Hide typing indicator
            typingIndicator.style.display = 'none';
        }
    });

    function addMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);

        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        
        // Basic markdown-like formatting for code blocks
        const formattedText = text.replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>');
        
        contentElement.innerHTML = formattedText;
        
        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);

        // Scroll to the bottom
        messageList.scrollTop = messageList.scrollHeight;
    }
});