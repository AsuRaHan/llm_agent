/**
 * Message Renderer
 * Рендеринг сообщений в чате
 * Vanilla JS + CommonJS
 */

class MessageRenderer {
    constructor(container) {
        this.container = container;
        this.messageIdCounter = 0;
    }

    /**
     * Генерация уникального ID для сообщения
     */
    generateId() {
        return 'msg-' + Date.now() + '-' + (this.messageIdCounter++);
    }

    /**
     * Рендеринг сообщения пользователя
     */
    renderUserMessage(text, images) {
        images = images || [];
        var message = {
            id: this.generateId(),
            role: 'user',
            content: text,
            images: images,
            timestamp: new Date().toISOString()
        };

        var messageElement = document.createElement('div');
        messageElement.className = 'message user animate-fade-in';
        messageElement.dataset.messageId = message.id;

        var contentDiv = document.createElement('div');
        contentDiv.className = 'message-content border-l-4 border-blue-500 bg-blue-500/10 p-4 rounded-r-lg';

        // Текст
        var textElement = document.createElement('p');
        textElement.className = 'text-sm text-gray-200 leading-relaxed whitespace-pre-wrap';
        textElement.textContent = text;
        contentDiv.appendChild(textElement);

        // Изображения
        if (images.length > 0) {
            var imagesContainer = document.createElement('div');
            imagesContainer.className = 'flex gap-2 mt-3 overflow-x-auto pb-2';

            for (var i = 0; i < images.length; i++) {
                var img = images[i];
                var imgElement = document.createElement('img');
                imgElement.src = img.url;
                imgElement.alt = img.alt || 'Uploaded image';
                imgElement.className = 'max-w-xs max-h-48 rounded-lg border border-white/10';
                imagesContainer.appendChild(imgElement);
            }

            contentDiv.appendChild(imagesContainer);
        }

        messageElement.appendChild(contentDiv);
        this.container.appendChild(messageElement);

        return message;
    }

    /**
     * Рендеринг сообщения агента
     */
    renderAgentMessage(content, type) {
        type = type || 'text';
        var message = {
            id: this.generateId(),
            role: 'agent',
            content: content,
            type: type,
            timestamp: new Date().toISOString()
        };

        var messageElement = document.createElement('div');
        messageElement.className = 'message agent animate-fade-in';
        messageElement.dataset.messageId = message.id;

        var contentDiv = document.createElement('div');
        contentDiv.className = 'message-content border-l-4 border-blue-500 bg-white/5 p-4 rounded-r-lg';

        // Обработка типа контента
        if (type === 'text') {
            var textElement = document.createElement('p');
            textElement.className = 'text-sm text-gray-200 leading-relaxed';
            textElement.textContent = content;
            contentDiv.appendChild(textElement);
        } else if (type === 'markdown') {
            contentDiv.innerHTML = this.renderMarkdown(content);
        } else if (type === 'code') {
            contentDiv.innerHTML = this.renderCodeBlock(content);
        } else if (type === 'tool') {
            contentDiv.innerHTML = this.renderToolCall(content);
        } else if (type === 'error') {
            contentDiv.innerHTML = this.renderError(content);
        }

        messageElement.appendChild(contentDiv);
        this.container.appendChild(messageElement);

        return message;
    }

    /**
     * Рендеринг Markdown
     */
    renderMarkdown(markdown) {
        var html = markdown
            // Заголовки
            .replace(/^### (.*$)/gim, '<h3 class="text-lg font-semibold mb-2">$1</h3>')
            .replace(/^## (.*$)/gim, '<h2 class="text-xl font-semibold mb-2">$1</h2>')
            .replace(/^# (.*$)/gim, '<h1 class="text-2xl font-bold mb-3">$1</h1>')
            // Код
            .replace(/`([^`]+)`/gim, '<code class="bg-gray-800 px-1.5 py-0.5 rounded text-sm font-mono text-blue-300">$1</code>')
            // Код блоки
            .replace(/```(\w+)\n([\s\S]*?)```/gim, '<pre class="code-block"><code class="text-sm font-mono text-gray-300">$2</code></pre>')
            // Списки
            .replace(/^\- (.*$)/gim, '<li class="ml-4 list-disc">$1</li>')
            .replace(/^\* (.*$)/gim, '<li class="ml-4 list-disc">$1</li>')
            // Цитаты
            .replace(/^> (.*$)/gim, '<blockquote class="border-l-4 border-blue-500 pl-4 italic text-gray-400">$1</blockquote>')
            // Переносы строк
            .replace(/\n/gim, '<br>');

        return html;
    }

    /**
     * Рендеринг кода
     */
    renderCodeBlock(code) {
        var lines = code.split('\n');
        var html = '<pre class="code-block"><code class="text-sm font-mono text-gray-300">';
        
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];
            // Простая подсветка синтаксиса
            var formattedLine = line
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;')
                .replace(/\b(const|let|var|function|return|if|else|for|while|class|import|export|from|default)\b/g, '<span class="text-purple-400">$1</span>')
                .replace(/\b(true|false|null|undefined)\b/g, '<span class="text-orange-400">$1</span>')
                .replace(/('.*?')/g, '<span class="text-green-400">$1</span>')
                .replace(/(#.*)/g, '<span class="text-gray-500">$1</span>');
            
            html += '<div>' + formattedLine + '</div>';
        }
        
        html += '</code></pre>';
        return html;
    }

    /**
     * Рендеринг tool call
     */
    renderToolCall(toolCall) {
        var function_name = toolCall.function_name;
        var args = toolCall.arguments;
        
        return '    <div class="space-y-2">' +
            '    <p class="text-sm font-semibold text-amber-400 mb-2">🛠️ Инструмент: <code class="text-sm">' + function_name + '</code></p>' +
            '    <pre class="code-block text-xs text-gray-300 overflow-x-auto p-3 rounded bg-gray-900 border border-gray-700">' +
            '        ' + JSON.stringify(args, null, 2) +
            '    </pre>' +
            '    </div>';
    }

    /**
     * Рендеринг ошибки
     */
    renderError(error) {
        var title = error.title;
        var message = error.message;
        var recovery_options = error.recovery_options;
        
        var html = '    <div class="error-widget">' +
            '    <p class="font-semibold text-red-400 mb-2 flex items-center gap-2">' +
            '        <i class="fas fa-exclamation-circle"></i>' +
            '        ' + (title || 'Ошибка') +
            '    </p>' +
            '    <pre class="code-block text-xs text-red-300 overflow-x-auto mb-3 p-3 rounded bg-gray-900 border border-gray-700">' +
            '        ' + message +
            '    </pre>';
        
        if (recovery_options) {
            html += '    <div class="flex flex-wrap gap-2">' +
                recovery_options.map(function(opt) {
                    return '        <button class="btn-secondary px-3 py-1.5 rounded text-xs text-white hover:bg-red-600/20 transition-all">' + opt + '</button>';
                }).join('') +
                '    </div>';
        }
        
        html += '    </div>';
        return html;
    }

    /**
     * Рендеринг истории сообщений
     */
    renderHistory(sessions) {
        for (var i = 0; i < sessions.length; i++) {
            var session = sessions[i];
            var sessionContainer = document.createElement('div');
            sessionContainer.className = 'space-y-3';
            
            for (var j = 0; j < session.history.length; j++) {
                var msg = session.history[j];
                if (msg.role === 'user') {
                    sessionContainer.appendChild(this.renderUserMessage(msg.content, msg.images || []));
                } else if (msg.role === 'agent') {
                    sessionContainer.appendChild(this.renderAgentMessage(msg.content, msg.type || 'text'));
                }
            }
            
            this.container.appendChild(sessionContainer);
        }
    }

    /**
     * Очистка контейнера
     */
    clear() {
        this.container.innerHTML = '';
    }

    /**
     * Удаление последнего сообщения
     */
    removeLast() {
        var lastChild = this.container.lastElementChild;
        if (lastChild) {
            lastChild.remove();
        }
    }

    /**
     * Получение последнего сообщения
     */
    getLastMessage() {
        var lastChild = this.container.lastElementChild;
        return lastChild ? lastChild.dataset.messageId : null;
    }

    /**
     * Получить все сообщения
     */
    getMessages() {
        var messages = [];
        var elements = this.container.querySelectorAll('.message');
        for (var i = 0; i < elements.length; i++) {
            var msg = {
                id: elements[i].dataset.messageId,
                role: elements[i].className.includes('user') ? 'user' : 'agent',
                content: elements[i].querySelector('.message-content') ? elements[i].querySelector('.message-content').innerHTML : ''
            };
            messages.push(msg);
        }
        return messages;
    }

    /**
     * Прокрутка к низу
     */
    scrollToBottom() {
        this.container.scrollTop = this.container.scrollHeight;
    }
}

// Экспорт через CommonJS
module.exports = MessageRenderer;
