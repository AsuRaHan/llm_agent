# 🎨 UI/UX Дизайн и компоненты

## 🎯 Дизайн-система

### Цветовая палитра

```css
/* Основные цвета */
:root {
  /* Primary */
  --color-primary: #6366f1;
  --color-primary-dark: #4f46e5;
  --color-primary-light: #818cf8;
  
  /* Secondary */
  --color-secondary: #8b5cf6;
  --color-secondary-dark: #7c3aed;
  
  /* Success */
  --color-success: #10b981;
  --color-success-dark: #059669;
  
  /* Warning */
  --color-warning: #f59e0b;
  --color-warning-dark: #d97706;
  
  /* Error */
  --color-error: #ef4444;
  --color-error-dark: #dc2626;
  
  /* Neutral */
  --color-text: #1f2937;
  --color-text-light: #6b7280;
  --color-text-muted: #9ca3af;
  --color-background: #f9fafb;
  --color-background-dark: #f3f4f6;
  --color-surface: #ffffff;
  --color-surface-dark: #f9fafb;
  --color-border: #e5e7eb;
  --color-border-light: #f3f4f6;
}

/* Темная тема */
[data-theme="dark"] {
  --color-text: #f9fafb;
  --color-text-light: #d1d5db;
  --color-text-muted: #9ca3af;
  --color-background: #111827;
  --color-background-dark: #1f2937;
  --color-surface: #1f2937;
  --color-surface-dark: #111827;
  --color-border: #374151;
  --color-border-light: #1f2937;
}
```

### Типографика

```css
:root {
  /* Шрифты */
  --font-family-base: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  --font-family-mono: 'Fira Code', 'Consolas', monospace;
  
  /* Размеры */
  --font-size-xs: 0.75rem;   /* 12px */
  --font-size-sm: 0.875rem;  /* 14px */
  --font-size-base: 1rem;    /* 16px */
  --font-size-lg: 1.125rem;  /* 18px */
  --font-size-xl: 1.25rem;   /* 20px */
  --font-size-2xl: 1.5rem;   /* 24px */
  --font-size-3xl: 1.875rem; /* 30px */
  
  /* Отступы */
  --spacing-xs: 0.25rem;   /* 4px */
  --spacing-sm: 0.5rem;    /* 8px */
  --spacing-md: 1rem;      /* 16px */
  --spacing-lg: 1.5rem;    /* 24px */
  --spacing-xl: 2rem;      /* 32px */
  --spacing-2xl: 3rem;     /* 48px */
  
  /* Тени */
  --shadow-sm: 0 1px 2px 0 rgb(0 0 0 / 0.05);
  --shadow-md: 0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1);
  --shadow-lg: 0 10px 15px -3px rgb(0 0 0 / 0.1), 0 4px 6px -4px rgb(0 0 0 / 0.1);
  --shadow-xl: 0 20px 25px -5px rgb(0 0 0 / 0.1), 0 8px 10px -6px rgb(0 0 0 / 0.1);
  
  /* Радиусы */
  --radius-sm: 0.25rem;
  --radius-md: 0.375rem;
  --radius-lg: 0.5rem;
  --radius-xl: 0.75rem;
  --radius-2xl: 1rem;
  --radius-full: 9999px;
}
```

## 🧩 Компоненты

### 1. Button (Кнопка)

```html
<!-- Базовая кнопка -->
<button class="btn btn-primary">
  Отправить
</button>

<!-- Кнопка с иконкой -->
<button class="btn btn-primary btn-icon">
  <svg>...</svg>
  Очистить
</button>

<!-- Кнопка с загрузкой -->
<button class="btn btn-primary btn-loading" disabled>
  <span class="spinner"></span>
  Отправка...
</button>

<!-- Кнопка с disabled -->
<button class="btn btn-primary" disabled>
  Отправить
</button>
```

```css
.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  padding: 0.5rem 1rem;
  font-size: 0.875rem;
  font-weight: 500;
  border: none;
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: all 0.2s ease;
}

.btn-primary {
  background: var(--color-primary);
  color: white;
}

.btn-primary:hover:not(:disabled) {
  background: var(--color-primary-dark);
}

.btn-primary:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-icon {
  padding: 0.5rem;
  width: 2.5rem;
  height: 2.5rem;
}

.btn-loading .spinner {
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}
```

### 2. Input (Поле ввода)

```html
<!-- Поле ввода -->
<div class="input-group">
  <textarea 
    id="messageInput" 
    class="input-textarea"
    placeholder="Введите сообщение..."
    rows="1"
  ></textarea>
  <span class="char-count">0/1000</span>
</div>

<!-- С кнопкой -->
<div class="input-group">
  <textarea class="input-textarea"></textarea>
  <button class="btn btn-primary">Отправить</button>
</div>
```

```css
.input-textarea {
  width: 100%;
  padding: 0.75rem 1rem;
  font-size: 1rem;
  font-family: var(--font-family-base);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  resize: none;
  transition: border-color 0.2s ease, box-shadow 0.2s ease;
  min-height: 44px;
  max-height: 200px;
}

.input-textarea:focus {
  outline: none;
  border-color: var(--color-primary);
  box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.1);
}

.input-textarea:disabled {
  background: var(--color-background-dark);
  cursor: not-allowed;
}

.char-count {
  font-size: 0.75rem;
  color: var(--color-text-muted);
  text-align: right;
  margin-top: 0.25rem;
}

.input-group {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.input-group.with-button {
  flex-direction: row;
  align-items: flex-end;
}
```

### 3. Message (Сообщение)

```html
<!-- Сообщение пользователя -->
<div class="message message-user">
  <div class="message-avatar">
    <svg>...</svg>
  </div>
  <div class="message-content">
    <div class="message-header">
      <span class="message-author">Вы</span>
      <span class="message-time">10:30</span>
    </div>
    <div class="message-body">
      <p>Привет! Как дела?</p>
    </div>
  </div>
</div>

<!-- Сообщение агента -->
<div class="message message-agent">
  <div class="message-content">
    <div class="message-header">
      <span class="message-author">LLM Agent</span>
      <span class="message-time">10:30</span>
    </div>
    <div class="message-body">
      <p>Привет! Я работаю отлично. Чем могу помочь?</p>
    </div>
    <div class="message-sources">
      <a href="#" class="source-link">Источник 1</a>
      <a href="#" class="source-link">Источник 2</a>
    </div>
  </div>
  <div class="message-avatar">
    <svg>...</svg>
  </div>
</div>

<!-- Системное сообщение -->
<div class="message message-system">
  <div class="message-content">
    <div class="message-body">
      <p style="color: var(--color-text-muted);">
        Это системное сообщение
      </p>
    </div>
  </div>
</div>

<!-- Сообщение с ошибкой -->
<div class="message message-error">
  <div class="message-content">
    <div class="message-body">
      <p style="color: var(--color-error);">
        Ошибка: Не удалось отправить сообщение
      </p>
    </div>
  </div>
</div>
```

```css
.message {
  display: flex;
  gap: 0.75rem;
  padding: var(--spacing-md) 0;
  max-width: 80%;
}

.message-user {
  flex-direction: row-reverse;
}

.message-agent {
  flex-direction: row;
}

.message-content {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  padding: var(--spacing-md) var(--spacing-lg);
  box-shadow: var(--shadow-sm);
  max-width: 100%;
}

.message-user .message-content {
  background: var(--color-primary);
  color: white;
}

.message-avatar {
  width: 36px;
  height: 36px;
  border-radius: var(--radius-full);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.message-avatar svg {
  width: 20px;
  height: 20px;
}

.message-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: var(--spacing-sm);
}

.message-author {
  font-weight: 600;
  font-size: 0.875rem;
}

.message-time {
  font-size: 0.75rem;
  color: var(--color-text-muted);
}

.message-body {
  line-height: 1.6;
  white-space: pre-wrap;
}

.message-sources {
  margin-top: var(--spacing-md);
  padding-top: var(--spacing-sm);
  border-top: 1px solid var(--color-border);
}

.source-link {
  color: var(--color-primary);
  text-decoration: none;
  font-size: 0.75rem;
}

.source-link:hover {
  text-decoration: underline;
}

.message-system .message-content {
  background: var(--color-background-dark);
  border-left: 3px solid var(--color-text-muted);
}

.message-error .message-content {
  background: #fef2f2;
  border-left: 3px solid var(--color-error);
}
```

### 4. Modal (Модальное окно)

```html
<div class="modal-overlay" id="modalOverlay">
  <div class="modal-container">
    <div class="modal-header">
      <h3 id="modalTitle">Подтверждение</h3>
      <button class="modal-close" id="modalCancelBtn">✕</button>
    </div>
    <div class="modal-body" id="modalBody">
      <p>Вы уверены, что хотите выполнить это действие?</p>
    </div>
    <div class="modal-footer">
      <button class="btn btn-secondary" id="modalCancelBtn">Отмена</button>
      <button class="btn btn-primary" id="modalConfirmBtn">Да</button>
    </div>
  </div>
</div>
```

```css
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  opacity: 0;
  visibility: hidden;
  transition: opacity 0.2s ease, visibility 0.2s ease;
}

.modal-overlay.hidden {
  opacity: 0;
  visibility: hidden;
}

.modal-overlay:not(.hidden) {
  opacity: 1;
  visibility: visible;
}

.modal-container {
  background: var(--color-surface);
  border-radius: var(--radius-xl);
  padding: var(--spacing-lg);
  max-width: 400px;
  width: 90%;
  max-height: 90vh;
  overflow-y: auto;
  transform: scale(0.9);
  transition: transform 0.2s ease;
}

.modal-overlay:not(.hidden) .modal-container {
  transform: scale(1);
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: var(--spacing-md);
}

.modal-header h3 {
  margin: 0;
  font-size: 1.25rem;
  font-weight: 600;
}

.modal-close {
  background: none;
  border: none;
  font-size: 1.5rem;
  cursor: pointer;
  color: var(--color-text-muted);
  padding: 0;
  width: 2rem;
  height: 2rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: var(--radius-md);
  transition: background 0.2s ease;
}

.modal-close:hover {
  background: var(--color-background-dark);
}

.modal-footer {
  display: flex;
  justify-content: flex-end;
  gap: var(--spacing-md);
  margin-top: var(--spacing-lg);
}
```

### 5. Connection Status (Статус подключения)

```html
<div class="connection-status">
  <span class="status-indicator" id="connectionStatus">🟡 Ожидание...</span>
  <span class="session-id" id="sessionIdDisplay" style="display: none;">
    ID: 12345...
  </span>
</div>
```

```css
.connection-status {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  font-size: 0.875rem;
  color: var(--color-text-muted);
}

.status-indicator {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
}

.status-indicator.connected {
  color: var(--color-success);
}

.status-indicator.disconnected {
  color: var(--color-error);
}

.status-indicator.error {
  color: var(--color-warning);
}

.session-id {
  font-size: 0.75rem;
  color: var(--color-text-light);
}
```

## 📱 Адаптивность

### Breakpoints

```css
/* Mobile */
@media (max-width: 640px) {
  .message {
    max-width: 100%;
  }
  
  .modal-container {
    width: 95%;
    padding: var(--spacing-md);
  }
}

/* Tablet */
@media (min-width: 641px) and (max-width: 1024px) {
  .message {
    max-width: 70%;
  }
}

/* Desktop */
@media (min-width: 1025px) {
  .message {
    max-width: 60%;
  }
}
```

### Mobile-first подход

```css
/* Базовые стили для мобильных */
.message {
  max-width: 100%;
}

/* Затем для планшетов */
@media (min-width: 641px) {
  .message {
    max-width: 70%;
  }
}

/* Затем для десктопов */
@media (min-width: 1025px) {
  .message {
    max-width: 60%;
  }
}
```

## 🎬 Анимации

```css
/* Fade in */
@keyframes fadeIn {
  from {
    opacity: 0;
    transform: translateY(10px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.message {
  animation: fadeIn 0.3s ease;
}

/* Pulse */
@keyframes pulse {
  0%, 100% {
    opacity: 1;
  }
  50% {
    opacity: 0.5;
  }
}

.loading-indicator {
  animation: pulse 1.5s ease-in-out infinite;
}

/* Slide up */
@keyframes slideUp {
  from {
    transform: translateY(100%);
    opacity: 0;
  }
  to {
    transform: translateY(0);
    opacity: 1;
  }
}

.modal-container {
  animation: slideUp 0.3s ease;
}

/* Scale */
@keyframes scaleIn {
  from {
    transform: scale(0.9);
    opacity: 0;
  }
  to {
    transform: scale(1);
    opacity: 1;
  }
}

.modal-overlay:not(.hidden) .modal-container {
  animation: scaleIn 0.2s ease;
}
```

## 🎨 Иконки

```html
<!-- SVG иконки -->
<svg class="icon" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
  <path d="M12 2L2 7l10 5 10-5-10-5z"/>
  <path d="M2 17l10 5 10-5"/>
  <path d="M2 12l10 5 10-5"/>
</svg>
```

## 📐 Grid System

```css
.container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 0 var(--spacing-md);
}

.grid {
  display: grid;
  gap: var(--spacing-md);
}

.grid-2 {
  grid-template-columns: repeat(2, 1fr);
}

.grid-3 {
  grid-template-columns: repeat(3, 1fr);
}

.grid-4 {
  grid-template-columns: repeat(4, 1fr);
}

@media (max-width: 640px) {
  .grid-2, .grid-3, .grid-4 {
    grid-template-columns: 1fr;
  }
}
```

## 📝 Best Practices

### 1. Доступность (A11y)
- Использовать семантические HTML теги
- Добавлять ARIA атрибуты
- Обеспечивать фокус для клавиатурной навигации
- Проверять контрастность цветов

### 2. Производительность
- Ленивая загрузка изображений
- Отложенная инициализация компонентов
- Debounce для частых событий (input, scroll)
- Memoization для вычислений

### 3. Консистентность
- Единый стиль для всех компонентов
- Единые отступы и размеры
- Единая система иконок

### 4. Поддержка тем
- CSS переменные для цветов
- Data-attribute для переключения тем
- Автоматическое определение системной темы
