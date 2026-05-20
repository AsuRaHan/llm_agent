/**
 * Точка входа в frontend
 * 
 * @module index
 */

import { SocketManager } from './core/socket.js';
import { SessionManager } from './core/session.js';
import { MessageRenderer } from './ui/message.js';
import { WidgetFactory } from './ui/widget.js';

// Экспорт для использования в других модулях
export { SocketManager, SessionManager, MessageRenderer, WidgetFactory };

/**
 * Инициализация приложения
 * @param {Object} options - Опции инициализации
 * @param {string} options.sessionId - ID сессии
 * @param {Object} options.socketManager - SocketManager
 * @param {Object} options.sessionManager - SessionManager
 * @param {Object} options.messageRenderer - MessageRenderer
 * @param {Object} options.widgetFactory - WidgetFactory
 */
function initApp(options = {}) {
    const {
        sessionId,
        socketManager,
        sessionManager,
        messageRenderer,
        widgetFactory
    } = options;

    console.log('Frontend initialized');
    console.log('Session ID:', sessionId);

    // Инициализация SocketManager
    if (socketManager) {
        socketManager.connect();
        socketManager.startKeepAlive(30000);
    }

    // Инициализация MessageRenderer
    if (messageRenderer) {
        messageRenderer.useMarkdown = true;
    }

    // Инициализация WidgetFactory
    if (widgetFactory) {
        widgetFactory.createConfirmationWidget = function(message, toolCall) {
            return this.createConfirmationWidget(message, toolCall);
        };
    }

    return {
        socketManager,
        sessionManager,
        messageRenderer,
        widgetFactory
    };
}

// Экспорт функции инициализации
export { initApp };
