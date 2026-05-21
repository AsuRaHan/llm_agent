# ⚠️ Обработка ошибок

## 📋 Обзор

Модуль обработки ошибок отвечает за глобальную обработку ошибок, уведомления пользователей и механизмы восстановления после сбоев.

## 🏗️ Архитектура

### Глобальная обработка ошибок

```
ErrorHandler
├── Конструктор
│   ├── handlers: Array
│   └── recoveryActions: Map
│
├── Методы
│   ├── handleError(error)   # Обработка ошибки
│   ├── showNotification()   # Отображение уведомления
│   ├── showRecoveryModal()  # Модальное окно восстановления
│   ├── logError()           # Логирование ошибки
│   └── registerHandler()    # Регистрация обработчика
│
└── Свойства
    ├── handlers: Array
    └── recoveryActions: Map
```

## 📝 Реализация

### Конструктор

```javascript
class ErrorHandler {
    constructor() {
        this.handlers = [];
        this.recoveryActions = new Map();
        this.showNotifications = true;
        this.logErrors = true;
        
        this.init();
    }
    
    init() {
        // Глобальный обработчик ошибок
        window.addEventListener('error', (event) => {
            this.handleError(event.error, event);
        });
        
        // Глобальный обработчик unhandled promise rejections
        window.addEventListener('unhandledrejection', (event) => {
            this.handleError(event.reason, event);
        });
    }
}
```

### Обработка ошибки

```javascript
handleError(error, event) {
    // Логирование
    if (this.logErrors) {
        this.logError(error, event);
    }
    
    // Вызов обработчиков
    this.handlers.forEach(handler => {
        try {
            handler(error, event);
        } catch (handlerError) {
            console.error('Error handler failed:', handlerError);
        }
    });
    
    // Показ уведомления
    if (this.showNotifications && !event.defaultPrevented) {
        this.showNotification(error, event);
    }
    
    // Предотвращение всплытия
    if (event) {
        event.preventDefault();
    }
}

logError(error, event) {
    const errorInfo = {
        timestamp: Date.now(),
        message: error?.message || 'Unknown error',
        stack: error?.stack,
        type: error?.constructor?.name,
        url: window.location.href,
        userAgent: navigator.userAgent,
        ...event?.detail
    };
    
    console.error('[Error]', errorInfo);
    
    // Отправка на сервер
    if (this.socketManager) {
        this.socketManager.send('error', {
            error: errorInfo,
            sessionId: this.sessionManager?.sessionId
        }).catch(() => {
            // Игнорируем ошибки отправки логов
        });
    }
}
```

### Отображение уведомления

```javascript
showNotification(error, event) {
    const message = this.formatErrorMessage(error);
    const type = this.getErrorType(error);
    
    this.showNotificationElement({
        type,
        message,
        duration: 5000,
        position: 'top-right'
    });
}

formatErrorMessage(error) {
    if (error instanceof Error) {
        return error.message;
    }
    
    if (typeof error === 'string') {
        return error;
    }
    
    return 'Произошла неизвестная ошибка';
}

getErrorType(error) {
    const message = error?.message?.toLowerCase() || '';
    
    if (message.includes('network') || message.includes('connection')) {
        return 'network';
    }
    
    if (message.includes('timeout')) {
        return 'timeout';
    }
    
    if (message.includes('404') || message.includes('not found')) {
        return 'not_found';
    }
    
    if (message.includes('500') || message.includes('server')) {
        return 'server';
    }
    
    return 'error';
}

showNotificationElement(notification) {
    const { type, message, duration, position } = notification;
    
    const notificationEl = document.createElement('div');
    notificationEl.className = `notification notification-${type}`;
    notificationEl.innerHTML = `
        <div class="notification-content">
            <span class="notification-icon">
                ${this.getNotificationIcon(type)}
            </span>
            <span class="notification-message">${this.escapeHtml(message)}</span>
            <button class="notification-close" aria-label="Закрыть">✕</button>
        </div>
    `;
    
    // Добавление в DOM
    const container = this.getNotificationContainer(position);
    container.appendChild(notificationEl);
    
    // Автозакрытие
    if (duration) {
        setTimeout(() => {
            this.dismissNotification(notificationEl);
        }, duration);
    }
    
    // Закрытие по клику
    notificationEl.querySelector('.notification-close')?.addEventListener('click', () => {
        this.dismissNotification(notificationEl);
    });
}

getNotificationContainer(position) {
    const containers = {
        'top-right': document.querySelector('.notifications-top-right') || 
                      this.createNotificationContainer('top-right'),
        'top-left': document.querySelector('.notifications-top-left') || 
                    this.createNotificationContainer('top-left'),
        'bottom-right': document.querySelector('.notifications-bottom-right') || 
                        this.createNotificationContainer('bottom-right'),
        'bottom-left': document.querySelector('.notifications-bottom-left') || 
                       this.createNotificationContainer('bottom-left')
    };
    
    return containers[position] || containers['top-right'];
}

createNotificationContainer(position) {
    const container = document.createElement('div');
    container.className = `notifications-${position}`;
    document.body.appendChild(container);
    return container;
}

getNotificationIcon(type) {
    const icons = {
        error: '⚠️',
        network: '🌐',
        timeout: '⏱️',
        not_found: '🔍',
        server: '🖥️'
    };
    return icons[type] || '⚠️';
}

escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

dismissNotification(element) {
    element.style.opacity = '0';
    element.style.transform = 'translateY(-10px)';
    
    setTimeout(() => {
        element.remove();
    }, 300);
}
```

### Модальное окно восстановления

```javascript
showRecoveryModal(error) {
    const message = this.formatErrorMessage(error);
    const recoveryOptions = this.getRecoveryOptions(error);
    
    const modalHtml = `
        <div class="modal-overlay" id="recoveryModalOverlay">
            <div class="modal-container">
                <div class="modal-header">
                    <h3>⚠️ Произошла ошибка</h3>
                    <button class="modal-close" id="recoveryModalClose">✕</button>
                </div>
                <div class="modal-body">
                    <p class="error-message">${this.escapeHtml(message)}</p>
                    ${recoveryOptions.length > 0 ? this.createRecoveryOptionsHtml(recoveryOptions) : ''}
                </div>
                <div class="modal-footer">
                    <button class="btn btn-secondary" id="recoveryModalCancel">
                        Закрыть
                    </button>
                    <button class="btn btn-primary" id="recoveryModalRetry">
                        Попробовать снова
                    </button>
                </div>
            </div>
        </div>
    `;
    
    document.body.insertAdjacentHTML('beforeend', modalHtml);
    
    // Привязка событий
    this.bindRecoveryModalEvents();
}

createRecoveryOptionsHtml(options) {
    return `
        <div class="recovery-options">
            ${options.map((option, index) => `
                <button class="btn btn-secondary recovery-option" data-index="${index}">
                    ${this.escapeHtml(option.label)}
                </button>
            `).join('')}
        </div>
    `;
}

bindRecoveryModalEvents() {
    // Кнопка "Попробовать снова"
    document.getElementById('recoveryModalRetry')?.addEventListener('click', () => {
        this.retryAction();
    });
    
    // Кнопка "Закрыть"
    document.getElementById('recoveryModalCancel')?.addEventListener('click', () => {
        this.closeModal();
    });
    
    // Кнопка закрытия
    document.getElementById('recoveryModalClose')?.addEventListener('click', () => {
        this.closeModal();
    });
    
    // Кнопки восстановления
    document.querySelectorAll('.recovery-option').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const index = e.target.dataset.index;
            this.executeRecoveryAction(index);
        });
    });
    
    // Закрытие по клику на оверлей
    document.getElementById('recoveryModalOverlay')?.addEventListener('click', (e) => {
        if (e.target.id === 'recoveryModalOverlay') {
            this.closeModal();
        }
    });
}

retryAction() {
    // Попытка повторить действие
    if (this.socketManager) {
        this.socketManager.connect().then(() => {
            this.closeModal();
        }).catch(() => {
            this.showNotification({
                type: 'error',
                message: 'Не удалось восстановить подключение'
            });
        });
    }
}

executeRecoveryAction(index) {
    const action = this.recoveryActions.get(index);
    if (action && action.callback) {
        action.callback();
        this.closeModal();
    }
}

closeModal() {
    const modal = document.getElementById('recoveryModalOverlay');
    if (modal) {
        modal.remove();
    }
}

getRecoveryOptions(error) {
    const message = error?.message?.toLowerCase() || '';
    
    if (message.includes('network') || message.includes('connection')) {
        return [
            {
                label: 'Перезагрузить страницу',
                callback: () => window.location.reload()
            },
            {
                label: 'Попробовать позже',
                callback: () => {
                    this.closeModal();
                }
            }
        ];
    }
    
    if (message.includes('timeout')) {
        return [
            {
                label: 'Повторить запрос',
                callback: () => this.retryAction()
            }
        ];
    }
    
    if (message.includes('404')) {
        return [
            {
                label: 'Вернуться на главную',
                callback: () => window.location.href = '/'
            }
        ];
    }
    
    return [];
}
```

### Регистрация обработчика

```javascript
registerHandler(handler) {
    this.handlers.push(handler);
}

// Пример использования
app.registerErrorHandler((error, event) => {
    // Кастомная обработка
    console.error('Custom error handler:', error);
});
```

## 📊 Типы ошибок

| Тип | Описание | Код |
|-----|----------|-----|
| Network | Ошибка сети | `NETWORK_ERROR` |
| Timeout | Таймаут | `TIMEOUT_ERROR` |
| Not Found | Ресурс не найден | `NOT_FOUND_ERROR` |
| Server | Ошибка сервера | `SERVER_ERROR` |
| Client | Ошибка клиента | `CLIENT_ERROR` |
| Validation | Ошибка валидации | `VALIDATION_ERROR` |
| Auth | Ошибка аутентификации | `AUTH_ERROR` |

## 🔄 Механизмы восстановления

### 1. Автоматический реконнект

```javascript
class AutoReconnect {
    constructor(socketManager) {
        this.socketManager = socketManager;
        this.maxAttempts = 5;
        this.attempts = 0;
        this.delay = 1000;
    }
    
    onDisconnect() {
        if (this.attempts >= this.maxAttempts) {
            console.error('Max reconnection attempts reached');
            return;
        }
        
        this.attempts++;
        console.log(`Reconnecting... (attempt ${this.attempts})`);
        
        setTimeout(() => {
            this.socketManager.connect().catch(() => {
                this.onDisconnect();
            });
        }, this.delay);
    }
}
```

### 2. Ретраи запросов

```javascript
class RetryHandler {
    constructor(maxRetries = 3, baseDelay = 1000) {
        this.maxRetries = maxRetries;
        this.baseDelay = baseDelay;
    }
    
    async executeWithRetry(fn) {
        let lastError;
        
        for (let i = 0; i < this.maxRetries; i++) {
            try {
                return await fn();
            } catch (error) {
                lastError = error;
                
                if (i < this.maxRetries - 1) {
                    await this.delay(i);
                }
            }
        }
        
        throw lastError;
    }
    
    delay(retryNumber) {
        return new Promise(resolve => {
            const delay = this.baseDelay * Math.pow(2, retryNumber);
            setTimeout(resolve, delay);
        });
    }
}
```

### 3. Fallback механизмы

```javascript
class FallbackHandler {
    constructor(primaryFn, fallbackFn) {
        this.primaryFn = primaryFn;
        this.fallbackFn = fallbackFn;
    }
    
    async execute() {
        try {
            return await this.primaryFn();
        } catch (error) {
            console.warn('Primary function failed, using fallback');
            return this.fallbackFn();
        }
    }
}
```

## 🧪 Тестирование

### Юнит-тесты

```javascript
describe('ErrorHandler', () => {
    let errorHandler;
    
    beforeEach(() => {
        errorHandler = new ErrorHandler();
    });
    
    test('should format error message', () => {
        const error = new Error('Test error');
        const message = errorHandler.formatErrorMessage(error);
        
        expect(message).toBe('Test error');
    });
    
    test('should determine error type', () => {
        const networkError = new Error('Network error');
        const type = errorHandler.getErrorType(networkError);
        
        expect(type).toBe('network');
    });
    
    test('should get recovery options for network error', () => {
        const networkError = new Error('Connection failed');
        const options = errorHandler.getRecoveryOptions(networkError);
        
        expect(options).toHaveLength(2);
        expect(options[0].label).toBe('Перезагрузить страницу');
    });
});
```

### Интеграционные тесты

```javascript
describe('ErrorHandler Integration', () => {
    test('should show notification on error', () => {
        const errorHandler = new ErrorHandler();
        
        // Mock DOM
        document.body.innerHTML = '<div class="notifications-top-right"></div>';
        
        errorHandler.showNotification({
            type: 'error',
            message: 'Test error'
        });
        
        expect(document.body.innerHTML).toContain('notification-error');
    });
});
```

## 📝 Best Practices

### 1. Централизованное логирование

```javascript
class Logger {
    static error(message, data = {}) {
        console.error(`[Error] ${message}`, data);
        // Отправка на сервер
    }
    
    static warn(message, data = {}) {
        console.warn(`[Warning] ${message}`, data);
    }
    
    static info(message, data = {}) {
        console.info(`[Info] ${message}`, data);
    }
}
```

### 2. Обертка для асинхронных операций

```javascript
async function withErrorHandling(fn, options = {}) {
    try {
        const result = await fn();
        return { success: true, data: result };
    } catch (error) {
        const errorHandler = new ErrorHandler();
        errorHandler.handleError(error);
        
        return {
            success: false,
            error,
            retry: options.retry ?? true
        };
    }
}

// Использование
const result = await withErrorHandling(
    async () => await socketManager.send('query', { text }),
    { retry: true }
);

if (result.success) {
    // Обработка успеха
} else {
    // Обработка ошибки
}
```

### 3. Обработка ошибок в компонентах

```javascript
class MessageRenderer {
    async renderMessage(message) {
        try {
            return await this.renderMessageContent(message);
        } catch (error) {
            const errorHandler = new ErrorHandler();
            errorHandler.handleError(error);
            
            return this.renderError('Не удалось отобразить сообщение');
        }
    }
}
```

### 4. Глобальная обработка ошибок API

```javascript
class ApiClient {
    async request(endpoint, options = {}) {
        try {
            const response = await fetch(endpoint, options);
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            return await response.json();
        } catch (error) {
            const errorHandler = new ErrorHandler();
            errorHandler.handleError(error);
            
            throw error;
        }
    }
}
```

## 🚀 Оптимизация

### 1. Отложенное логирование

```javascript
// Отложенная отправка логов
const logQueue = [];
const MAX_LOG_SIZE = 10 * 1024 * 1024; // 10MB

function queueLog(error) {
    logQueue.push(error);
    
    if (logQueue.length > 100) {
        flushLogs();
    }
}

function flushLogs() {
    if (logQueue.length === 0) return;
    
    const totalSize = JSON.stringify(logQueue).length;
    
    if (totalSize < MAX_LOG_SIZE) {
        sendLogsToServer(logQueue);
        logQueue.length = 0;
    }
}
```

### 2. Сжатие логов

```javascript
import { deflateSync } from 'zlib';

function compressLogs(logs) {
    return deflateSync(JSON.stringify(logs));
}

function decompressLogs(data) {
    return JSON.parse(inflate(data));
}
```

### 3. Агрегация ошибок

```javascript
class ErrorAggregator {
    constructor(window = 60000) {
        this.window = window;
        this.errors = [];
    }
    
    add(error) {
        this.errors.push({
            timestamp: Date.now(),
            error
        });
        
        // Очистка старых ошибок
        this.errors = this.errors.filter(
            e => Date.now() - e.timestamp < this.window
        );
    }
    
    getSummary() {
        const grouped = {};
        
        this.errors.forEach(({ error }) => {
            const key = error.message;
            if (!grouped[key]) {
                grouped[key] = 0;
            }
            grouped[key]++;
        });
        
        return grouped;
    }
}
```

## 🔧 Настройка

### Конфигурация

```javascript
const ERROR_CONFIG = {
    showNotifications: true,
    logErrors: true,
    notificationDuration: 5000,
    maxNotificationCount: 3,
    autoReconnect: true,
    maxReconnectAttempts: 5,
    reconnectDelay: 1000,
};
```

### Расширенная конфигурация

```javascript
class ErrorHandler {
    constructor(options = {}) {
        this.showNotifications = options.showNotifications ?? true;
        this.logErrors = options.logErrors ?? true;
        this.notificationDuration = options.notificationDuration ?? 5000;
        this.maxNotificationCount = options.maxNotificationCount ?? 3;
        this.autoReconnect = options.autoReconnect ?? true;
        this.maxReconnectAttempts = options.maxReconnectAttempts ?? 5;
        this.reconnectDelay = options.reconnectDelay ?? 1000;
        
        this.init();
    }
}
```
