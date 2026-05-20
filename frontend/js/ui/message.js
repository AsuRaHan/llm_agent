/**
 * MessageRenderer - Рендеринг сообщений
 * 
 * @module ui/message
 */

class MessageRenderer {
    /**
     * Конструктор MessageRenderer
     * @param {Object} options - Опции рендерера
     * @param {boolean} options.useMarkdown - Использовать markdown парсинг
     */
    constructor(options = {}) {
        this.useMarkdown = options.useMarkdown || false;
        this.marked = options.marked || null;
    }

    /**
     * Рендеринг сообщения пользователя
     * @param {string} text - Текст сообщения
     * @returns {string} HTML элемент
     */
    renderUserMessage(text) {
        return this._renderMessage(text, 'user');
    }

    /**
     * Рендеринг сообщения агента
     * @param {string} text - Текст сообщения
     * @returns {string} HTML элемент
     */
    renderAgentMessage(text) {
        return this._renderMessage(text, 'agent');
    }

    /**
     * Рендеринг сообщения ошибки
     * @param {string} text - Текст ошибки
     * @returns {string} HTML элемент
     */
    renderErrorMessage(text) {
        return this._renderMessage(text, 'error');
    }

    /**
     * Рендеринг сообщения мысли
     * @param {string} text - Текст мысли
     * @returns {string} HTML элемент
     */
    renderThoughtMessage(text) {
        return this._renderMessage(text, 'thought');
    }

    /**
     * Рендеринг блока кода
     * @param {string} text - Текст кода
     * @param {string} language - Язык кода
     * @returns {string} HTML элемент
     */
    renderCodeBlock(text, language = '') {
        const wrapper = document.createElement('div');
        wrapper.className = 'relative my-3 rounded-lg overflow-hidden border border-gray-700 bg-gray-950 font-mono text-sm';
        
        wrapper.innerHTML = `
            <div class="flex items-center justify-between px-4 py-1.5 bg-gray-900 text-xs text-gray-400 border-b border-gray-800">
                <span>${language || 'CODE'}</span>
                <button onclick="navigator.clipboard.writeText(this.parentElement.nextElementSibling.innerText); this.textContent='Скопировано!'; setTimeout(()=>this.textContent='Копировать', 2000)" class="hover:text-white transition-colors cursor-pointer">Копировать</button>
            </div>
            <pre class="p-4 overflow-x-auto text-gray-200"><code class="whitespace-pre">${this._escapeHtml(text)}</code></pre>
        `;
        
        return wrapper.outerHTML;
    }

    /**
     * Внутренний метод рендеринга сообщения
     * @private
     * @param {string} text - Текст сообщения
     * @param {string} sender - Отправитель
     * @returns {string} HTML элемент
     */
    _renderMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        
        const contentElement = document.createElement('div');
        contentElement.classList.add('message-content');
        
        if (this.useMarkdown && this.marked) {
            const rawHtml = this.marked.parse(text);
            contentElement.innerHTML = this._sanitizeHtml(rawHtml);
        } else {
            // Простой парсинг блоков кода
            const formattedText = text.replace(/```([\s\S]*?)```/g, (match, code) => {
                return `<pre><code>${this._escapeHtml(code)}</code></pre>`;
            });
            contentElement.innerHTML = this._sanitizeHtml(formattedText);
        }
        
        messageElement.appendChild(contentElement);
        return messageElement.outerHTML;
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

    /**
     * Очистка HTML от вредоносного кода
     * @private
     * @param {string} html - HTML для очистки
     * @returns {string} Очистленный HTML
     */
    _sanitizeHtml(html) {
        const tempDiv = document.createElement('div');
        tempDiv.innerHTML = html;
        
        // Удаляем опасные теги
        const dangerousTags = ['script', 'iframe', 'object', 'embed', 'form', 'input', 'button'];
        dangerousTags.forEach(tag => {
            const elements = tempDiv.querySelectorAll(tag);
            elements.forEach(el => el.remove());
        });
        
        return tempDiv.innerHTML;
    }
}

export { MessageRenderer };
