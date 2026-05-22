/**
 * @typedef {object} StreamingIndicatorOptions
 * @property {number} [dotsCount=3] Количество точек в индикаторе.
 * @property {number} [animationDuration=1400] Длительность анимации в мс.
 * @property {string} [defaultText='Генерация ответа...'] Текст, отображаемый рядом с индикатором.
 */

/**
 * Отображает анимированный индикатор загрузки для потоковой передачи.
 */
class StreamingIndicator {
    /**
     * @param {HTMLElement} container Элемент DOM, в который будет добавлен индикатор.
     * @param {StreamingIndicatorOptions} [options] Опции для StreamingIndicator.
     */
    constructor(container, options = {}) {
        this.container = container;
        this.options = {
            dotsCount: 3,
            animationDuration: 1400,
            defaultText: 'Генерация ответа...',
            ...options
        };

        this.isShowing = false;
        this.currentText = this.options.defaultText;
        this.indicatorElement = null;

        this._createIndicatorElement();
    }

    /**
     * Создает DOM-элемент индикатора.
     * @private
     */
    _createIndicatorElement() {
        this.indicatorElement = document.createElement('div');
        this.indicatorElement.className = 'streaming-indicator';
        this.indicatorElement.style.display = 'none'; // Скрыт по умолчанию

        const textSpan = document.createElement('span');
        textSpan.className = 'streaming-indicator-text';
        textSpan.textContent = this.currentText;
        this.textSpan = textSpan;

        const dotsContainer = document.createElement('span');
        dotsContainer.className = 'streaming-indicator-dots';

        for (let i = 0; i < this.options.dotsCount; i++) {
            const dot = document.createElement('span');
            dot.className = 'dot';
            dot.style.animationDelay = `${-this.options.animationDuration / this.options.dotsCount * (this.options.dotsCount - 1 - i) / 1000}s`; // Анимация с задержкой
            dotsContainer.appendChild(dot);
        }

        this.indicatorElement.appendChild(textSpan);
        this.indicatorElement.appendChild(dotsContainer);
        this.container.appendChild(this.indicatorElement);
    }

    /**
     * Показывает индикатор.
     */
    show() {
        this.isShowing = true;
        this.indicatorElement.style.display = 'flex';
    }

    /**
     * Скрывает индикатор.
     */
    hide() {
        this.isShowing = false;
        this.indicatorElement.style.display = 'none';
    }

    setText(text) {
        this.currentText = text;
        if (this.textSpan) {
            this.textSpan.textContent = text;
        }
    }

    /**
     * Переключает видимость индикатора.
     */
    toggle() {
        if (this.isShowing) {
            this.hide();
        } else {
            this.show();
        }
    }
}

export { StreamingIndicator };