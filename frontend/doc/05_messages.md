# 💬 Обработка и рендеринг сообщений

## 📋 Обзор

Модуль обработки сообщений отвечает за рендеринг, форматирование и управление сообщениями между пользователем и LLM Agent.

## 🏗️ Архитектура

### Класс MessageRenderer

```
MessageRenderer
├── Конструктор
│   ├── container: HTMLElement
│   ├── autoScroll: boolean
│   └── maxMessages: number
│
├── Методы
│   ├── renderWelcome()     # Рендер приветствия
│   ├── renderUserMessage() # Рендер сообщения пользователя
│   ├── renderAgentMessage() # Рендер сообщения агента
│   ├── renderSystemMessage() # Рендер системного сообщения
│   ├── renderError()       # Рендер ошибки
│   ├── renderMarkdown()    # Рендер markdown
│   ├── clearMessages()     # Очистка сообщений
│   ├── scrollToBottom()    # Авто-скролл
│   └── getMessages()       # Получение сообщений
│
└── Свойства
    ├── messages: Array
    └── scrollContainer: HTMLElement
```

## 📝 Реализация

### Конструктор

```javascript
class MessageRenderer {
    constructor(container, options = {}) {
        this.container = container;
        this.autoScroll = options.autoScroll ?? true;
        this.maxMessages = options.maxMessages ?? 100;
        this.messages = [];
        this.scrollContainer = container;
        
        this.init();
    }
    
    init() {
        // Установка обработчика скролла
        this.scrollContainer.addEventListener('scroll', () => {
            if (this.autoScroll && this.shouldAutoScroll()) {
                this.scrollToBottom();
            }
        });
    }
    
    shouldAutoScroll() {
        const scrollHeight = this.scrollContainer.scrollHeight;
        const scrollTop = this.scrollContainer.scrollTop;
        const clientHeight = this.scrollContainer.clientHeight;
        
        // Авто-скролл, если пользователь внизу
        return scrollHeight - scrollTop - clientHeight < 100;
    }
}
```

### Рендеринг приветствия

```javascript
renderWelcome() {
    const welcomeHtml = `
        <div class="welcome-message">
            <div class="welcome-content">
                <div class="welcome-icon">👋</div>
                <h2 class="welcome-title">Добро пожаловать!</h2>
                <p class="welcome-description">
                    Введите сообщение ниже, чтобы начать общение с LLM Agent.
                </p>
                <div class="welcome-features">
                    <h3 class="features-title">🌟 Возможности:</h3>
                    <ul class="features-list">
                        <li>Обсуждение кода и архитектуры</li>
                        <li>Генерация планов действий</li>
                        <li>Автоматическое выполнение задач</li>
                        <li>Интерактивное восстановление после ошибок</li>
                    </ul>
                </div>
            </div>
        </div>
    `;
    
    this.container.innerHTML = welcomeHtml;
    this.messages.push({
        type: 'welcome',
        content: welcomeHtml,
        timestamp: Date.now()
    });
}
```

### Рендеринг сообщения пользователя

```javascript
renderUserMessage(text) {
    const message = {
        id: `user_${Date.now()}_${Math.random()}`,
        type: 'user',
        content: text,
        timestamp: Date.now(),
        metadata: {}
    };
    
    // Добавление в массив сообщений
    this.messages.push(message);
    
    // Рендер HTML
    const messageHtml = this.createMessageHtml(message);
    this.appendMessage(messageHtml, 'user');
    
    // Сохранение состояния
    this.saveMessageState(message);
    
    // Авто-скролл
    this.scrollToBottom();
    
    return message.id;
}

createMessageHtml(message) {
    const isUser = message.type === 'user';
    
    return `
        <div class="message message-${message.type}" data-message-id="${message.id}">
            <div class="message-content">
                <div class="message-header">
                    <span class="message-author">${isUser ? 'Вы' : 'LLM Agent'}</span>
                    <span class="message-time">${this.formatTime(message.timestamp)}</span>
                </div>
                <div class="message-body">
                    ${this.escapeHtml(message.content)}
                </div>
            </div>
            ${isUser ? this.createAvatarHtml('user') : ''}
        </div>
    `;
}

formatTime(timestamp) {
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now - date;
    
    // Если меньше минуты - показываем "только что"
    if (diff < 60000) {
        return 'Только что';
    }
    
    // Если меньше часа - часы
    if (diff < 3600000) {
        return `${Math.floor(diff / 60000)} мин. назад`;
    }
    
    // Иначе - время
    return date.toLocaleTimeString('ru-RU', { 
        hour: '2-digit', 
        minute: '2-digit' 
    });
}

escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}
```

### Рендеринг сообщения агента

```javascript
renderAgentMessage(text, sources = []) {
    const message = {
        id: `agent_${Date.now()}_${Math.random()}`,
        type: 'agent',
        content: text,
        timestamp: Date.now(),
        metadata: {
            sources: sources,
            plan: null
        }
    };
    
    this.messages.push(message);
    
    const messageHtml = this.createAgentMessageHtml(message);
    this.appendMessage(messageHtml, 'agent');
    
    this.saveMessageState(message);
    this.scrollToBottom();
    
    return message.id;
}

createAgentMessageHtml(message) {
    const { content, metadata } = message;
    
    return `
        <div class="message message-agent" data-message-id="${message.id}">
            <div class="message-content">
                <div class="message-header">
                    <span class="message-author">LLM Agent</span>
                    <span class="message-time">${this.formatTime(message.timestamp)}</span>
                </div>
                <div class="message-body">
                    ${this.renderMarkdown(content)}
                </div>
                ${metadata.sources.length > 0 ? this.createSourcesHtml(metadata.sources) : ''}
            </div>
            ${this.createAvatarHtml('agent')}
        </div>
    `;
}

createSourcesHtml(sources) {
    return `
        <div class="message-sources">
            ${sources.map(source => `
                <a href="${source.url}" target="_blank" class="source-link">
                    📚 ${source.title}
                </a>
            `).join('')}
        </div>
    `;
}
```

### Рендеринг системного сообщения

```javascript
renderSystemMessage(text) {
    const message = {
        id: `system_${Date.now()}_${Math.random()}`,
        type: 'system',
        content: text,
        timestamp: Date.now()
    };
    
    this.messages.push(message);
    
    const messageHtml = `
        <div class="message message-system" data-message-id="${message.id}">
            <div class="message-content">
                <div class="message-body">
                    <p style="color: var(--color-text-muted);">
                        ${this.escapeHtml(text)}
                    </p>
                </div>
            </div>
        </div>
    `;
    
    this.appendMessage(messageHtml, 'system');
    this.scrollToBottom();
}
```

### Рендеринг ошибки

```javascript
renderError(message) {
    const messageHtml = `
        <div class="message message-error" data-message-id="error_${Date.now()}">
            <div class="message-content">
                <div class="message-body">
                    <p style="color: var(--color-error);">
                        ⚠️ ${this.escapeHtml(message)}
                    </p>
                </div>
            </div>
        </div>
    `;
    
    this.appendMessage(messageHtml, 'error');
    this.scrollToBottom();
}
```

### Рендеринг Markdown

```javascript
renderMarkdown(text) {
    // Простая реализация markdown
    let html = text
        // Заголовки
        .replace(/^### (.*$)/gim, '<h3>$1</h3>')
        .replace(/^## (.*$)/gim, '<h2>$1</h2>')
        .replace(/^# (.*$)/gim, '<h1>$1</h1>')
        // Жирный текст
        .replace(/\*\*(.*)\*\*/gim, '<strong>$1</strong>')
        .replace(/\*(.*)\*/gim, '<em>$1</em>')
        // Код
        .replace(/`(.*)`/gim, '<code>$1</code>')
        // Списки
        .replace(/^\- (.*$)/gim, '<li>$1</li>')
        // Параграфы
        .replace(/\n/gim, '<br/>');
    
    // Обертка списков
    html = html.replace(/(<li>.*<\/li>)/gim, '<ul>$1</ul>');
    
    return html;
}
```

### Очистка сообщений

```javascript
clearMessages() {
    this.container.innerHTML = '';
    this.messages = [];
    
    // Перерисовка приветствия
    this.renderWelcome();
}

clearMessagesExceptLast() {
    // Оставить только последнее сообщение
    this.container.innerHTML = '';
    this.messages = [];
}
```

### Авто-скролл

```javascript
scrollToBottom() {
    const scrollHeight = this.scrollContainer.scrollHeight;
    this.scrollContainer.scrollTop = scrollHeight;
}
```

### Сохранение состояния

```javascript
saveMessageState(message) {
    // Сохранение в localStorage
    const state = {
        messages: this.messages,
        lastUpdated: Date.now()
    };
    
    try {
        localStorage.setItem('llm_agent_messages', JSON.stringify(state));
    } catch (error) {
        console.error('Failed to save messages:', error);
    }
}

loadMessageState() {
    try {
        const saved = localStorage.getItem('llm_agent_messages');
        if (saved) {
            const state = JSON.parse(saved);
            this.messages = state.messages || [];
        }
    } catch (error) {
        console.error('Failed to load messages:', error);
    }
}
```

## 📊 Структура сообщения

```json
{
  "id": "msg_1234567890",
  "type": "user",
  "content": "Привет! Как дела?",
  "timestamp": 1704067200000,
  "metadata": {
    "sources": [],
    "plan": null,
    "attachments": [],
    "mentions": []
  }
}
```

### Типы сообщений

| Тип | Описание | Пример |
|-----|----------|--------|
| `user` | Сообщение пользователя | "Привет!" |
| `agent` | Ответ агента | "Здравствуйте!" |
| `system` | Системное сообщение | "Подключение установлено" |
| `error` | Сообщение об ошибке | "Не удалось отправить" |
| `welcome` | Приветственное сообщение | "Добро пожаловать!" |
| `plan` | План действий | { steps: [...] } |
| `progress` | Прогресс выполнения | { current: 3, total: 5 } |

## 🔄 Поток обработки

```
1. Пользователь вводит сообщение
   └── renderUserMessage(text)
       ├── Добавить в массив
       ├── Создать HTML
       ├── Отправить на сервер
       └── Ожидать ответ

2. Получение ответа от сервера
   └── handleIncomingMessage(message)
       ├── Определить тип
       ├── Вызвать соответствующий рендер
       └── Обновить UI

3. Отображение ответа
   ├── renderAgentMessage(text, sources)
   ├── Создать HTML
   ├── Добавить в контейнер
   └── Авто-скролл
```

## 🧪 Тестирование

### Юнит-тесты

```javascript
describe('MessageRenderer', () => {
    let renderer;
    let container;
    
    beforeEach(() => {
        container = document.createElement('div');
        renderer = new MessageRenderer(container);
    });
    
    test('should render user message', () => {
        renderer.renderUserMessage('Привет!');
        
        expect(container.innerHTML).toContain('message-user');
        expect(container.innerHTML).toContain('Привет!');
    });
    
    test('should render agent message', () => {
        renderer.renderAgentMessage('Здравствуйте!');
        
        expect(container.innerHTML).toContain('message-agent');
        expect(container.innerHTML).toContain('Здравствуйте!');
    });
    
    test('should format time correctly', () => {
        const now = new Date();
        const recent = new Date(now.getTime() - 300000); // 5 минут назад
        
        expect(renderer.formatTime(recent.getTime())).toContain('мин.');
    });
    
    test('should escape HTML', () => {
        const html = '<script>alert("xss")</script>';
        const escaped = renderer.escapeHtml(html);
        
        expect(escaped).not.toContain('<script>');
    });
});
```

### Интеграционные тесты

```javascript
describe('MessageRenderer Integration', () => {
    test('should handle message flow', async () => {
        const container = document.createElement('div');
        const renderer = new MessageRenderer(container);
        
        // Рендер приветствия
        renderer.renderWelcome();
        expect(container.innerHTML).toContain('Добро пожаловать!');
        
        // Рендер сообщения
        renderer.renderUserMessage('Тест');
        expect(container.innerHTML).toContain('message-user');
        
        // Авто-скролл
        expect(container.scrollTop).toBe(container.scrollHeight);
    });
});
```

## 📝 Best Practices

### 1. Валидация контента

```javascript
validateMessageContent(text) {
    // Проверка на XSS
    if (/<script/i.test(text)) {
        throw new Error('Недопустимый HTML');
    }
    
    // Проверка на длину
    if (text.length > 1000) {
        throw new Error('Слишком длинное сообщение');
    }
    
    return true;
}
```

### 2. Debounce для рендеринга

```javascript
debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

// Использование
const debouncedRender = debounce(() => {
    this.renderMessages();
}, 100);
```

### 3. Виртуальный скроллинг

```javascript
// Для больших списков сообщений
class VirtualMessageList {
    constructor(container, messages) {
        this.container = container;
        this.messages = messages;
        this.visibleMessages = [];
    }
    
    render() {
        // Рендер только видимых сообщений
        const visibleCount = Math.ceil(this.container.clientHeight / 50);
        this.visibleMessages = this.messages.slice(
            this.messages.length - visibleCount
        );
        
        this.container.innerHTML = this.visibleMessages.map(msg => 
            this.createMessageHtml(msg)
        ).join('');
    }
}
```

### 4. Анимации

```javascript
// Плавное появление сообщений
animateMessage(messageElement) {
    messageElement.style.opacity = '0';
    messageElement.style.transform = 'translateY(10px)';
    
    requestAnimationFrame(() => {
        messageElement.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
        messageElement.style.opacity = '1';
        messageElement.style.transform = 'translateY(0)';
    });
}
```

## 🚀 Оптимизация

### 1. Ленивая загрузка

```javascript
// Загрузка только последних N сообщений
loadRecentMessages(limit = 50) {
    return this.messages.slice(-limit);
}
```

### 2. Кэширование

```javascript
// Кэширование отформатированного контента
const messageCache = new Map();

cacheMessage(messageId, html) {
    messageCache.set(messageId, html);
}

getCachedMessage(messageId) {
    return messageCache.get(messageId);
}
```

### 3. Web Workers для тяжелого рендеринга

```javascript
// Рендеринг в worker
const worker = new Worker('message-renderer-worker.js');

worker.postMessage({ type: 'render', message });
worker.onmessage = (e) => {
    this.container.innerHTML = e.data.html;
};
```

## 🔧 Настройка

### Конфигурация

```javascript
const MESSAGE_CONFIG = {
    maxMessages: 100,
    autoScroll: true,
    scrollThreshold: 100, // пикселей от низа
    messageSize: 50, // высота сообщения в пикселях
    renderDebounce: 100, // мс
};
```

### Расширенная конфигурация

```javascript
class MessageRenderer {
    constructor(container, options = {}) {
        this.container = container;
        this.autoScroll = options.autoScroll ?? true;
        this.maxMessages = options.maxMessages ?? 100;
        this.scrollThreshold = options.scrollThreshold ?? 100;
        this.renderDebounce = options.renderDebounce ?? 100;
        
        this.init();
    }
}
```
