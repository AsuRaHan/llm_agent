# 📝 Управление сессиями

## 📋 Обзор

Система управления сессиями отвечает за создание, хранение и управление уникальными идентификаторами сессий для каждого взаимодействия пользователя с LLM Agent.

## 🏗️ Архитектура

### Класс SessionManager

```
SessionManager
├── Конструктор
│   ├── storage: Storage
│   ├── prefix: string
│   └── sessionId: string
│
├── Методы
│   ├── getSessionId()      # Получение ID сессии
│   ├── createSession()     # Создание новой сессии
│   ├── saveState()         # Сохранение состояния
│   ├── loadState()         # Загрузка состояния
│   ├── clearSession()      # Очистка сессии
│   ├── getState()          # Получение состояния
│   └── setState()          # Установка состояния
│
└── События
    ├── onSessionCreated
    └── onSessionExpired
```

## 📝 Реализация

### Конструктор

```javascript
class SessionManager {
    constructor() {
        this.storage = window.localStorage;
        this.prefix = 'llm_agent_';
        this.sessionId = null;
        this.sessionData = {};
        
        this.init();
    }
    
    async init() {
        // Попытка восстановить существующую сессию
        this.sessionId = this.getSessionId();
        
        if (this.sessionId) {
            this.sessionData = this.loadState();
            console.log('Session restored:', this.sessionId);
        } else {
            await this.createSession();
        }
    }
}
```

### Получение ID сессии

```javascript
getSessionId() {
    const sessionId = this.storage.getItem(`${this.prefix}session_id`);
    
    if (sessionId) {
        // Проверка на истечение срока
        if (this.isSessionExpired(sessionId)) {
            this.storage.removeItem(`${this.prefix}session_id`);
            return null;
        }
        
        return sessionId;
    }
    
    return null;
}

isSessionExpired(sessionId) {
    const sessionData = this.storage.getItem(`${this.prefix}session_${sessionId}`);
    if (!sessionData) return true;
    
    const { expiresAt } = JSON.parse(sessionData);
    return Date.now() > expiresAt;
}
```

### Создание новой сессии

```javascript
createSession() {
    return new Promise((resolve, reject) => {
        try {
            // Генерация уникального ID
            this.sessionId = this.generateSessionId();
            
            // Установка срока действия (24 часа)
            const expiresAt = Date.now() + 24 * 60 * 60 * 1000;
            
            // Сохранение ID
            this.storage.setItem(`${this.prefix}session_id`, this.sessionId);
            
            // Сохранение начального состояния
            this.saveState({
                createdAt: Date.now(),
                expiresAt,
                messages: [],
                settings: {}
            });
            
            console.log('New session created:', this.sessionId);
            this.emit('onSessionCreated', { sessionId: this.sessionId });
            
            resolve(this.sessionId);
        } catch (error) {
            console.error('Failed to create session:', error);
            reject(error);
        }
    });
}

generateSessionId() {
    // Генерация UUID v4
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
        const r = Math.random() * 16 | 0;
        const v = c === 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}
```

### Сохранение состояния

```javascript
saveState(data) {
    try {
        const expiresAt = Date.now() + 24 * 60 * 60 * 1000;
        
        const sessionData = {
            ...data,
            expiresAt,
            updatedAt: Date.now()
        };
        
        this.storage.setItem(`${this.prefix}session_${this.sessionId}`, JSON.stringify(sessionData));
        this.sessionData = data;
        
        console.log('Session state saved');
    } catch (error) {
        console.error('Failed to save session state:', error);
    }
}
```

### Загрузка состояния

```javascript
loadState() {
    try {
        const sessionData = this.storage.getItem(`${this.prefix}session_${this.sessionId}`);
        
        if (!sessionData) {
            return {
                messages: [],
                settings: {},
                createdAt: Date.now(),
                expiresAt: Date.now() + 24 * 60 * 60 * 1000
            };
        }
        
        const data = JSON.parse(sessionData);
        
        // Проверка на истечение срока
        if (data.expiresAt && Date.now() > data.expiresAt) {
            this.clearSession();
            return null;
        }
        
        return data;
    } catch (error) {
        console.error('Failed to load session state:', error);
        return {
            messages: [],
            settings: {}
        };
    }
}
```

### Очистка сессии

```javascript
clearSession() {
    try {
        // Удаление всех данных сессии
        this.storage.removeItem(`${this.prefix}session_id`);
        this.storage.removeItem(`${this.prefix}session_${this.sessionId}`);
        
        // Сброс состояния
        this.sessionId = null;
        this.sessionData = {
            messages: [],
            settings: {}
        };
        
        console.log('Session cleared');
        this.emit('onSessionExpired', { sessionId: this.sessionId });
    } catch (error) {
        console.error('Failed to clear session:', error);
    }
}
```

### Установка состояния

```javascript
setState(key, value) {
    if (this.sessionData) {
        this.sessionData[key] = value;
        this.saveState(this.sessionData);
    }
}

getState(key) {
    return this.sessionData?.[key];
}

getState() {
    return this.sessionData;
}
```

## 🔄 Жизненный цикл сессии

```
┌─────────────────┐
│  Session Start  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Create Session │
│  Generate ID    │
│  Save Initial   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Active State   │
│  Save Updates   │
│  Check Expiry   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Session End    │
│  Clear Data     │
│  New Session    │
└─────────────────┘
```

## 📊 Структура данных сессии

```json
{
  "sessionId": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "createdAt": 1704067200000,
  "expiresAt": 1704153600000,
  "updatedAt": 1704067200000,
  "messages": [
    {
      "id": "msg_1",
      "type": "user",
      "content": "Привет!",
      "timestamp": 1704067200000,
      "metadata": {}
    },
    {
      "id": "msg_2",
      "type": "agent",
      "content": "Здравствуйте! Чем могу помочь?",
      "timestamp": 1704067201000,
      "metadata": {
        "sources": [],
        "plan": null
      }
    }
  ],
  "settings": {
    "theme": "light",
    "fontSize": "medium",
    "language": "ru"
  },
  "metadata": {
    "userAgent": "Mozilla/5.0...",
    "platform": "web",
    "version": "1.0.0"
  }
}
```

## 🔐 Безопасность

### 1. Валидация ID

```javascript
validateSessionId(id) {
    // Проверка формата UUID
    const uuidPattern = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
    return uuidPattern.test(id);
}
```

### 2. Защита от XSS

```javascript
sanitizeSessionData(data) {
    // Эскейп HTML в данных сессии
    const escapeHtml = (text) => {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    };
    
    return JSON.parse(JSON.stringify(data)).map(item => ({
        ...item,
        content: escapeHtml(item.content)
    }));
}
```

### 3. Ограничение размера

```javascript
MAX_SESSION_SIZE = 10 * 1024 * 1024; // 10MB

checkSessionSize() {
    const size = JSON.stringify(this.sessionData).length;
    return size < MAX_SESSION_SIZE;
}

// Очистка при превышении
if (!this.checkSessionSize()) {
    this.clearOldMessages();
}
```

## 🧪 Тестирование

### Юнит-тесты

```javascript
describe('SessionManager', () => {
    let sessionManager;
    
    beforeEach(() => {
        sessionManager = new SessionManager();
    });
    
    test('should generate unique session ID', () => {
        const id1 = sessionManager.generateSessionId();
        const id2 = sessionManager.generateSessionId();
        
        expect(id1).not.toBe(id2);
        expect(id1).toMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i);
    });
    
    test('should create new session', async () => {
        const sessionId = await sessionManager.createSession();
        
        expect(sessionId).toBeDefined();
        expect(sessionId).toBeTruthy();
        
        const storedId = localStorage.getItem('llm_agent_session_id');
        expect(storedId).toBe(sessionId);
    });
    
    test('should save and load state', () => {
        const testData = { messages: [], settings: {} };
        sessionManager.saveState(testData);
        
        const loadedState = sessionManager.loadState();
        expect(loadedState).toEqual(testData);
    });
    
    test('should clear session', () => {
        const sessionId = sessionManager.generateSessionId();
        localStorage.setItem('llm_agent_session_id', sessionId);
        
        sessionManager.clearSession();
        
        expect(localStorage.getItem('llm_agent_session_id')).toBeNull();
    });
});
```

### Интеграционные тесты

```javascript
describe('SessionManager Integration', () => {
    test('should handle session expiry', async () => {
        const sessionManager = new SessionManager();
        const sessionId = await sessionManager.createSession();
        
        // Установить истекший срок
        const expiredData = {
            sessionId,
            expiresAt: Date.now() - 1000,
            messages: []
        };
        localStorage.setItem(
            `llm_agent_session_${sessionId}`,
            JSON.stringify(expiredData)
        );
        
        const loaded = sessionManager.loadState();
        expect(loaded).toBeNull();
    });
});
```

## 📝 Best Practices

### 1. Автоматическое сохранение

```javascript
// Автосохранение каждые 30 секунд
setInterval(() => {
    if (sessionManager.sessionId) {
        sessionManager.saveState({
            lastActivity: Date.now()
        });
    }
}, 30000);
```

### 2. Синхронизация с сервером

```javascript
// Синхронизация с сервером при подключении WebSocket
socketManager.onMessage((message) => {
    if (message.type === 'sync') {
        sessionManager.saveState(message.data);
    }
});
```

### 3. Резервное копирование

```javascript
// Экспорт сессии
exportSession() {
    const data = JSON.stringify(sessionManager.sessionData, null, 2);
    const blob = new Blob([data], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `session_${sessionManager.sessionId}.json`;
    a.click();
}

// Импорт сессии
importSession(file) {
    const reader = new FileReader();
    reader.onload = (e) => {
        try {
            const data = JSON.parse(e.target.result);
            sessionManager.sessionData = data;
            sessionManager.saveState(data);
        } catch (error) {
            console.error('Failed to import session:', error);
        }
    };
    reader.readAsText(file);
}
```

### 4. Очистка старых сессий

```javascript
// Очистка сессий старше 7 дней
cleanupOldSessions() {
    const sevenDaysAgo = Date.now() - 7 * 24 * 60 * 60 * 1000;
    
    Object.keys(localStorage).forEach(key => {
        if (key.startsWith('llm_agent_session_')) {
            const data = JSON.parse(localStorage.getItem(key));
            if (data.expiresAt < sevenDaysAgo) {
                localStorage.removeItem(key);
            }
        }
    });
}
```

## 🚀 Оптимизация

### 1. Ленивая загрузка

```javascript
// Загрузка только необходимых данных
loadMessages(limit = 50) {
    const data = this.loadState();
    return data.messages.slice(-limit);
}
```

### 2. Сжатие данных

```javascript
// Сжатие больших сообщений
compressMessages(messages) {
    return JSON.stringify(messages);
}

// Декомпрессия
decompressMessages(data) {
    return JSON.parse(data);
}
```

### 3. Индексация

```javascript
// Индексация по времени
indexMessagesByTime() {
    const indexed = {};
    
    this.sessionData.messages.forEach(msg => {
        const time = msg.timestamp;
        if (!indexed[time]) {
            indexed[time] = [];
        }
        indexed[time].push(msg);
    });
    
    return indexed;
}
```

## 🔧 Настройка

### Конфигурация

```javascript
const SESSION_CONFIG = {
    prefix: 'llm_agent_',
    storage: 'localStorage', // localStorage, sessionStorage, IndexedDB
    expiresAfter: 24 * 60 * 60 * 1000, // 24 часа
    maxSize: 10 * 1024 * 1024, // 10MB
    autoSaveInterval: 30000, // 30 секунд
    cleanupInterval: 86400000, // 24 часа
};
```

### Расширенная конфигурация

```javascript
class SessionManager {
    constructor(config = {}) {
        this.prefix = config.prefix || 'llm_agent_';
        this.storage = config.storage || localStorage;
        this.expiresAfter = config.expiresAfter || 24 * 60 * 60 * 1000;
        this.maxSize = config.maxSize || 10 * 1024 * 1000;
        this.autoSaveInterval = config.autoSaveInterval || 30000;
        
        this.init();
    }
}
```
