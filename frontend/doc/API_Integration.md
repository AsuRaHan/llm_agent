# Документация API и интеграций для Frontend Streaming

## 📋 Обзор

Эта документация описывает API и интеграции для реализации потоковой передачи LLM ответов на фронтенде.

---

## 📡 WebSocket API

### 📤 Сообщения Frontend → Backend

#### 1. `query` - Новый запрос
```json
{
  "type": "query",
  "session_id": "sess_abc123",
  "data": {
    "text": "Почему не работает функция X?",
    "force_plan": false
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"query"` |
| `session_id` | string | ✅ | ID сессии |
| `data.text` | string | ✅ | Текст запроса |
| `data.force_plan` | boolean | ❌ | Принудительный план |

---

#### 2. `sync_session` - Синхронизация сессии
```json
{
  "type": "sync_session",
  "session_id": "sess_abc123"
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"sync_session"` |
| `session_id` | string | ✅ | ID сессии |

---

#### 3. `confirm_action` - Подтверждение действия
```json
{
  "type": "confirm_action",
  "session_id": "sess_abc123",
  "data": {
    "confirmed": true
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"confirm_action"` |
| `session_id` | string | ✅ | ID сессии |
| `data.confirmed` | boolean | ✅ | `true` - подтвердить, `false` - отклонить |

---

#### 4. `confirm_plan` - Подтверждение плана
```json
{
  "type": "confirm_plan",
  "session_id": "sess_abc123",
  "data": {
    "confirmed": true,
    "steps": ["Шаг 1", "Шаг 2"]
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"confirm_plan"` |
| `session_id` | string | ✅ | ID сессии |
| `data.confirmed` | boolean | ✅ | `true` - подтвердить, `false` - отклонить |
| `data.steps` | array | ❌ | Список шагов (опционально) |

---

#### 5. `confirm_error_recovery` - Выбор опции восстановления
```json
{
  "type": "confirm_error_recovery",
  "session_id": "sess_abc123",
  "data": {
    "option": "retry"
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"confirm_error_recovery"` |
| `session_id` | string | ✅ | ID сессии |
| `data.option` | string | ✅ | Опция: `retry`, `skip`, `re-plan`, `abort` |

---

#### 6. `control_file_watcher` - Управление файловым watcher
```json
{
  "type": "control_file_watcher",
  "data": {
    "command": "freeze"
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"control_file_watcher"` |
| `data.command` | string | ✅ | `freeze` или `unfreeze` |

---

#### 7. `clear_history` - Очистка истории
```json
{
  "type": "clear_history",
  "session_id": "sess_abc123"
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"clear_history"` |
| `session_id` | string | ✅ | ID сессии |

---

### 📥 Сообщения Backend → Frontend

#### 1. `session_state` - Состояние сессии
```json
{
  "type": "session_state",
  "data": {
    "history": [
      {"role": "user", "content": "Привет"},
      {"role": "assistant", "content": "Здравствуйте!"}
    ],
    "greeting": "Добро пожаловать!",
    "status": "IDLE",
    "confirmation_data": {
      "message": "Разрешить выполнение?",
      "tool_call": {...}
    }
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"session_state"` |
| `data.history` | array | ❌ | История сообщений |
| `data.greeting` | string | ❌ | Приветствие |
| `data.status` | string | ❌ | Статус: `IDLE`, `AWAITING_CONFIRMATION` |
| `data.confirmation_data` | object | ❌ | Данные подтверждения |

---

#### 2. `query_response` - Ответ (старый режим)
```json
{
  "type": "query_response",
  "data": {
    "answer": "Это ответ от LLM"
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"query_response"` |
| `data.answer` | string | ✅ | Текст ответа |

---

#### 3. `llm_token` - **Новый** - Токен LLM
```json
{
  "type": "llm_token",
  "data": {
    "token": "Прив"
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"llm_token"` |
| `data.token` | string | ✅ | Текст токена |

---

#### 4. `stream_end` - **Новый** - Завершение потока
```json
{
  "type": "stream_end"
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"stream_end"` |

---

#### 5. `agent_thought` - Мысль агента
```json
{
  "type": "agent_thought",
  "data": {
    "message": "Анализирую запрос..."
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"agent_thought"` |
| `data.message` | string | ✅ | Текст мысли |

---

#### 6. `action_required` - Требуется подтверждение
```json
{
  "type": "action_required",
  "data": {
    "message": "Разрешить выполнение?",
    "tool_call": {
      "id": "call_abc123",
      "type": "function",
      "function": {
        "name": "execute_tool",
        "arguments": {...}
      }
    }
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"action_required"` |
| `data.message` | string | ✅ | Текст запроса |
| `data.tool_call` | object | ✅ | Данные вызова инструмента |

---

#### 7. `plan_generated` - Сгенерирован план
```json
{
  "type": "plan_generated",
  "data": {
    "steps": ["Шаг 1", "Шаг 2", "Шаг 3"]
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"plan_generated"` |
| `data.steps` | array | ✅ | Список шагов плана |

---

#### 8. `plan_update` - Прогресс плана
```json
{
  "type": "plan_update",
  "data": {
    "current_step": 1,
    "steps": [
      {"text": "Шаг 1", "status": "completed"},
      {"text": "Шаг 2", "status": "current"},
      {"text": "Шаг 3", "status": "pending"}
    ]
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"plan_update"` |
| `data.current_step` | number | ✅ | Индекс текущего шага |
| `data.steps` | array | ✅ | Список шагов с статусом |

---

#### 9. `plan_error` - Ошибка плана
```json
{
  "type": "plan_error",
  "data": {
    "error_message": "Ошибка выполнения шага",
    "recovery_options": ["retry", "skip", "re-plan"]
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"plan_error"` |
| `data.error_message` | string | ✅ | Сообщение об ошибке |
| `data.recovery_options` | array | ✅ | Опции восстановления |

---

#### 10. `error` - Ошибка
```json
{
  "type": "error",
  "data": {
    "message": "Внутренняя ошибка сервера"
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"error"` |
| `data.message` | string | ✅ | Сообщение об ошибке |

---

#### 11. `pong` - Ответ на ping
```json
{
  "type": "pong"
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"pong"` |

---

#### 12. `file_watcher_status` - Статус watcher
```json
{
  "type": "file_watcher_status",
  "data": {
    "status": "indexing"
  }
}
```

**Поля:**
| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `type` | string | ✅ | Всегда `"file_watcher_status"` |
| `data.status` | string | ✅ | Статус: `indexing`, `idle` |

---

## 🧩 API Компонентов Frontend

### 1. SocketManager

**Файл:** `frontend/js/core/socket.js`
```javascript
class SocketManager {
    constructor(url, options = {}) {
        // ... реализация
    }
    
    connect() { /* ... */ }
    disconnect() { /* ... */ }
    send(message) { /* ... */ }
    
    onMessage(callback) { /* ... */ }
    offMessage(callback) { /* ... */ }
    
    startKeepAlive(interval) { /* ... */ }
    stopKeepAlive() { /* ... */ }
    
    isConnected() { /* ... */ }
    getReconnectAttempts() { /* ... */ }
    setReconnectAttempts(max) { /* ... */ }
}
```

**Пример использования:**
```javascript
const socketManager = new SocketManager('ws://localhost:9000/ws', {
    reconnectAttempts: 5,
    reconnectDelay: 3000,
    keepAliveInterval: 30000
});

socketManager.connect();

socketManager.onMessage((message) => {
    console.log('Received:', message);
});

socketManager.send({ type: 'query', session_id: 'sess_123', data: { text: 'Привет' } });
```

---

### 2. StreamingManager
**Файл:** `frontend/js/core/streaming.js`
```javascript
class StreamingManager {
    constructor(socketManager) {
        this.socketManager = socketManager;
        this.isStreaming = false;
        this.accumulatedText = '';
        // ... другие свойства и реализация
    }
    
    startStreaming() { /* ... */ }
    stopStreaming() { /* ... */ }
    // isStreaming() уже есть как свойство, но можно добавить метод для консистентности
    
    onToken(callback) { /* ... */ }
    offToken(callback) { /* ... */ }
    
    onComplete(callback) { /* ... */ }
    onError(callback) { /* ... */ }
    
    getAccumulatedText() { return this.accumulatedText; }
    clear() { this.accumulatedText = ''; }
    reset() { this.clear(); this.isStreaming = false; }
}
```

**Пример использования:**
```javascript
const streamingManager = new StreamingManager(socketManager);

streamingManager.startStreaming();

streamingManager.onToken((token) => {
    console.log('Token:', token);
    // Отправляем токен в UI
});

streamingManager.onComplete(() => {
    console.log('Stream completed');
    // Завершаем стриминг
});
```

---

### 3. TokenAccumulator
**Файл:** `frontend/js/ui/token-accumulator.js`
```javascript
class TokenAccumulator {
    constructor(container, options = {}) {
        this.container = container;
        this.text = '';
        this.isComplete = false;
        // ... другие свойства и реализация
    }
    
    appendToken(token) { /* ... */ }
    appendText(text) { /* ... */ }
    clear() { this.text = ''; this.isComplete = false; }
    getText() { return this.text; }
    // isComplete() уже есть как свойство
    markComplete() { this.isComplete = true; }
    render() { /* ... */ }
}
```

**Пример использования:**
```javascript
const tokenAccumulator = new TokenAccumulator(thoughtContainer, {
    animate: true,
    highlightCurrent: true,
    useMarkdown: true
});

// Добавляем токены
tokenAccumulator.appendToken('Прив');
tokenAccumulator.appendToken('ет');
tokenAccumulator.appendToken('!');

// Получаем текст
const fullText = tokenAccumulator.getText();
```

---

### 4. StreamingIndicator
**Файл:** `frontend/js/ui/streaming-indicator.js`
```javascript
class StreamingIndicator {
    constructor(container, options = {}) {
        this.container = container;
        this.isShowing = false;
        // ... другие свойства и реализация
    }
    
    show() { this.isShowing = true; /* ... */ }
    hide() { this.isShowing = false; /* ... */ }
    toggle() { this.isShowing = !this.isShowing; /* ... */ }
    // isShowing() уже есть как свойство
    setText(text) { /* ... */ }
    render() { /* ... */ }
}
```

**Пример использования:**
```javascript
const streamingIndicator = new StreamingIndicator(indicatorContainer, {
    dotsCount: 3,
    animationDuration: 1400,
    defaultText: 'Генерация ответа...'
});

// Показываем индикатор
streamingIndicator.show();

// Скрываем индикатор
streamingIndicator.hide();
```

---

### 5. MessageRenderer
**Файл:** `frontend/js/ui/message.js`
```javascript
class MessageRenderer {
    constructor() {
        this.useMarkdown = false;
        this.markdownParser = null; // Или инициализация Marked.js
        // ... другие свойства и реализация
    }
    
    render(text, sender) { /* ... */ }
    appendToExisting(messageElement, text) { /* ... */ }
    renderMarkdown(text) { /* ... */ }
    escapeHTML(text) { /* ... */ }
}
```

**Пример использования:**
```javascript
const messageRenderer = new MessageRenderer();
messageRenderer.useMarkdown = true;

// Рендерим новое сообщение
const messageElement = messageRenderer.render('Привет!', 'agent');
messageList.appendChild(messageElement);

// Дописываем токен
messageRenderer.appendToExisting(messageElement, 'ет');
```

---

### 6. SessionManager
**Файл:** `frontend/js/core/session.js`
```javascript
class SessionManager {
    constructor(socketManager) {
        this.socketManager = socketManager;
        this.sessionId = null;
        // ... другие свойства и реализация
    }
    
    connect() { /* ... */ }
    disconnect() { /* ... */ }
    startStreaming() { /* ... */ }
    stopStreaming() { /* ... */ }
    
    saveSession() { /* ... */ }
    loadSession() { /* ... */ }
    clearSession() { /* ... */ }
    clearStreamingState() { /* ... */ }
}
```

**Пример использования:**
```javascript
const sessionManager = new SessionManager(socketManager);

sessionManager.connect();

// Сохраняем сессию
sessionManager.saveSession();

// Очищаем состояние стрима
sessionManager.clearStreamingState();
```

---

## 🔌 Интеграция компонентов

### Схема интеграции

```
┌─────────────────────────────────────────────────────────────┐
│                    chat.js (Main)                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Инициализация:                                       │  │
│  │  - SocketManager                                     │  │
│  │  - StreamingManager                                  │  │
│  │  - TokenAccumulator                                  │  │
│  │  - StreamingIndicator                                │  │
│  │  - MessageRenderer                                   │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Обработка сообщений:                                │  │
│  │  - llm_token → StreamingManager.onToken()           │  │
│  │  - stream_end → StreamingManager.onComplete()       │  │
│  │  - agent_thought → showAgentThought()               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Пример полной интеграции

```javascript
// chat.js
import { SocketManager } from './core/socket.js';
import { StreamingManager } from './core/streaming.js';
import { TokenAccumulator } from './ui/token-accumulator.js';
import { StreamingIndicator } from './ui/streaming-indicator.js';
import { MessageRenderer } from './ui/message.js';
import { helpers } from './utils/helpers.js';

// Инициализация
const socketManager = new SocketManager('ws://localhost:9000/ws');
const streamingManager = new StreamingManager(socketManager);
const tokenAccumulator = new TokenAccumulator(thoughtContainer, {
    animate: true,
    useMarkdown: true
});
const streamingIndicator = new StreamingIndicator(indicatorContainer);
const messageRenderer = new MessageRenderer();
messageRenderer.useMarkdown = true;

// Обработка сообщений WebSocket
socketManager.onMessage((message) => {
    switch (message.type) {
        case 'llm_token':
            streamingManager.onToken((token) => {
                tokenAccumulator.appendToken(token);
                messageRenderer.render(token, 'agent');
            });
            break;
            
        case 'stream_end':
            streamingManager.onComplete(() => {
                streamingIndicator.hide();
                enableInput();
            });
            break;
            
        case 'agent_thought':
            showAgentThought(message.data.message);
            break;
            
        // ... остальные кейсы
    }
});

// Отправка запроса
function sendMessage(text) {
    socketManager.send({
        type: 'query',
        session_id: sessionManager.sessionId,
        data: { text: text, force_plan: false }
    });
    
    streamingManager.startStreaming();
    streamingIndicator.show();
}
```

---

## 🎨 Стили и анимации

### StreamingIndicator

```css
.streaming-indicator {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 12px 20px;
    background-color: #1a202c;
    border-radius: 8px;
    margin: 10px;
}

.streaming-indicator .dot {
    width: 8px;
    height: 8px;
    background-color: #a0aec0;
    border-radius: 50%;
    animation: pulse 1.4s infinite ease-in-out;
}

.streaming-indicator .dot:nth-child(1) { animation-delay: -0.32s; }
.streaming-indicator .dot:nth-child(2) { animation-delay: -0.16s; }

@keyframes pulse {
    0%, 80%, 100% { transform: scale(0); }
    40% { transform: scale(1); }
}
```

### TokenAccumulator

```css
.token-accumulator {
    display: inline;
    font-family: 'Fira Code', monospace;
}

.token-accumulator .token {
    opacity: 0.7;
    transition: opacity 0.2s;
}

.token-accumulator .token.current {
    opacity: 1;
    color: #22d3ee;
}

@keyframes type {
    0% { opacity: 0; transform: translateY(5px); }
    100% { opacity: 1; transform: translateY(0); }
}

.token-accumulator .token {
    animation: type 0.2s ease-out;
}
```

---

## 📊 Матрица событий

| Событие | Источник | Действие |
|---------|----------|----------|
| `llm_token` | Backend | Добавить токен в накопитель |
| `stream_end` | Backend | Скрыть индикатор, активировать ввод |
| `agent_thought` | Backend | Показать мысль агента |
| `action_required` | Backend | Показать виджет подтверждения |
| `plan_generated` | Backend | Показать виджет плана |
| `plan_update` | Backend | Обновить прогресс плана |
| `plan_error` | Backend | Показать виджет восстановления |
| `error` | Backend | Показать ошибку |
| `session_state` | Backend | Загрузить состояние сессии |
| `query_response` | Backend | Показать ответ (старый режим) |

---

## ⚠️ Обработка ошибок

### SocketManager

```javascript
socketManager.onMessage((message) => {
    try {
        // Обработка сообщения
    } catch (error) {
        console.error('Error processing message:', error);
    }
});

socketManager.onClose(() => {
    console.log('WebSocket closed, reconnecting...');
    socketManager.connect();
});
```

### StreamingManager

```javascript
streamingManager.onError((error) => {
    console.error('Streaming error:', error);
    streamingIndicator.hide();
    enableInput();
    addMessage(`Ошибка стриминга: ${error.message}`, 'error');
});
```

---

## 📝 Чек-лист интеграции

- [ ] SocketManager подключен
- [ ] StreamingManager инициализирован
- [ ] TokenAccumulator настроен
- [ ] StreamingIndicator настроен
- [ ] MessageRenderer настроен
- [ ] Обработка `llm_token` реализована
- [ ] Обработка `stream_end` реализована
- [ ] Обработка `agent_thought` реализована
- [ ] Обработка ошибок реализована
- [ ] Стили применены
- [ ] Тестирование пройдено

---

## 📁 Структура проекта

```
frontend/
├── index.html
├── css/
│   ├── style.css
│   └── streaming.css
├── js/
│   ├── index.js
│   ├── chat.js
│   ├── core/
│   │   ├── socket.js
│   │   ├── session.js
│   │   └── streaming.js
│   ├── ui/
│   │   ├── message.js
│   │   ├── widget.js
│   │   ├── streaming-indicator.js
│   │   └── token-accumulator.js
│   └── utils/
│       ├── markdown.js
│       └── helpers.js
└── config/
    └── api.js
```

---

## 📦 Зависимости

### Frontend (`package.json`)

```json
{
  "name": "llm-agent-frontend",
  "version": "1.0.0",
  "description": "Frontend для LLM Agent",
  "type": "module",
  "dependencies": {
    "marked": "^9.1.0",
    "sortablejs": "^1.15.0"
  },
  "devDependencies": {
    "vite": "^5.0.0",
    "typescript": "^5.0.0"
  }
}
```

### Backend (`CMakeLists.txt`)

```cmake
# Текущие зависимости
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(httplib REQUIRED)

# Новые зависимости (если понадобятся)
# find_package(Boost REQUIRED COMPONENTS asio)
```

---

## 🎯 Критерии успеха

1. ✅ Токены LLM отображаются в реальном времени
2. ✅ Индикатор стриминга работает корректно
3. ✅ Завершение потока обрабатывается правильно
4. ✅ Существующие функции не сломаны
5. ✅ Обработка ошибок работает
6. ✅ Код протестирован и документирован

---

## 📊 Статус реализации

| Компонент | Статус | Примечания |
|-----------|--------|------------|
| Backend (C++) | ✅ Завершено | 5 файлов обновлено |
| Frontend API | ✅ Завершено | Документация создана |
| Frontend Core | ⏳ В процессе | SocketManager, StreamingManager |
| Frontend UI | ⏳ В процессе | TokenAccumulator, StreamingIndicator |
| Интеграция | ⏳ В процессе | chat.js обновление |
| Тестирование | ⏳ Не начато | После реализации |

---

**Дата создания:** 2024  
**Версия:** 1.0  
**Статус:** Готов к использованию
