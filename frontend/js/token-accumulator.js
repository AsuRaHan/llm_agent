/**
 * @typedef {object} TokenAccumulatorOptions
 * @property {boolean} [animate=true] Включить анимацию появления токенов.
 * @property {boolean} [highlightCurrent=false] Выделять текущий токен.
 * @property {boolean} [useMarkdown=false] Обрабатывать текст как Markdown.
 */

/**
 * Накапливает токены и отображает их в указанном контейнере, опционально с анимацией и Markdown.
 */
class TokenAccumulator {
    /**
     * @param {HTMLElement} container Элемент DOM, в который будут добавляться токены.
     * @param {TokenAccumulatorOptions} [options] Опции для TokenAccumulator.
     */
    constructor(container, options = {}) {
        this.container = container;
        this.options = {
            animate: true,
            highlightCurrent: false,
            useMarkdown: false,
            ...options
        };

        this.text = '';
        this.isComplete = false;
        this.currentTimeout = null;
        this.animationDelay = 50; // Задержка между токенами для анимации

        // Если используем Markdown, нам понадобится библиотека Marked.js или аналогичная.
        // Для простоты, здесь будет базовая обработка или заглушка.
        if (this.options.useMarkdown && typeof marked === 'undefined') {
            console.warn('Marked.js не найден. Markdown не будет использоваться.');
            this.options.useMarkdown = false;
        }
    }

    /**
     * Добавляет один токен к накопленному тексту и обновляет отображение.
     * @param {string} token Токен для добавления.
     */
    appendToken(token) {
        this.text += token;
        this.render();
    }

    /**
     * Добавляет весь текст сразу и обновляет отображение.
     * @param {string} text Текст для добавления.
     */
    appendText(text) {
        this.text = text;
        this.render();
    }

    /**
     * Очищает накопленный текст и контейнер.
     */
    clear() {
        this.text = '';
        this.isComplete = false;
        this.container.innerHTML = '';
    }

    /**
     * Возвращает весь накопленный текст.
     * @returns {string}
     */
    getText() {
        return this.text;
    }

    /**
     * Отмечает, что поток токенов завершен.
     */
    markComplete() {
        this.isComplete = true;
    }

    /**
     * Обновляет содержимое контейнера.
     */
    render() {
        let content = this.text;
        if (this.options.useMarkdown && typeof marked !== 'undefined') {
            content = marked.parse(content);
        } else {
            // Простая замена переносов строк на <br> для базового форматирования
            content = content.replace(/\n/g, '<br>');
        }
        this.container.innerHTML = content;
        // Можно добавить логику для прокрутки, если контейнер переполняется
        this.container.scrollTop = this.container.scrollHeight;
    }
}

export { TokenAccumulator };