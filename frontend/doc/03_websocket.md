# 🔌 WebSocket Коммуникация

## 📋 Обзор

WebSocket обеспечивает двустороннюю коммуникацию между клиентом и сервером в реальном времени. Этот документ описывает реализацию WebSocket менеджера для LLM Agent.

## 🏗️ Архитектура

### Класс SocketManager

```
SocketManager
├── Конструктор
│   ├── sessionId: string
│   ├── ws: WebSocket
│   ├── url: string
│   ├── reconnectAttempts: number
│   ├── maxReconnectAttempts: number
│   ├── reconnectDelay: number
│   ├── messageHandlers: Array<Function>
│   ├── errorHandlers: Array<Function>
│   └── isConnecting: boolean
│
├── Методы
│   ├── connect()              # Подключение
│   ├── send(type, data)       # Отправка
│   ├── onMessage(handler)     # Подписка на сообщения
│   ├── onError(handler)       # Подписка на ошибки
│   ├── handleClose()          # Обработка закрытия
│   ├── handleError(error)     # Обработка ошибок
│   └── handleMessage(message) # Обработка входящих сообщений
│
└── События
    ├── onopen
    ├── onclose
    ├── onmessage
    └── onerror
```

## 📝 Реализация

### Конструктор

```javascript
class SocketManager {
    constructor(sessionId) {
        this.sessionId = sessionId;
        this.ws = null;
        this.url = `ws://${window.location.host}/ws`;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 1000;
        this.messageHandlers = [];
        this.errorHandlers = [];
        this.isConnecting = false;
        this.pendingMessages = [];  // Очередь сообщений
    }
}
```

### Подключение

```javascript
connect() {
    return new Promise((resolve, reject) => {
        // Если уже подключено
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            console.log('WebSocket already connected');
            resolve();
            return;
        }
        
        this.isConnecting = true;
        
        try {
            this.ws = new WebSocket(this.url);
            
            // Обработка открытия
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.isConnecting = false;
                this.reconnectAttempts = 0;
                resolve();
            };
            
            // Обработка сообщений
            this.ws.onmessage = (event) => {
                try {
                    const message = JSON.parse(event.data);
                    this.handleMessage(message);
                } catch (error) {
                    console.error('Failed to parse message:', error);
                }
            };
            
            // Обработка ошибок
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.handleError(error);
            };
            
            // Обработка закрытия
            this.ws.onclose = () => {
                console.log('WebSocket closed');
                this.isConnecting = false;
                this.handleClose();
            };
            
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.isConnecting = false;
            reject(error);
        }
    });
}
```

### Отправка сообщения

```javascript
send(type, data) {
    // Проверка подключения
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
        console.error('WebSocket not connected');
        return Promise.reject(new Error('WebSocket not connected'));
    }
    
    // Формирование сообщения
    const message = {
        type,
        sessionId: this.sessionId,
        timestamp: Date.now(),
        ...data
    };
    
    return new Promise((resolve, reject) => {
        const messageStr = JSON.stringify(message);
        this.ws.send(messageStr);
        
        // Добавление в очередь
        this.pendingMessages.push({
            resolve,
            reject,
            messageId: Date.now() + Math.random()
        });
    });
}
```

### Обработка входящих сообщений

```javascript
handleMessage(message) {
    // Вызов всех зарегистрированных обработчиков
    this.messageHandlers.forEach(handler => {
        try {
            handler(message);
        } catch (error) {
            console.error('Error in message handler:', error);
        }
    });
}

onMessage(handler) {
    this.messageHandlers.push(handler);
}
```

### Обработка ошибок

```javascript
handleError(error) {
    console.error('Socket error:', error);
    this.onError(error);
}

onError(handler) {
    this.errorHandlers.push(handler);
}
```

### Реконнект

```javascript
handleClose() {
    // Если достигнут лимит попыток
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
        console.error('Max reconnection attempts reached');
        this.handleError(new Error('Max reconnection attempts reached'));
        return;
    }
    
    this.reconnectAttempts++;
    console.log(`Reconnecting in ${this.reconnectDelay}ms... (attempt ${this.reconnectAttempts})`);
    
    // Планирование реконнекта
    setTimeout(() => {
        this.connect().catch(error => {
            console.error('Reconnection failed:', error);
            this.handleError(error);
        });
    }, this.reconnectDelay);
}
```

## 📊 Состояния WebSocket

```
CLOSED (3) → CONNECTING (0) → OPEN (1) → CLOSING (2) → CLOSED (3)
```

### Проверка состояния

```javascript
WebSocket.CLOSED = 3;
WebSocket.CONNECTING = 0;
WebSocket.OPEN = 1;
WebSocket.CLOSING = 2;

// Проверка подключения
if (this.ws.readyState === WebSocket.OPEN) {
    // Подключено
} else if (this.ws.readyState === WebSocket.CONNECTING) {
    // Подключение...
} else if (this.ws.readyState === WebSocket.CLOSING) {
    // Закрытие...
} else if (this.ws.readyState === WebSocket.CLOSED) {
    // Закрыто
}
```

## 🔐 Безопасность

### 1. Валидация URL

```javascript
validateWebSocketUrl(url) {
    const wsUrlPattern = /^wss?:\/\/[^\/]+/i;
    return wsUrlPattern.test(url);
}
```

### 2. Проверка CORS

```javascript
// На сервере нужно настроить CORS
// Для клиента - проверка заголовков
checkCORS() {
    const allowedOrigins = ['https://example.com'];
    const currentOrigin = window.location.origin;
    return allowedOrigins.includes(currentOrigin);
}
```

### 3. HTTPS для production

```javascript
// Для production использовать wss:// вместо ws://
const isSecure = window.location.protocol === 'https:';
const wsProtocol = isSecure ? 'wss' : 'ws';
const url = `${wsProtocol}://${window.location.host}/ws`;
```

## 🔄 Очередь сообщений

```javascript
// Структура очереди
pendingMessages = [
    {
        resolve: Function,
        reject: Function,
        messageId: number
    }
];

// Обработка ответа сервера
handleResponse(message) {
    const { messageId, type } = message;
    
    // Поиск в очереди
    const pendingIndex = this.pendingMessages.findIndex(
        msg => msg.messageId === messageId
    );
    
    if (pendingIndex !== -1) {
        const pending = this.pendingMessages[pendingIndex];
        
        if (type === 'success') {
            pending.resolve(message.data);
        } else {
            pending.reject(new Error(message.error));
        }
        
        // Удаление из очереди
        this.pendingMessages.splice(pendingIndex, 1);
    }
}
```

## ⚙️ Настройки

### Конфигурация по умолчанию

```javascript
const DEFAULT_CONFIG = {
    maxReconnectAttempts: 5,
    reconnectDelay: 1000,
    reconnectDelayMultiplier: 2,  // Увеличение задержки
    messageTimeout: 30000,        // Таймаут сообщения
    maxMessageSize: 10000,        // Максимальный размер сообщения
};
```

### Расширенная конфигурация

```javascript
class SocketManager {
    constructor(sessionId, config = {}) {
        this.sessionId = sessionId;
        this.ws = null;
        this.url = `ws://${window.location.host}/ws`;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = config.maxReconnectAttempts || DEFAULT_CONFIG.maxReconnectAttempts;
        this.reconnectDelay = config.reconnectDelay || DEFAULT_CONFIG.reconnectDelay;
        this.messageHandlers = [];
        this.errorHandlers = [];
        this.isConnecting = false;
        this.pendingMessages = [];
        this.messageTimeout = config.messageTimeout || DEFAULT_CONFIG.messageTimeout;
    }
}
```

## 🧪 Тестирование

### Юнит-тесты

```javascript
describe('SocketManager', () => {
    let socketManager;
    
    beforeEach(() => {
        socketManager = new SocketManager('test-session');
    });
    
    test('should initialize with correct values', () => {
        expect(socketManager.sessionId).toBe('test-session');
        expect(socketManager.maxReconnectAttempts).toBe(5);
        expect(socketMessageHandlers).toHaveLength(0);
    });
    
    test('should reject when not connected', async () => {
        await expect(socketManager.send('test', { data: 'test' }))
            .rejects.toThrow('WebSocket not connected');
    });
    
    test('should call message handlers', () => {
        const handler = jest.fn();
        socketManager.onMessage(handler);
        
        const message = { type: 'test', data: 'test' };
        socketManager.handleMessage(message);
        
        expect(handler).toHaveBeenCalledWith(message);
    });
    
    test('should handle reconnection', () => {
        // Тест реконнекта
    });
});
```

### Интеграционные тесты

```javascript
describe('SocketManager Integration', () => {
    test('should connect and send message', async () => {
        const socketManager = new SocketManager('integration-test');
        
        // Mock WebSocket
        const mockWs = {
            readyState: WebSocket.OPEN,
            send: jest.fn(),
            onopen: null,
            onclose: null,
            onmessage: null,
            onerror: null
        };
        
        socketManager.ws = mockWs;
        
        await socketManager.connect();
        await socketManager.send('test', { data: 'test' });
        
        expect(mockWs.send).toHaveBeenCalledWith(
            JSON.stringify({
                type: 'test',
                sessionId: 'integration-test',
                timestamp: expect.any(Number),
                data: 'test'
            })
        );
    });
});
```

## 📝 Best Practices

### 1. Обработка ошибок

```javascript
// Всегда обрабатывать ошибки
try {
    await socketManager.send('query', { text: message });
} catch (error) {
    console.error('Failed to send message:', error);
    // Показать пользователю
    showError('Не удалось отправить сообщение');
}
```

### 2. Ограничение частоты отправки

```javascript
// Debounce для предотвращения спама
let lastSendTime = 0;
const SEND_THROTTLE = 1000; // 1 секунда

function throttledSend(type, data) {
    const now = Date.now();
    if (now - lastSendTime < SEND_THROTTLE) {
        return;
    }
    lastSendTime = now;
    socketManager.send(type, data);
}
```

### 3. Очистка ресурсов

```javascript
// При уничтожении компонента
destroy() {
    if (this.socketManager) {
        this.socketManager.ws.close();
        this.socketManager = null;
    }
}
```

### 4. Логирование

```javascript
// Структурированное логирование
logMessage(level, message, data = {}) {
    console[level](`[${level.toUpperCase()}] ${message}`, data);
}

// Использование
logMessage('info', 'Message sent', { type, sessionId });
logMessage('error', 'Connection failed', { error, attempts });
```

## 🚀 Оптимизация

### 1. Кэширование

```javascript
// Кэширование часто используемых сообщений
const messageCache = new Map();

getCacheKey(type, data) {
    return `${type}:${JSON.stringify(data)}`;
}

// Проверка кэша перед отправкой
if (messageCache.has(key)) {
    // Использовать кэшированный ответ
}
```

### 2. Бatching

```javascript
// Группировка сообщений
let messageBuffer = [];
let bufferTimer = null;

function bufferMessage(type, data) {
    messageBuffer.push({ type, data });
    
    clearTimeout(bufferTimer);
    bufferTimer = setTimeout(() => {
        socketManager.send('batch', { messages: messageBuffer });
        messageBuffer = [];
    }, 1000);
}
```

### 3. Сжатие

```javascript
// Сжатие больших сообщений
import { deflateSync } from 'zlib';

function compressMessage(message) {
    return deflateSync(JSON.stringify(message));
}
```

## 🔧 Отладка

### 1. Включить подробное логирование

```javascript
const DEBUG_MODE = true;

if (DEBUG_MODE) {
    console.log('WebSocket state:', this.ws.readyState);
    console.log('Pending messages:', this.pendingMessages.length);
    console.log('Message handlers:', this.messageHandlers.length);
}
```

### 2. Отладка подключения

```javascript
// Отслеживание всех событий
this.ws.addEventListener('open', () => console.log('🟢 Open'));
this.ws.addEventListener('close', () => console.log('🔴 Close'));
this.ws.addEventListener('error', () => console.log('🟠 Error'));
this.ws.addEventListener('message', (e) => console.log('💬 Message:', e.data));
```

### 3. Проверка сети

```javascript
// Проверка доступности WebSocket сервера
async function checkWebSocketAvailability() {
    try {
        const response = await fetch('/ws', { method: 'HEAD' });
        return response.ok;
    } catch (error) {
        console.error('WebSocket server not available');
        return false;
    }
}
```
