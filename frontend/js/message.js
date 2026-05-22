/**
 * @typedef {object} MessageRendererOptions
 * @property {boolean} [useMarkdown=true] Использовать ли Marked.js для рендеринга Markdown.
 * @property {Function} [markdownParser=null] Кастомная функция для парсинга Markdown (если useMarkdown=true).
 */

/**
 * Отвечает за рендеринг сообщений в DOM, включая форматирование Markdown и безопасное отображение HTML.
 */
class MessageRenderer {
    /**
     * @param {HTMLElement} messagesContainer Контейнер, куда будут добавляться сообщения.
     * @param {MessageRendererOptions} [options] Опции для MessageRenderer.
     */
    constructor(messagesContainer, options = {}) {
        this.messagesContainer = messagesContainer;
        this.options = {
            useMarkdown: true,
            markdownParser: null, // Ожидается функция, например, marked.parse
            ...options
        };

        // Проверяем наличие Marked.js, если включен Markdown
        if (this.options.useMarkdown && !this.options.markdownParser && typeof marked === 'undefined') {
            console.warn('Marked.js не найден и кастомный парсер не предоставлен. Markdown не будет использоваться.');
            this.options.useMarkdown = false;
        }
    }

    /**
     * Рендерит новое сообщение в контейнере.
     * @param {string} text Текст сообщения.
     * @param {'user'|'agent'|'system'|'error'} sender Отправитель сообщения.
     * @returns {HTMLElement} Созданный элемент сообщения.
     */
    render(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.className = `message message-${sender}`;

        let contentHtml = this._processText(text);

        messageElement.innerHTML = `
            <div class="message-content">
                <div class="message-header">
                    <span class="message-author">${sender === 'user' ? 'Вы' : 'LLM Agent'}</span>
                    <span class="message-time">${this._formatTime(Date.now())}</span>
                </div>
                <div class="message-body">${contentHtml}</div>
            </div>
            ${sender === 'user' ? '<div class="message-avatar">👤</div>' : '<div class="message-avatar">🤖</div>'}
        `;

        this.messagesContainer.appendChild(messageElement);
        this._scrollToBottom();
        return messageElement;
    }

    /**
     * Добавляет текст к существующему элементу сообщения.
     * @param {HTMLElement} messageElement Элемент DOM сообщения, к которому нужно добавить текст.
     * @param {string} text Добавляемый текст.
     */
    appendToExisting(messageElement, text) {
        const messageBody = messageElement.querySelector('.message-body');
        if (messageBody) {
            // Для потоковой передачи лучше добавлять текст напрямую, а не перерендеривать весь Markdown каждый раз.
            // Если используется Markdown, это может быть сложнее, так как нужно корректно обновлять HTML.
            // Для простоты, здесь мы просто добавляем текст. Если нужен Markdown, TokenAccumulator лучше подходит.
            messageBody.innerHTML += this._processText(text);
            this._scrollToBottom();
        }
    }

    /**
     * Обрабатывает текст, применяя Markdown или экранирование HTML.
     * @param {string} text Входной текст.
     * @returns {string} Обработанный HTML-текст.
     * @private
     */
    _processText(text) {
        if (this.options.useMarkdown) {
            if (this.options.markdownParser) {
                return this.options.markdownParser(text);
            } else if (typeof marked !== 'undefined') {
                return marked.parse(text);
            }
        }
        return this._escapeHTML(text).replace(/\n/g, '<br>'); // Базовое форматирование переносов строк
    }

    /**
     * Рендерит Markdown-текст в HTML.
     * @param {string} text Markdown-текст.
     * @returns {string} HTML-текст.
     */
    renderMarkdown(text) {
        return this._processText(text);
    }

    /**
     * Экранирует HTML-сущности в строке для предотвращения XSS.
     * @param {string} text Входной текст.
     * @returns {string} Экранированный текст.
     * @private
     */
    _escapeHTML(text) {
        const div = document.createElement('div');
        div.appendChild(document.createTextNode(text));
        return div.innerHTML;
    }

    /**
     * Форматирует временную метку в читаемый вид.
     * @param {number} timestamp Временная метка в мс.
     * @returns {string} Отформатированное время.
     * @private
     */
    _formatTime(timestamp) {
        const date = new Date(timestamp);
        const now = new Date();
        const diff = now - date;

        if (diff < 60000) return 'Только что'; // Меньше минуты
        if (diff < 3600000) return `${Math.floor(diff / 60000)} мин. назад`; // Меньше часа

        return date.toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
    }

    /**
     * Прокручивает контейнер сообщений до конца.
     * @private
     */
    _scrollToBottom() {
        this.messagesContainer.scrollTop = this.messagesContainer.scrollHeight;
    }
}

export { MessageRenderer };