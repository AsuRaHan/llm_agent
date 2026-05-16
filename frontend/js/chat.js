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

    // 1. Настраиваем marked перед использованием
    marked.setOptions({
        breaks: true,   // Переносы строк \n превратятся в <br>
        gfm: true       // Включаем GitHub Flavored Markdown (таблицы, зачеркивания)
    });

    // Утилита для экранирования HTML, чтобы логи и символы не ломали верстку
    function escapeHTML(str) {
        return str.replace(/&/g, '&amp;')
                  .replace(/</g, '&lt;')
                  .replace(/>/g, '&gt;')
                  .replace(/"/g, '&quot;')
                  .replace(/'/g, '&#039;');
    }

    // 2. Полностью заменяем старую функцию parseMarkdown на использование библиотеки:
    function parseMarkdown(text) {
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

    function connect() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsHost = window.location.hostname;
        const wsPort = 9000; 
        socket = new WebSocket(`${wsProtocol}//${wsHost}:${wsPort}/ws`);

        socket.onopen = () => {
            console.log('WebSocket connection established.');
            connectionStatus.textContent = 'Соединение установлено';
            connectionStatus.style.color = '#2ecc71';
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
        
        // Рендерим через безопасный кастомный парсер Markdown
        contentElement.innerHTML = parseMarkdown(text);

        messageElement.appendChild(contentElement);
        messageList.appendChild(messageElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addThoughtMessage(text) {
        const thoughtElement = document.createElement('div');
        thoughtElement.classList.add('message', 'thought');
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content', 'italic', 'text-gray-400', 'text-sm');
        contentElement.textContent = `🤔 Мысль агента: ${text}`;
        thoughtElement.appendChild(contentElement);
        messageList.appendChild(thoughtElement);
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addConfirmationWidget(promptText, toolCall) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        const contentElement = document.createElement('div');
        contentElement.className = 'message-content border-l-4 border-amber-500 bg-gray-800/40 p-4';
        
        contentElement.innerHTML = `
            <p class="font-bold text-amber-400">⚠️ Запрос на подтверждение действия:</p>
            <p class="my-2">${escapeHTML(promptText)}</p>
            <pre class="bg-gray-950 p-3 rounded font-mono text-xs text-gray-300 overflow-x-auto">${escapeHTML(JSON.stringify(toolCall.function, null, 2))}</pre>
        `;
        
        const buttonContainer = document.createElement('div');
        buttonContainer.className = 'confirm-buttons flex gap-3 mt-4';

        const yesButton = document.createElement('button');
        yesButton.className = 'px-4 py-2 rounded-md text-white bg-green-600 hover:bg-green-700 font-bold cursor-pointer';
        yesButton.textContent = 'Да, разрешить';
        yesButton.onclick = () => {
            socket.send(JSON.stringify({ type: 'confirm_action', session_id: sessionId, data: { confirmed: true } }));
            setChatInputEnabled(false);
            widgetElement.remove();
        };

        const noButton = document.createElement('button');
        noButton.className = 'px-4 py-2 rounded-md text-white bg-red-600 hover:bg-red-700 cursor-pointer';
        noButton.textContent = 'Отклонить';
        noButton.onclick = () => {
            socket.send(JSON.stringify({ type: 'confirm_action', session_id: sessionId, data: { confirmed: false } }));
            widgetElement.remove();
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
        contentElement.className = 'message-content border-l-4 border-cyan-500 bg-gray-800/30 p-4';

        let htmlContent = `<p><strong>📋 Составлен пошаговый план действий. Вы можете отредактировать его перед запуском:</strong></p>`;
        htmlContent += `<ol id="plan-editor-list" class="ml-5 pl-2 space-y-2 mt-2">`;
        steps.forEach((step, index) => {
            htmlContent += `
                <li data-step-id="${index}" class="flex items-center gap-2">
                    <div contenteditable="true" class="plan-step-text flex-grow p-1.5 border border-dashed border-gray-600 rounded bg-gray-900/60 focus:outline-none focus:border-cyan-400 text-sm text-gray-200">${escapeHTML(step)}</div>
                    <button class="btn-remove-step flex-shrink-0 bg-red-700 text-white rounded-full w-6 h-6 font-bold flex items-center justify-center hover:bg-red-600 cursor-pointer">-</button>
                </li>`;
        });
        htmlContent += `</ol>`;
        htmlContent += `<button id="btn-add-step" class="ml-7 mt-3 px-3 py-1 bg-blue-600 text-white rounded hover:bg-blue-700 text-xs cursor-pointer">+ Добавить шаг</button>`;
        
        htmlContent += `
            <div class="confirm-buttons flex gap-3 mt-5">
                <button class="btn-approve-plan px-4 py-2 rounded-md text-white bg-green-600 hover:bg-green-700 font-bold cursor-pointer">🚀 Утвердить и запустить</button>
                <button class="btn-reject-plan px-4 py-2 rounded-md text-white bg-red-600 hover:bg-red-700 cursor-pointer">Отмена</button>
            </div>`;
        
        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        messageList.appendChild(widgetElement);
        messageList.scrollTop = messageList.scrollHeight;

        setChatInputEnabled(false);
        messageInput.placeholder = "Ожидается утверждение плана действий...";

        const planList = widgetElement.querySelector('#plan-editor-list');

        planList.addEventListener('click', (e) => {
            if (e.target && e.target.classList.contains('btn-remove-step')) {
                e.target.closest('li').remove();
            }
        });

        widgetElement.querySelector('#btn-add-step').addEventListener('click', () => {
            const newStepIndex = planList.children.length;
            const newLi = document.createElement('li');
            newLi.setAttribute('data-step-id', newStepIndex);
            newLi.className = 'flex items-center gap-2';
            newLi.innerHTML = `
                <div contenteditable="true" class="plan-step-text flex-grow p-1.5 border border-dashed border-gray-600 rounded bg-gray-900/60 focus:outline-none focus:border-cyan-400 text-sm text-gray-200">Новый шаг плана...</div>
                <button class="btn-remove-step flex-shrink-0 bg-red-700 text-white rounded-full w-6 h-6 font-bold flex items-center justify-center hover:bg-red-600 cursor-pointer">-</button>
            `;
            planList.appendChild(newLi);
            newLi.querySelector('.plan-step-text').focus();
        });

        widgetElement.querySelector('.btn-approve-plan').addEventListener('click', () => {
            const editedSteps = [];
            const stepElements = widgetElement.querySelectorAll('.plan-step-text');
            stepElements.forEach(el => {
                const txt = el.textContent.trim();
                if(txt) editedSteps.push(txt);
            });

            widgetElement.querySelector('.confirm-buttons')?.remove();
            widgetElement.querySelector('#btn-add-step')?.remove();
            stepElements.forEach(el => el.setAttribute('contenteditable', 'false'));
            widgetElement.querySelectorAll('.btn-remove-step').forEach(btn => btn.remove());

            typingIndicator.style.display = 'flex';
            messageInput.placeholder = "Выполнение плана автономным агентом...";
            
            socket.send(JSON.stringify({
                type: 'confirm_plan',
                session_id: sessionId,
                data: { 
                    confirmed: true,
                    steps: editedSteps
                }
            }));
        });

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
        let progressElement = document.getElementById('active-plan-progress');
        if (!progressElement) {
            progressElement = document.createElement('div');
            progressElement.id = 'active-plan-progress';
            progressElement.classList.add('message', 'agent');
            messageList.appendChild(progressElement);
        }

        let htmlContent = `<div class="message-content border-l-4 border-green-500 bg-gray-800/40 p-4 rounded-r-lg">`;
        htmlContent += `<p class="mt-0 text-green-400 font-bold flex items-center gap-2"><span>⚡</span> Выполнение автономной кампании:</p>`;
        htmlContent += `<ul class="list-none p-0 m-0 space-y-2.5 mt-2">`;
        
        steps.forEach((step, index) => {
            let icon = '⏳'; 
            let style = 'color: #94a3b8; opacity: 0.6;'; 
            
            if (index < currentStepIndex) {
                icon = '✅'; 
                style = 'color: #10b981; text-decoration: line-through; opacity: 0.5; font-size: 0.95em;';
            } else if (index === currentStepIndex) {
                icon = '⚙️'; 
                style = 'color: #38bdf8; font-weight: bold; background: rgba(56, 189, 248, 0.08); padding: 4px 8px; rounded: 4px; display: inline-block; width: 100%;';
            }
            
            htmlContent += `<li style="${style}" class="flex items-start gap-2"><span>${icon}</span> <span>${escapeHTML(step)}</span></li>`;
        });
        
        htmlContent += `</ul></div>`;
        progressElement.innerHTML = htmlContent;
        messageList.scrollTop = messageList.scrollHeight;
    }

    function addErrorRecoveryWidget(errorMessage, recoveryOptions) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        const contentElement = document.createElement('div');
        contentElement.className = 'message-content border-l-4 border-red-500 bg-gray-800/30 p-4';

        let htmlContent = `<p class="font-bold text-red-400 flex items-center gap-1"><span>❗️</span> Ошибка выполнения кампании:</p>`;
        htmlContent += `<pre class="bg-gray-950 p-3 my-2 rounded font-mono text-xs text-red-300 overflow-x-auto whitespace-pre-wrap">${escapeHTML(errorMessage)}</pre>`;
        htmlContent += `<p class="text-sm my-2">Выберите стратегию восстановления для агента:</p>`;
        
        const buttonContainer = document.createElement('div');
        buttonContainer.className = 'confirm-buttons flex flex-wrap gap-2.5 mt-3';

        const optionMap = {
            'retry': '🔄 Повторить шаг',
            'skip': '⏭ Пропустить шаг',
            're-plan': '🗺 Перепланировать',
            'abort': '🛑 Остановить всё'
        };

        if (!recoveryOptions.includes('abort')) {
            recoveryOptions.push('abort');
        }

        recoveryOptions.forEach(option => {
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
                    session_id: sessionId,
                    data: { option: button.dataset.option }
                }));
                widgetElement.remove(); 
                setChatInputEnabled(false);
                messageInput.placeholder = "Агент обрабатывает решение...";
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
        messageList.innerHTML = ''; 
        
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

    function sendMessage(forcePlan = false) {
        const text = messageInput.value.trim();
        if (text && socket && socket.readyState === WebSocket.OPEN) {
            addMessage(text, 'user');
            messageInput.value = '';
            setChatInputEnabled(false);

            const message = {
                type: 'query',
                session_id: sessionId,
                data: { 
                    text: text,
                    force_plan: forcePlan
                }
            };
            socket.send(JSON.stringify(message));
        }
    }

    sendButton.addEventListener('click', () => sendMessage(false));

    // Кнопка для ручного запуска планирования
    const planButton = document.getElementById('plan-button');
    if (planButton) {
        planButton.addEventListener('click', () => {
            if (messageInput.value.trim()) {
                sendMessage(true);
            } else {
                alert('Пожалуйста, введите задачу перед запуском планирования.');
            }
        });
    }

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
            if (socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({ type: 'clear_history', session_id: sessionId }));
                messageList.innerHTML = '';
                addMessage('История очищена. Готов к новым задачам!', 'agent');
            }
        }
    });

    // setInterval(() => {
    //     if (socket && socket.readyState === WebSocket.OPEN) {
    //         socket.send(JSON.stringify({ type: 'ping' }));
    //     }
    // }, 30000);

    connect();
});
