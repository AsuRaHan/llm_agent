/**
 * Widget Renderer
 * Рендеринг интерактивных виджетов
 * Vanilla JS + CommonJS
 */

class WidgetRenderer {
    constructor(container) {
        this.container = container;
    }

    /**
     * Показать виджет подтверждения действия
     */
    showConfirmation(data) {
        var widget = document.createElement('div');
        widget.className = 'message agent';
        widget.innerHTML = this.getConfirmationTemplate(data);
        this.container.appendChild(widget);

        // Привязка обработчиков к кнопкам
        var yesBtn = widget.querySelector('[data-role="yes-btn"]');
        var noBtn = widget.querySelector('[data-role="no-btn"]');

        if (yesBtn) {
            yesBtn.addEventListener('click', () => {
                this.hideWidget(widget);
                this.emit('confirmation', { confirmed: true, data: data });
            });
        }

        if (noBtn) {
            noBtn.addEventListener('click', () => {
                this.hideWidget(widget);
                this.emit('confirmation', { confirmed: false, data: data });
            });
        }

        return widget;
    }

    /**
     * Показать виджет ошибки
     */
    showError(data) {
        var widget = document.createElement('div');
        widget.className = 'message agent';
        widget.innerHTML = this.getErrorTemplate(data);
        this.container.appendChild(widget);

        // Привязка обработчиков к кнопкам восстановления
        var buttonContainer = widget.querySelector('[data-role="button-container"]');
        if (buttonContainer && data.recovery_options) {
            for (var i = 0; i < data.recovery_options.length; i++) {
                var option = data.recovery_options[i];
                var btn = document.createElement('button');
                btn.className = 'btn-secondary px-3 py-1.5 rounded text-xs text-white hover:bg-red-600/20 transition-all';
                btn.textContent = option;
                btn.addEventListener('click', () => {
                    this.hideWidget(widget);
                    this.emit('error_recovery', { option: option, data: data });
                });
                buttonContainer.appendChild(btn);
            }
        }

        return widget;
    }

    /**
     * Показать виджет плана
     */
    showPlan(plan) {
        var widget = document.createElement('div');
        widget.className = 'message agent';
        widget.innerHTML = this.getPlanTemplate(plan);
        this.container.appendChild(widget);

        // Привязка обработчиков
        var approveBtn = widget.querySelector('[data-role="approve-btn"]');
        var editBtn = widget.querySelector('[data-role="edit-btn"]');

        if (approveBtn) {
            approveBtn.addEventListener('click', () => {
                this.hideWidget(widget);
                this.emit('plan_approved', { plan: plan });
            });
        }

        if (editBtn) {
            editBtn.addEventListener('click', () => {
                this.hideWidget(widget);
                this.emit('plan_edit', { plan: plan });
            });
        }

        return widget;
    }

    /**
     * Показать виджет прогресса
     */
    showProgress(steps) {
        var widget = document.createElement('div');
        widget.className = 'message agent';
        widget.innerHTML = this.getProgressTemplate(steps);
        this.container.appendChild(widget);

        return widget;
    }

    /**
     * Скрыть виджет
     */
    hideWidget(widget) {
        widget.style.display = 'none';
    }

    /**
     * Удалить виджет
     */
    removeWidget(widget) {
        widget.remove();
    }

    /**
     * Получить шаблон подтверждения
     */
    getConfirmationTemplate(data) {
        var message = data.message || '';
        var toolCall = data.tool_call || {};
        var function_name = toolCall.function_name || '';
        var arguments_str = JSON.stringify(toolCall.arguments || {}, null, 2);

        return '    <div class="confirmation-widget">' +
            '    <p class="font-semibold text-amber-400 mb-2 flex items-center gap-2">' +
            '        <i class="fas fa-exclamation-triangle"></i>' +
            '        Запрос на подтверждение действия' +
            '    </p>' +
            '    <p class="text-sm text-secondary mb-3">' + message + '</p>' +
            '    <pre class="code-block text-xs text-gray-300 overflow-x-auto mb-4 p-3 rounded bg-gray-900 border border-gray-700">' +
            '        ' + arguments_str +
            '    </pre>' +
            '    <div class="flex flex-wrap gap-3">' +
            '        <button data-role="yes-btn" class="btn-primary px-4 py-2 rounded-lg text-sm font-semibold text-white flex items-center gap-2">' +
            '            <i class="fas fa-check"></i> Разрешить' +
            '        </button>' +
            '        <button data-role="no-btn" class="btn-secondary px-4 py-2 rounded-lg text-sm font-semibold text-white hover:bg-red-600/20 transition-all">' +
            '            <i class="fas fa-times"></i> Отклонить' +
            '        </button>' +
            '    </div>' +
            '    </div>';
    }

    /**
     * Получить шаблон ошибки
     */
    getErrorTemplate(data) {
        var title = data.title || 'Ошибка';
        var message = data.error_message || '';
        var recovery_options = data.recovery_options || [];

        var html = '    <div class="error-widget">' +
            '    <p class="font-semibold text-red-400 mb-2 flex items-center gap-2">' +
            '        <i class="fas fa-exclamation-circle"></i>' +
            '        ' + title +
            '    </p>' +
            '    <pre class="code-block text-xs text-red-300 overflow-x-auto mb-3 p-3 rounded bg-gray-900 border border-gray-700">' +
            '        ' + message +
            '    </pre>';

        if (recovery_options.length > 0) {
            html += '    <p class="text-sm text-secondary mb-3">Выберите стратегию восстановления:</p>' +
                '    <div data-role="button-container" class="flex flex-wrap gap-2"></div>';
        }

        html += '    </div>';
        return html;
    }

    /**
     * Получить шаблон плана
     */
    getPlanTemplate(plan) {
        var steps = plan.steps || [];
        var stepsHtml = '';

        for (var i = 0; i < steps.length; i++) {
            var step = steps[i];
            stepsHtml += '        <li class="flex items-start gap-2 p-2 rounded bg-white/5 border border-white/10">' +
                '            <span class="flex-shrink-0 w-6 h-6 rounded-full bg-blue-500/20 text-blue-400 flex items-center justify-center text-xs font-bold">' + (i + 1) + '</span>' +
                '            <span class="text-sm text-gray-200 flex-grow">' + step.description + '</span>' +
                '        </li>';
        }

        return '    <div class="plan-widget">' +
            '    <p class="font-semibold text-cyan-400 mb-3 flex items-center gap-2">' +
            '        <i class="fas fa-list-check"></i>' +
            '        Составлен пошаговый план' +
            '    </p>' +
            '    <ol data-role="plan-list" class="list-none space-y-2 text-sm text-secondary">' +
            stepsHtml +
            '    </ol>' +
            '    <div class="flex flex-wrap gap-3 mt-4">' +
            '        <button data-role="approve-btn" class="btn-primary px-4 py-2 rounded-lg text-sm font-semibold text-white flex items-center gap-2">' +
            '            <i class="fas fa-rocket"></i> Утвердить и запустить' +
            '        </button>' +
            '        <button data-role="edit-btn" class="btn-secondary px-4 py-2 rounded-lg text-sm font-semibold text-white hover:bg-cyan-600/20 transition-all">' +
            '            <i class="fas fa-edit"></i> Редактировать' +
            '        </button>' +
            '    </div>' +
            '    </div>';
    }

    /**
     * Получить шаблон прогресса
     */
    getProgressTemplate(steps) {
        var stepsHtml = '';

        for (var i = 0; i < steps.length; i++) {
            var step = steps[i];
            var statusClass = step.completed ? 'text-green-400' : 'text-secondary';
            var statusIcon = step.completed ? 'fa-check-circle' : 'fa-circle';

            stepsHtml += '        <li class="flex items-start gap-2 p-2 rounded bg-white/5 border border-white/10">' +
                '            <i class="fas ' + statusIcon + ' ' + statusClass + ' flex-shrink-0 mt-0.5"></i>' +
                '            <span class="text-sm ' + (step.completed ? 'text-gray-200' : 'text-secondary') + '">' + step.description + '</span>' +
                '        </li>';
        }

        return '    <div class="message-content border-l-4 border-green-500 bg-white/5 p-4 rounded-r-lg">' +
            '    <p class="font-semibold text-green-400 mb-2 flex items-center gap-2">' +
            '        <i class="fas fa-bolt"></i>' +
            '        Выполнение задачи' +
            '    </p>' +
            '    <ul data-role="progress-list" class="space-y-2 text-sm text-secondary">' +
            stepsHtml +
            '    </ul>' +
            '    </div>';
    }

    /**
     * Эмитить событие
     */
    emit(event, data) {
        if (typeof window !== 'undefined' && window.smartHammer) {
            window.smartHammer.emit(event, data);
        }
    }
}

// Экспорт через CommonJS
module.exports = WidgetRenderer;
