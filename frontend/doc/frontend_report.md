# 📋 Подробный отчет: HTML и index.js фронтенда Smart Hammer

## 📄 1. HTML Структура (`index.html`)

### 1.1 Мета-информация
```html
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LLM Agent - Frontend</title>
</head>
```

### 1.2 Подключаемые ресурсы
| Ресурс | Тип | Описание |
|--------|-----|----------|
| Tailwind CSS | CDN | Стилизация интерфейса |
| style.css | Local | Кастомные стили |
| Sortable.js | CDN | Drag-and-drop функциональность |
| marked.js | CDN | Парсинг Markdown |

### 1.3 Основная структура

```
┌─────────────────────────────────────────────────────────────┐
│  chat-container (max-w-7xl mx-auto)                         │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ chat-header                                          │   │
│  │  ┌─────────────────┐  ┌──────────────────────────┐  │   │
│  │  │ LLM Agent       │  │ 🤖⚡ ❄️ На главную 🗑️   │  │   │
│  │  └─────────────────┘  └──────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ message-list (custom-scrollbar)                     │   │
│  │  → Сообщения будут загружены с сервера              │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ agent-thought-container (hidden)                    │   │
│  │  [spinner] Агент думает...                          │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ chat-form                                           │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ indicator-container                         │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ message-input (textarea, auto-height)       │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ [Отправить] [📋]                            │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 1.4 Встроенные шаблоны (Templates)

#### **confirmation-widget-template**
```html
<template id="confirmation-widget-template">
    <div class="message agent">
        <div class="message-content border-l-4 border-amber-500">
            <p>⚠️ Запрос на подтверждение действия:</p>
            <p data-role="prompt-text"></p>
            <pre data-role="tool-call-json"></pre>
            <div class="confirm-buttons flex gap-3">
                <button data-role="yes-button">Да, разрешить</button>
                <button data-role="no-button">Отклонить</button>
            </div>
        </div>
    </div>
</template>
```

#### **plan-confirmation-widget-template**
```html
<template id="plan-confirmation-widget-template">
    <div class="message agent">
        <div class="message-content border-l-4 border-cyan-500">
            <p>📋 Составлен пошаговый план...</p>
            <ol data-role="plan-editor-list"></ol>
            <button data-role="add-step-button">+ Добавить шаг</button>
            <div class="confirm-buttons">
                <button data-role="approve-plan-button">🚀 Утвердить и запустить</button>
                <button data-role="reject-plan-button">Отмена</button>
            </div>
        </div>
    </div>
</template>
```

#### **plan-step-item-template**
```html
<template id="plan-step-item-template">
    <li class="flex items-center gap-2">
        <span class="drag-handle">⠿</span>
        <div contenteditable="true" class="plan-step-text"></div>
        <button class="btn-remove-step">-</button>
    </li>
</template>
```

#### **plan-progress-widget-template**
```html
<template id="plan-progress-widget-template">
    <div class="message-content border-l-4 border-green-500">
        <p class="text-green-400 font-bold">⚡ Выполнение автономной кампании:</p>
        <ul data-role="progress-list"></ul>
    </div>
</template>
```

#### **error-recovery-widget-template**
```html
<template id="error-recovery-widget-template">
    <div class="message agent">
        <div class="message-content border-l-4 border-red-500">
            <p class="text-red-400">❗️ Ошибка выполнения кампании:</p>
            <pre data-role="error-message"></pre>
            <p>Выберите стратегию восстановления:</p>
            <div data-role="button-container"></div>
        </div>
    </div>
</template>
```

---

## ⚙️ 2. JavaScript Логика (`index.js`)

### 2.1 Инициализация модулей

```javascript
document.addEventListener('DOMContentLoaded', async () => {
    // Импорты модулей (ES6)
    const { SocketManager } = await import('./socket.js');
    const { StreamingManager } = await import('./streaming.js');
    const { SessionManager } = await import('./session.js');
    const { MessageRenderer } = await import('./message.js');
    const { StreamingIndicator } = await import('./streaming-indicator.js');
    const { TokenAccumulator } = await import('./token-accumulator.js');
});
```

### 2.2 Состояние приложения

```javascript
let state = {
    isAutoConfirmEnabled: false,      // Авто-подтверждение действий
    isWatcherFrozen: false,           // Заморозка индексатора
};
let streamingMessageElement = null;  // DOM-элемент текущего стриминга
```

### 2.3 Инициализация менеджеров

| Менеджер | Класс | Конфигурация |
|----------|-------|--------------|
| SocketManager | `new SocketManager()` | `ws://${window.location.host}/ws`, 5 реконнектов |
| SessionManager | `new SessionManager()` | `llm_agent_` префикс |
| StreamingManager | `new StreamingManager()` | Привязка к socketManager |
| MessageRenderer | `new MessageRenderer()` | `useMarkdown: true`, `marked: window.marked` |
| StreamingIndicator | `new StreamingIndicator()` | `indicatorContainer` |

### 2.4 Вспомогательные функции

```javascript
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
```

### 2.5 Обработчики WebSocket

#### **onOpen**
```javascript
socketManager.onOpen(() => {
    connectionStatusSpan.textContent = '🟢 Подключено';
    connectionStatusSpan.style.color = '#2ecc71';
    socketManager.send({ type: 'sync_session', session_id: sessionManager.getSessionId() });
});
```

#### **onClose**
```javascript
socketManager.onClose(() => {
    connectionStatusSpan.textContent = '🔴 Отключено';
    connectionStatusSpan.style.color = '#e74c3c';
    enableInput();
});
```

#### **onError**
```javascript
socketManager.onError((error) => {
    connectionStatusSpan.textContent = '🟠 Ошибка';
    connectionStatusSpan.style.color = '#e67e22';
    console.error('Socket Error:', error);
    enableInput();
});
```

#### **onMessage** (Главный обработчик)
```javascript
socketManager.onMessage((msg) => {
    switch (msg.type) {
        case 'session_state':
            handleSessionState(msg.data);
            break;
        case 'llm_token':
            // Обработка через StreamingManager
            break;
        case 'stream_end':
            // Завершение потока
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
            // Управление статусом индексатора
            break;
    }
});
```

### 2.6 Обработчики StreamingManager

#### **onToken**
```javascript
streamingManager.onToken((token) => {
    if (!streamingMessageElement) {
        // Первый токен: создание нового сообщения
        streamingIndicator.hide();
        hideThought();
        streamingMessageElement = messageRenderer.render('', 'agent');
    }
    
    // Обновление содержимого
    const messageBody = streamingMessageElement.querySelector('.message-body');
    if (messageBody) {
        const fullText = streamingManager.getAccumulatedText();
        messageBody.innerHTML = messageRenderer.renderMarkdown(fullText);
        messageList.scrollTop = messageList.scrollHeight;
    }
});
```

#### **onComplete**
```javascript
streamingManager.onComplete(() => {
    const finalText = streamingManager.getAccumulatedText();
    if (finalText) {
        const history = sessionManager.loadSession().history || [];
        sessionManager.updateSessionData('history', [...history, { role: 'assistant', content: finalText }]);
    }
    streamingMessageElement = null;
    enableInput();
});
```

### 2.7 Логика отправки сообщения

```javascript
function sendMessage(forcePlan = false) {
    const text = messageInput.value.trim();
    if (!text || !socketManager.isConnected()) return;

    // Отображение сообщения пользователя
    messageRenderer.render(text, 'user');
    const history = sessionManager.loadSession().history || [];
    sessionManager.updateSessionData('history', [...history, { role: 'user', content: text }]);
    
    messageInput.value = '';
    messageInput.style.height = 'auto';

    // Блокировка ввода
    disableInput();
    streamingManager.startStreaming();
    streamingIndicator.show();

    // Отправка запроса
    socketManager.send({
        type: 'query',
        session_id: sessionManager.getSessionId(),
        data: { text, force_plan }
    });
}
```

### 2.8 Обработка состояния сессии

```javascript
function handleSessionState(data) {
    messageList.innerHTML = ''; // Очистка чата
    
    const history = data.history || [];
    sessionManager.updateSessionData('history', history);

    if (history.length > 0) {
        history.forEach(msg => {
            if (msg.role === 'user' || (msg.role === 'assistant' && msg.content)) {
                messageRenderer.render(msg.content, msg.role);
            }
        });
    } else if (data.greeting) {
        messageRenderer.render(data.greeting, 'system');
    }

    if (data.status === 'AWAITING_CONFIRMATION') {
        renderConfirmationWidget(data.confirmation_data.message, data.confirmation_data.tool_call);
    } else {
        enableInput();
    }
}
```

### 2.9 Рендеринг виджетов

#### **renderConfirmationWidget**
```javascript
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
            session_id: sessionManager.getSessionId(),
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
```

### 2.10 Настройка обработчиков событий

```javascript
function setupEventListeners() {
    // Кнопки отправки
    sendButton.addEventListener('click', () => sendMessage(false));
    planButton.addEventListener('click', () => {
        if (messageInput.value.trim()) {
            sendMessage(true);
        } else {
            alert('Пожалуйста, введите задачу перед запуском планирования.');
        }
    });

    // Авто-подтверждение
    autoConfirmButton.addEventListener('click', () => {
        state.isAutoConfirmEnabled = !state.isAutoConfirmEnabled;
        autoConfirmButton.classList.toggle('active', state.isAutoConfirmEnabled);
        autoConfirmButton.title = state.isAutoConfirmEnabled ? 'Авто-подтверждение включено' : 'Авто-подтверждение выключено';
    });

    // Заморозка индексатора
    freezeWatcherButton.addEventListener('click', () => {
        state.isWatcherFrozen = !state.isWatcherFrozen;
        const command = state.isWatcherFrozen ? 'freeze' : 'unfreeze';
        socketManager.send({ type: 'control_file_watcher', data: { command } });
        freezeWatcherButton.classList.toggle('frozen', state.isWatcherFrozen);
        freezeWatcherButton.title = state.isWatcherFrozen ? 'Возобновить индексацию (заморожено)' : 'Приостановить индексацию';
    });

    // Обработка ввода
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

    // Очистка истории
    clearHistoryButton.addEventListener('click', () => {
        if (confirm('Вы уверены, что хотите очистить историю текущего чата?')) {
            socketManager.send({ type: 'clear_history', session_id: sessionManager.getSessionId() });
            messageList.innerHTML = '';
            messageRenderer.render('История очищена. Готов к новым задачам!', 'system');
        }
    });
}
```

---

## 📊 3. Диаграмма потока данных

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ПОЛЬЗОВАТЕЛЬ                                │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      index.js (DOM Events)                          │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ sendMessage() → disableInput() → streamingManager.start()   │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    SocketManager (WebSocket)                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ send({ type: 'query', session_id, data: { text, force_plan } }) │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    BACKEND (WebSocket Server)                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ AssistantRole → LLM → Token Stream                          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  StreamingManager (Token Handler)                   │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ onToken(token) → accumulateText → renderMarkdown()          │   │
│  │ onComplete() → saveToHistory → enableInput()                │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   MessageRenderer (UI Rendering)                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ renderMarkdown() → marked.parse() → sanitizeHtml()          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      DOM (Message List)                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ <div class="message agent">...</div>                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🎯 4. Ключевые особенности реализации

### 4.1 Архитектурные паттерны

| Паттерн | Применение |
|---------|------------|
| **Module Pattern** | ES6 модули для изоляции кода |
| **Observer Pattern** | Обработчики событий WebSocket |
| **Factory Pattern** | Создание виджетов из шаблонов |
| **State Pattern** | Управление состоянием приложения |
| **Singleton Pattern** | SocketManager (один экземпляр) |

### 4.2 Безопасность

```javascript
// Экранирование HTML
_escapeHtml(str) {
    return str
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
}

// Очистка вредоносного кода
_sanitizeHtml(html) {
    const tempDiv = document.createElement('div');
    tempDiv.innerHTML = html;
    const dangerousTags = ['script', 'iframe', 'object', 'embed', 'form', 'input', 'button'];
    dangerousTags.forEach(tag => {
        const elements = tempDiv.querySelectorAll(tag);
        elements.forEach(el => el.remove());
    });
    return tempDiv.innerHTML;
}
```

### 4.3 Производительность

- **Lazy Loading**: Модули загружаются по требованию
- **Streaming**: Показ ответа в реальном времени
- **Virtual DOM**: Нет, используется прямой DOM-манипуляции
- **Debouncing**: Автовысота textarea без задержек

### 4.4 UX/UI

| Фича | Описание |
|------|----------|
| **Auto-height textarea** | Поле растет с текстом |
| **Markdown support** | Форматирование через marked.js |
| **Code blocks** | Копирование одной кнопкой |
| **Drag-and-drop** | Редактирование шагов плана |
| **Status indicators** | Визуальная обратная связь |
| **Auto-reconnect** | Восстановление соединения |

---

## 📝 5. Типы сообщений WebSocket

| Тип | Отправитель | Описание |
|-----|-------------|----------|
| `query` | Frontend → Backend | Запрос пользователя |
| `llm_token` | Backend → Frontend | Токен от LLM (стриминг) |
| `stream_end` | Backend → Frontend | Завершение потока |
| `agent_thought` | Backend → Frontend | Мысль агента |
| `action_required` | Backend → Frontend | Требуется подтверждение |
| `plan_generated` | Backend → Frontend | Сгенерирован план |
| `plan_update` | Backend → Frontend | Прогресс плана |
| `plan_error` | Backend → Frontend | Ошибка плана |
| `error` | Backend → Frontend | Ошибка |
| `session_state` | Backend → Frontend | Состояние сессии |
| `file_watcher_status` | Backend → Frontend | Статус индексатора |
| `confirm_action` | Frontend → Backend | Подтверждение действия |
| `clear_history` | Frontend → Backend | Очистка истории |

---

## 🏁 6. Итог

Фронтенд Smart Hammer представляет собой **модульную, реактивную систему** на JavaScript с чётким разделением ответственности:

- **HTML**: Семантическая структура с встроенными шаблонами для виджетов
- **CSS**: Tailwind CSS + кастомные стили
- **JavaScript**: ES6 модули с паттернами проектирования
- **WebSocket**: Реальное время взаимодействие с бэкендом
- **Streaming**: Потоковая передача токенов от LLM
- **Security**: Экранирование HTML, удаление опасных тегов
- **UX**: Интерактивные виджеты, авто-подтверждение, drag-and-drop

Архитектура обеспечивает **масштабируемость**, **поддерживаемость** и **безопасность** при взаимодействии с AI-ассистентом.

---

*Документ сгенерирован автоматически на основе анализа кодовой базы проекта Smart Hammer.*
