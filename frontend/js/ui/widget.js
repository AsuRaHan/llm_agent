/**
 * WidgetFactory - Фабрика виджетов
 * 
 * @module ui/widget
 */

class WidgetFactory {
    /**
     * Создание виджета подтверждения действия
     * @param {string} message - Сообщение
     * @param {Object} toolCall - Вызов инструмента
     * @returns {HTMLElement} Виджет
     */
    createConfirmationWidget(message, toolCall) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content', 'confirmation');
        
        contentElement.innerHTML = `
            <p>${this._escapeHtml(message)}</p>
            <pre>${JSON.stringify(toolCall.function, null, 2)}</pre>
        `;
        
        const buttonContainer = document.createElement('div');
        buttonContainer.classList.add('confirm-buttons');

        const yesButton = document.createElement('button');
        yesButton.textContent = 'Да';
        yesButton.onclick = () => this._handleConfirmationYes(buttonContainer);

        const noButton = document.createElement('button');
        noButton.textContent = 'Нет';
        noButton.onclick = () => this._handleConfirmationNo(buttonContainer);

        buttonContainer.appendChild(yesButton);
        buttonContainer.appendChild(noButton);
        contentElement.appendChild(buttonContainer);
        widgetElement.appendChild(contentElement);
        
        return widgetElement;
    }

    /**
     * Создание виджета плана
     * @param {Array} steps - Шаги плана
     * @returns {HTMLElement} Виджет
     */
    createPlanWidget(steps) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.style.borderLeft = '4px solid #3498db';
        
        let htmlContent = `
            <p><strong>📋 Я составил пошаговый план для выполнения вашей задачи:</strong></p>
            <ol style="margin-left: 20px; padding: 0 0 0 10px;">
        `;
        
        steps.forEach((step) => {
            htmlContent += `<li style="margin-bottom: 6px; color: #2c3e50;">${this._escapeHtml(step)}</li>`;
        });
        
        htmlContent += `
            </ol>
            <div class="confirm-buttons" style="display: flex; gap: 10px; margin-top: 12px;">
                <button class="btn-approve-plan" style="background-color: #2ecc71; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; font-weight: bold;">🚀 Утвердить и запустить</button>
                <button class="btn-reject-plan" style="background-color: #e74c3c; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer;">Отмена</button>
            </div>
        `;
        
        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        
        // Добавляем обработчики событий
        widgetElement.querySelector('.btn-approve-plan').addEventListener('click', () => {
            widgetElement.querySelector('.confirm-buttons')?.remove();
            this._handlePlanApproved(widgetElement);
        });

        widgetElement.querySelector('.btn-reject-plan').addEventListener('click', () => {
            widgetElement.querySelector('.confirm-buttons')?.remove();
            this._handlePlanRejected(widgetElement);
        });
        
        return widgetElement;
    }

    /**
     * Создание виджета восстановления от ошибки
     * @param {string} errorMessage - Сообщение об ошибке
     * @param {Array} recoveryOptions - Опции восстановления
     * @returns {HTMLElement} Виджет
     */
    createErrorWidget(errorMessage, recoveryOptions) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.style.borderLeft = '4px solid #e74c3c';
        
        let htmlContent = `
            <p><strong><span style="color: #e74c3c;">❗️</span> Ошибка выполнения плана:</strong></p>
            <p style="background: #2d2d2d; padding: 8px; border-radius: 4px; font-family: monospace; font-size: 0.9em;">${this._escapeHtml(errorMessage)}</p>
            <p>Что мне делать дальше?</p>
        `;
        
        const buttonContainer = document.createElement('div');
        buttonContainer.classList.add('confirm-buttons');
        buttonContainer.style.display = 'flex';
        buttonContainer.style.gap = '10px';
        buttonContainer.style.marginTop = '12px';

        const optionMap = {
            'retry': 'Повторить',
            'skip': 'Пропустить шаг',
            're-plan': 'Перепланировать',
            'abort': 'Отменить план'
        };

        // Ensure 'abort' is always an option
        if (!recoveryOptions.includes('abort')) {
            recoveryOptions.push('abort');
        }

        recoveryOptions.forEach(option => {
            const button = document.createElement('button');
            button.textContent = optionMap[option] || option;
            button.dataset.option = option;
            button.style.padding = '8px 16px';
            button.style.borderRadius = '4px';
            button.style.border = 'none';
            button.style.cursor = 'pointer';
            button.style.backgroundColor = (option === 'abort' || option === 'skip') ? '#c0392b' : '#2980b9';
            if (option === 'retry') button.style.backgroundColor = '#27ae60';

            button.addEventListener('click', () => {
                this._handleErrorRecovery(button.dataset.option, buttonContainer, widgetElement);
            });
            buttonContainer.appendChild(button);
        });

        htmlContent += buttonContainer.outerHTML;
        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        
        return widgetElement;
    }

    /**
     * Создание виджета прогресса плана
     * @param {number} currentStepIndex - Текущий индекс шага
     * @param {Array} steps - Шаги плана
     * @returns {HTMLElement} Виджет
     */
    createProgressWidget(currentStepIndex, steps) {
        const widgetElement = document.createElement('div');
        widgetElement.classList.add('message', 'agent');
        
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        contentElement.style.borderLeft = '4px solid #2ecc71';
        contentElement.style.background = '#f4fbf7';
        contentElement.style.padding = '12px';
        contentElement.style.borderRadius = '4px';
        contentElement.style.boxShadow = '0 1px 3px rgba(0,0,0,0.05)';
        
        let htmlContent = `
            <p style="margin-top: 0; color: #27ae60;"><strong>⚡ Выполнение автономной кампании:</strong></p>
            <ul style="list-style: none; padding-left: 0; margin-bottom: 0;">
        `;
        
        steps.forEach((step, index) => {
            let icon = '⏳'; 
            let style = 'color: #7f8c8d;';
            
            if (index < currentStepIndex) {
                icon = '✅'; 
                style = 'color: #27ae60; text-decoration: line-through; opacity: 0.7;';
            } else if (index === currentStepIndex) {
                icon = '⚙️'; 
                style = 'color: #2980b9; font-weight: bold; animation: pulse 2s infinite;';
            }
            
            htmlContent += `<li style="margin-bottom: 8px; ${style}">${icon} ${this._escapeHtml(step)}</li>`;
        });
        
        htmlContent += `</ul>`;
        contentElement.innerHTML = htmlContent;
        widgetElement.appendChild(contentElement);
        
        return widgetElement;
    }

    /**
     * Обработчик подтверждения "Да"
     * @private
     * @param {HTMLElement} buttonContainer - Контейнер кнопок
     */
    _handleConfirmationYes(buttonContainer) {
        buttonContainer.remove();
        console.log('Action confirmed');
    }

    /**
     * Обработчик подтверждения "Нет"
     * @private
     * @param {HTMLElement} buttonContainer - Контейнер кнопок
     */
    _handleConfirmationNo(buttonContainer) {
        buttonContainer.remove();
        console.log('Action rejected');
    }

    /**
     * Обработчик утверждения плана
     * @private
     * @param {HTMLElement} widgetElement - Виджет
     */
    _handlePlanApproved(widgetElement) {
        console.log('Plan approved');
    }

    /**
     * Обработчик отклонения плана
     * @private
     * @param {HTMLElement} widgetElement - Виджет
     */
    _handlePlanRejected(widgetElement) {
        console.log('Plan rejected');
    }

    /**
     * Обработчик восстановления от ошибки
     * @private
     * @param {string} option - Выбранная опция
     * @param {HTMLElement} buttonContainer - Контейнер кнопок
     * @param {HTMLElement} widgetElement - Виджет
     */
    _handleErrorRecovery(option, buttonContainer, widgetElement) {
        console.log('Error recovery option:', option);
        buttonContainer.remove();
        widgetElement.remove();
    }

    /**
     * Экранирование HTML
     * @private
     * @param {string} str - Строка для экранирования
     * @returns {string} Экранированная строка
     */
    _escapeHtml(str) {
        if (!str) return '';
        return str
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }
}

export { WidgetFactory };
