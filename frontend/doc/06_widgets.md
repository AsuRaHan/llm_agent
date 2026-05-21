# 🧩 Система виджетов

## 📋 Обзор

Система виджетов отвечает за динамическую генерацию интерактивных компонентов интерфейса для отображения планов действий, прогресса, подтверждений и других элементов.

## 🏗️ Архитектура

### Класс WidgetFactory

```
WidgetFactory
├── Конструктор
│   ├── container: HTMLElement
│   ├── widgets: Map
│   └── registry: Map
│
├── Методы
│   ├── renderPlanWidget()  # Виджет плана
│   ├── renderProgressWidget() # Виджет прогресса
│   ├── renderConfirmationWidget() # Виджет подтверждения
│   ├── renderErrorWidget() # Виджет ошибки
│   ├── createWidget()      # Создание виджета
│   ├── removeWidget(id)    # Удаление виджета
│   ├── updateWidget(id, data) # Обновление виджета
│   └── getWidget(id)       # Получение виджета
│
└── Свойства
    ├── widgets: Map
    └── registry: Map
```

## 📝 Реализация

### Конструктор

```javascript
class WidgetFactory {
    constructor(container) {
        this.container = container;
        this.widgets = new Map();
        this.registry = new Map();
        
        this.init();
    }
    
    init() {
        // Регистрация виджетов
        this.registerWidget('plan', PlanWidget);
        this.registerWidget('progress', ProgressWidget);
        this.registerWidget('confirmation', ConfirmationWidget);
        this.registerWidget('error', ErrorWidget);
    }
    
    registerWidget(type, WidgetClass) {
        this.registry.set(type, WidgetClass);
    }
}
```

### Виджет плана

```javascript
class PlanWidget {
    constructor(container, data) {
        this.container = container;
        this.data = data;
        this.steps = data.steps || [];
        this.id = data.id || `plan_${Date.now()}`;
        
        this.render();
        this.bindEvents();
    }
    
    render() {
        if (this.steps.length === 0) {
            this.container.innerHTML = `
                <div class="widget widget-plan">
                    <div class="widget-header">
                        <h3>План действий</h3>
                        <button class="widget-close" data-widget-id="${this.id}">✕</button>
                    </div>
                    <div class="widget-body">
                        <p class="empty-state">План действий не сформирован</p>
                    </div>
                </div>
            `;
            return;
        }
        
        this.container.innerHTML = `
            <div class="widget widget-plan" data-widget-id="${this.id}">
                <div class="widget-header">
                    <h3>📋 План действий</h3>
                    <button class="widget-close" data-widget-id="${this.id}">✕</button>
                </div>
                <div class="widget-body">
                    <div class="plan-steps">
                        ${this.steps.map((step, index) => 
                            this.createStepHtml(step, index)
                        ).join('')}
                    </div>
                </div>
            </div>
        `;
    }
    
    createStepHtml(step, index) {
        const { title, description, status, estimatedTime } = step;
        
        const statusClass = {
            pending: 'status-pending',
            in_progress: 'status-in-progress',
            completed: 'status-completed',
            failed: 'status-failed'
        }[status] || 'status-pending';
        
        return `
            <div class="plan-step ${statusClass}" data-step-index="${index}">
                <div class="step-number">${index + 1}</div>
                <div class="step-content">
                    <h4 class="step-title">${this.escapeHtml(title)}</h4>
                    ${description ? `<p class="step-description">${this.escapeHtml(description)}</p>` : ''}
                    ${estimatedTime ? `<span class="step-estimate">⏱️ ${estimatedTime}</span>` : ''}
                </div>
                <div class="step-status">
                    ${this.getStatusIcon(status)}
                </div>
            </div>
        `;
    }
    
    getStatusIcon(status) {
        const icons = {
            pending: '⏳',
            in_progress: '🔄',
            completed: '✅',
            failed: '❌'
        };
        return icons[status] || '⏳';
    }
    
    bindEvents() {
        // Закрытие виджета
        this.container.querySelector('.widget-close')?.addEventListener('click', () => {
            this.remove();
        });
        
        // Обновление статуса шага
        this.container.querySelectorAll('.plan-step').forEach(step => {
            step.addEventListener('click', (e) => {
                if (e.target.classList.contains('step-status')) {
                    this.updateStepStatus(step.dataset.stepIndex);
                }
            });
        });
    }
    
    updateStepStatus(index) {
        const step = this.steps[index];
        if (!step) return;
        
        // Цикл статусов
        const statusCycle = ['pending', 'in_progress', 'completed', 'failed'];
        const currentIndex = statusCycle.indexOf(step.status);
        const nextIndex = (currentIndex + 1) % statusCycle.length;
        
        step.status = statusCycle[nextIndex];
        this.render();
        
        // Уведомление
        this.notify(`Шаг ${index + 1} обновлён: ${step.status}`);
    }
    
    notify(message) {
        console.log(`[Widget] ${message}`);
        // Можно добавить уведомление пользователю
    }
    
    remove() {
        this.container.remove();
        this.widgets.delete(this.id);
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
```

### Виджет прогресса

```javascript
class ProgressWidget {
    constructor(container, data) {
        this.container = container;
        this.data = data;
        this.currentStep = data.currentStep || 0;
        this.totalSteps = data.steps?.length || 0;
        this.id = data.id || `progress_${Date.now()}`;
        
        this.render();
        this.bindEvents();
    }
    
    render() {
        const percentage = this.totalSteps > 0 
            ? Math.round((this.currentStep / this.totalSteps) * 100) 
            : 0;
        
        this.container.innerHTML = `
            <div class="widget widget-progress" data-widget-id="${this.id}">
                <div class="widget-header">
                    <h3>📊 Прогресс выполнения</h3>
                    <button class="widget-close" data-widget-id="${this.id}">✕</button>
                </div>
                <div class="widget-body">
                    <div class="progress-info">
                        <span class="progress-text">
                            ${this.currentStep} из ${this.totalSteps} шагов завершено
                        </span>
                        <span class="progress-percentage">${percentage}%</span>
                    </div>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: ${percentage}%"></div>
                    </div>
                    ${this.totalSteps > 0 ? this.createStepsList() : ''}
                </div>
            </div>
        `;
    }
    
    createStepsList() {
        return `
            <div class="progress-steps">
                ${Array.from({ length: this.totalSteps }, (_, i) => 
                    this.createStepItem(i)
                ).join('')}
            </div>
        `;
    }
    
    createStepItem(index) {
        const isCompleted = index < this.currentStep;
        return `
            <div class="progress-step ${isCompleted ? 'completed' : ''}">
                <div class="step-circle">
                    ${isCompleted ? '✓' : ''}
                </div>
                <span class="step-number">${index + 1}</span>
            </div>
        `;
    }
    
    bindEvents() {
        this.container.querySelector('.widget-close')?.addEventListener('click', () => {
            this.remove();
        });
    }
    
    remove() {
        this.container.remove();
        this.widgets.delete(this.id);
    }
}
```

### Виджет подтверждения

```javascript
class ConfirmationWidget {
    constructor(container, data) {
        this.container = container;
        this.data = data;
        this.id = data.id || `confirmation_${Date.now()}`;
        this.confirmed = false;
        
        this.render();
        this.bindEvents();
    }
    
    render() {
        const { title, message, options } = this.data;
        
        this.container.innerHTML = `
            <div class="widget widget-confirmation" data-widget-id="${this.id}">
                <div class="widget-header">
                    <h3>${this.escapeHtml(title || 'Подтверждение')}</h3>
                    <button class="widget-close" data-widget-id="${this.id}">✕</button>
                </div>
                <div class="widget-body">
                    <p class="confirmation-message">${this.escapeHtml(message)}</p>
                    ${options?.description ? `
                        <p class="confirmation-description">${this.escapeHtml(options.description)}</p>
                    ` : ''}
                </div>
                <div class="widget-footer">
                    <button class="btn btn-secondary" data-action="cancel">
                        ${options?.cancelText || 'Отмена'}
                    </button>
                    <button class="btn btn-primary" data-action="confirm">
                        ${options?.confirmText || 'Да'}
                    </button>
                </div>
            </div>
        `;
    }
    
    bindEvents() {
        // Подтверждение
        this.container.querySelector('[data-action="confirm"]')?.addEventListener('click', () => {
            this.confirm();
        });
        
        // Отмена
        this.container.querySelector('[data-action="cancel"]')?.addEventListener('click', () => {
            this.cancel();
        });
        
        // Закрытие
        this.container.querySelector('.widget-close')?.addEventListener('click', () => {
            this.cancel();
        });
    }
    
    confirm() {
        this.confirmed = true;
        this.container.remove();
        this.widgets.delete(this.id);
        
        // Вызов callback
        if (this.data.callback) {
            this.data.callback({
                type: 'confirmation',
                data: { confirmed: true }
            });
        }
    }
    
    cancel() {
        this.container.remove();
        this.widgets.delete(this.id);
        
        // Вызов callback
        if (this.data.callback) {
            this.data.callback({
                type: 'confirmation',
                data: { confirmed: false }
            });
        }
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
```

### Виджет ошибки

```javascript
class ErrorWidget {
    constructor(container, data) {
        this.container = container;
        this.data = data;
        this.id = data.id || `error_${Date.now()}`;
        this.recoveryOptions = data.recoveryOptions || [];
        
        this.render();
        this.bindEvents();
    }
    
    render() {
        const { message, recoveryOptions } = this.data;
        
        this.container.innerHTML = `
            <div class="widget widget-error" data-widget-id="${this.id}">
                <div class="widget-header">
                    <h3>⚠️ Ошибка</h3>
                    <button class="widget-close" data-widget-id="${this.id}">✕</button>
                </div>
                <div class="widget-body">
                    <div class="error-content">
                        <p class="error-message">${this.escapeHtml(message)}</p>
                        ${recoveryOptions.length > 0 ? this.createRecoveryOptions() : ''}
                    </div>
                </div>
            </div>
        `;
    }
    
    createRecoveryOptions() {
        return `
            <div class="recovery-options">
                ${recoveryOptions.map((option, index) => `
                    <button class="btn btn-secondary recovery-btn" data-option="${index}">
                        ${this.escapeHtml(option.label)}
                    </button>
                `).join('')}
            </div>
        `;
    }
    
    bindEvents() {
        this.container.querySelector('.widget-close')?.addEventListener('click', () => {
            this.remove();
        });
        
        this.container.querySelectorAll('.recovery-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const index = e.target.dataset.option;
                this.selectRecoveryOption(index);
            });
        });
    }
    
    selectRecoveryOption(index) {
        const option = this.recoveryOptions[index];
        if (option && option.callback) {
            option.callback();
        }
        
        this.remove();
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
```

### Создание виджета

```javascript
createWidget(type, data) {
    const WidgetClass = this.registry.get(type);
    
    if (!WidgetClass) {
        console.error(`Widget type "${type}" not found`);
        return null;
    }
    
    const widget = new WidgetClass(this.container, data);
    this.widgets.set(widget.id, widget);
    
    return widget;
}
```

### Удаление виджета

```javascript
removeWidget(id) {
    const widget = this.widgets.get(id);
    if (widget) {
        widget.remove();
    }
}
```

### Обновление виджета

```javascript
updateWidget(id, newData) {
    const widget = this.widgets.get(id);
    if (widget) {
        widget.data = { ...widget.data, ...newData };
        widget.render();
    }
}
```

## 📊 Типы виджетов

| Тип | Описание | Примеры данных |
|-----|----------|----------------|
| `plan` | План действий | `{ steps: [], id: string }` |
| `progress` | Прогресс выполнения | `{ currentStep: number, steps: number[] }` |
| `confirmation` | Подтверждение действия | `{ title, message, options, callback }` |
| `error` | Ошибка с recovery | `{ message, recoveryOptions: [] }` |
| `info` | Информационное сообщение | `{ title, message }` |
| `loading` | Индикатор загрузки | `{ message, spinner: boolean }` |

## 🔄 Жизненный цикл виджета

```
1. Создание
   └── createWidget(type, data)
       ├── Найти класс виджета
       ├── Создать экземпляр
       └── Добавить в Map

2. Рендеринг
   └── render()
       ├── Создать HTML
       ├── Добавить в контейнер
       └── Bind events

3. Обновление
   └── updateWidget(id, data)
       ├── Найти виджет
       ├── Обновить данные
       └── Перерендерить

4. Удаление
   └── removeWidget(id)
       ├── Найти виджет
       ├── Удалить из DOM
       └── Удалить из Map
```

## 🧪 Тестирование

### Юнит-тесты

```javascript
describe('WidgetFactory', () => {
    let factory;
    let container;
    
    beforeEach(() => {
        container = document.createElement('div');
        factory = new WidgetFactory(container);
    });
    
    test('should create plan widget', () => {
        const data = {
            steps: [
                { title: 'Шаг 1', status: 'pending' }
            ]
        };
        
        const widget = factory.createWidget('plan', data);
        
        expect(container.innerHTML).toContain('widget-plan');
        expect(container.innerHTML).toContain('План действий');
    });
    
    test('should create progress widget', () => {
        const data = {
            currentStep: 3,
            steps: 5
        };
        
        const widget = factory.createWidget('progress', data);
        
        expect(container.innerHTML).toContain('widget-progress');
        expect(container.innerHTML).toContain('3 из 5');
    });
    
    test('should create confirmation widget', () => {
        const data = {
            title: 'Подтверждение',
            message: 'Вы уверены?',
            callback: jest.fn()
        };
        
        const widget = factory.createWidget('confirmation', data);
        
        expect(container.innerHTML).toContain('widget-confirmation');
    });
});
```

## 📝 Best Practices

### 1. Валидация данных

```javascript
validateWidgetData(type, data) {
    const validators = {
        plan: (data) => {
            if (!Array.isArray(data.steps)) {
                throw new Error('steps must be an array');
            }
        },
        progress: (data) => {
            if (typeof data.currentStep !== 'number') {
                throw new Error('currentStep must be a number');
            }
        },
        confirmation: (data) => {
            if (typeof data.callback !== 'function') {
                throw new Error('callback must be a function');
            }
        }
    };
    
    validators[type](data);
}
```

### 2. Debounce для обновлений

```javascript
debounceUpdate(func, wait) {
    let timeout;
    return (...args) => {
        clearTimeout(timeout);
        timeout = setTimeout(() => func(...args), wait);
    };
}

// Использование
updateWidgetDebounced = debounceUpdate((id, data) => {
    this.updateWidget(id, data);
}, 300);
```

### 3. Анимации

```javascript
animateWidgetAppearance(widget) {
    widget.style.opacity = '0';
    widget.style.transform = 'scale(0.9)';
    
    requestAnimationFrame(() => {
        widget.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
        widget.style.opacity = '1';
        widget.style.transform = 'scale(1)';
    });
}
```

### 4. Доступность

```javascript
// Добавление ARIA атрибутов
addAriaAttributes(widget) {
    widget.setAttribute('role', 'dialog');
    widget.setAttribute('aria-modal', 'true');
    
    const header = widget.querySelector('.widget-header');
    header.setAttribute('role', 'document');
    
    const closeBtn = widget.querySelector('.widget-close');
    closeBtn.setAttribute('aria-label', 'Закрыть');
}
```

## 🚀 Оптимизация

### 1. Ленивая загрузка

```javascript
// Создание виджетов по требованию
lazyLoadWidget(type, data) {
    if (!this.widgets.has('lazy_' + type)) {
        this.createWidget(type, data);
    }
}
```

### 2. Кэширование

```javascript
// Кэширование HTML шаблонов
const widgetTemplates = new Map();

getTemplate(type) {
    if (!widgetTemplates.has(type)) {
        widgetTemplates.set(type, this.getTemplateHtml(type));
    }
    return widgetTemplates.get(type);
}
```

### 3. Web Workers

```javascript
// Рендеринг в worker для больших виджетов
const worker = new Worker('widget-renderer-worker.js');

worker.postMessage({ type: 'render', widgetType, data });
worker.onmessage = (e) => {
    this.container.innerHTML = e.data.html;
};
```

## 🔧 Настройка

### Конфигурация

```javascript
const WIDGET_CONFIG = {
    maxWidgets: 10,
    renderDebounce: 100,
    animationDuration: 300,
    closeOnOverlayClick: true,
};
```

### Расширенная конфигурация

```javascript
class WidgetFactory {
    constructor(container, options = {}) {
        this.container = container;
        this.maxWidgets = options.maxWidgets ?? 10;
        this.renderDebounce = options.renderDebounce ?? 100;
        
        this.init();
    }
}
```
