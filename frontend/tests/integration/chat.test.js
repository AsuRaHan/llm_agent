/**
 * Интеграционные тесты для чата
 * 
 * @module tests/integration/chat.test
 */

// Mock WebSocket
const mockWebSocket = {
    readyState: 1,
    onopen: null,
    onmessage: null,
    onclose: null,
    onerror: null,
    send: null,
    close: null
};

global.window = {
    location: {
        protocol: 'https:'
    }
};

global.WebSocket = function(url) {
    return mockWebSocket;
};

import { SocketManager } from '../js/core/socket.js';
import { SessionManager } from '../js/core/session.js';
import { MessageRenderer } from '../js/ui/message.js';
import { WidgetFactory } from '../js/ui/widget.js';

describe('Chat Integration', () => {
    let socketManager;
    let sessionManager;
    let messageRenderer;
    let widgetFactory;

    beforeEach(() => {
        socketManager = new SocketManager('integration-test-session');
        sessionManager = new SessionManager();
        messageRenderer = new MessageRenderer();
        widgetFactory = new WidgetFactory();
    });

    afterEach(() => {
        if (socketManager) {
            socketManager.disconnect();
        }
    });

    describe('Message flow', () => {
        it('should handle query_response message', () => {
            const msg = {
                type: 'query_response',
                data: { answer: 'Hello, World!' }
            };
            
            socketManager.onMessage(msg);
            
            // Check that onMessage was called
            expect(socketManager.onMessage).toHaveBeenCalled();
        });

        it('should handle agent_thought message', () => {
            const msg = {
                type: 'agent_thought',
                data: { message: 'Thinking...' }
            };
            
            socketManager.onMessage(msg);
            expect(socketManager.onMessage).toHaveBeenCalled();
        });

        it('should handle action_required message', () => {
            const msg = {
                type: 'action_required',
                data: {
                    message: 'Confirm action',
                    tool_call: { function: { name: 'test' } }
                }
            };
            
            socketManager.onMessage(msg);
            expect(socketManager.onMessage).toHaveBeenCalled();
        });

        it('should handle plan_generated message', () => {
            const msg = {
                type: 'plan_generated',
                data: { steps: ['Step 1', 'Step 2'] }
            };
            
            socketManager.onMessage(msg);
            expect(socketManager.onMessage).toHaveBeenCalled();
        });

        it('should handle plan_error message', () => {
            const msg = {
                type: 'plan_error',
                data: {
                    error_message: 'Something went wrong',
                    recovery_options: ['retry', 'skip']
                }
            };
            
            socketManager.onMessage(msg);
            expect(socketManager.onMessage).toHaveBeenCalled();
        });
    });

    describe('Widget creation', () => {
        it('should create confirmation widget with correct structure', () => {
            const toolCall = { function: { name: 'test', args: {} } };
            const widget = widgetFactory.createConfirmationWidget('Confirm', toolCall);
            
            expect(widget.classList.contains('message')).toBe(true);
            expect(widget.classList.contains('agent')).toBe(true);
            expect(widget.querySelector('p')).toBeTruthy();
            expect(widget.querySelector('pre')).toBeTruthy();
            expect(widget.querySelector('button')).toBeTruthy();
        });

        it('should create plan widget with correct structure', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createPlanWidget(steps);
            
            expect(widget.classList.contains('message')).toBe(true);
            expect(widget.querySelector('ol')).toBeTruthy();
            expect(widget.querySelector('.btn-approve-plan')).toBeTruthy();
            expect(widget.querySelector('.btn-reject-plan')).toBeTruthy();
        });

        it('should create error widget with correct structure', () => {
            const recoveryOptions = ['retry', 'skip', 'abort'];
            const widget = widgetFactory.createErrorWidget('Error', recoveryOptions);
            
            expect(widget.classList.contains('message')).toBe(true);
            expect(widget.querySelector('p')).toBeTruthy();
            expect(widget.querySelectorAll('button').length).toBeGreaterThan(0);
        });

        it('should create progress widget with correct structure', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createProgressWidget(1, steps);
            
            expect(widget.classList.contains('message')).toBe(true);
            expect(widget.querySelector('ul')).toBeTruthy();
        });
    });

    describe('Session management', () => {
        it('should create and save session', () => {
            const sessionId = sessionManager.getSessionId();
            sessionManager.saveSession({ test: 'data' });
            
            expect(sessionId).toBeTruthy();
            expect(localStorage.getItem(`session_${sessionId}`)).toBeTruthy();
        });

        it('should load saved session', () => {
            const sessionId = 'test-session';
            const testData = { name: 'test', value: 123 };
            localStorage.setItem(`session_${sessionId}`, JSON.stringify(testData));
            
            const loaded = sessionManager.loadSession(sessionId);
            expect(loaded).toEqual(testData);
        });
    });

    describe('Message rendering', () => {
        it('should render user message', () => {
            const result = messageRenderer.renderUserMessage('Hello');
            expect(result).toContain('message');
            expect(result).toContain('user');
        });

        it('should render agent message', () => {
            const result = messageRenderer.renderAgentMessage('Response');
            expect(result).toContain('message');
            expect(result).toContain('agent');
        });

        it('should render error message', () => {
            const result = messageRenderer.renderErrorMessage('Error');
            expect(result).toContain('message');
            expect(result).toContain('error');
        });

        it('should render thought message', () => {
            const result = messageRenderer.renderThoughtMessage('Thinking');
            expect(result).toContain('message');
            expect(result).toContain('thought');
        });

        it('should escape HTML in messages', () => {
            const result = messageRenderer.renderUserMessage('<script>alert("xss")</script>');
            expect(result).not.toContain('<script>');
        });

        it('should render code blocks', () => {
            const result = messageRenderer.renderCodeBlock('console.log("test");');
            expect(result).toContain('CODE');
            expect(result).toContain('console.log("test");');
        });
    });

    describe('Socket communication', () => {
        it('should send query message', () => {
            socketManager.connect();
            socketManager.send('query', { text: 'Hello' });
            expect(mockWebSocket.send).toHaveBeenCalled();
        });

        it('should send confirm_action message', () => {
            socketManager.connect();
            socketManager.send('confirm_action', { session_id: 'test', data: { confirmed: true } });
            expect(mockWebSocket.send).toHaveBeenCalled();
        });

        it('should send confirm_plan message', () => {
            socketManager.connect();
            socketManager.send('confirm_plan', { session_id: 'test', data: { confirmed: true } });
            expect(mockWebSocket.send).toHaveBeenCalled();
        });

        it('should send clear_history message', () => {
            socketManager.connect();
            socketManager.send('clear_history', { session_id: 'test' });
            expect(mockWebSocket.send).toHaveBeenCalled();
        });

        it('should send control_file_watcher message', () => {
            socketManager.connect();
            socketManager.send('control_file_watcher', { data: { command: 'freeze' } });
            expect(mockWebSocket.send).toHaveBeenCalled();
        });
    });

    describe('Keep-alive', () => {
        it('should start keep-alive interval', () => {
            socketManager.startKeepAlive(30000);
            expect(socketManager.keepAliveInterval).toBeDefined();
        });

        it('should stop keep-alive interval', () => {
            socketManager.startKeepAlive(30000);
            socketManager.stopKeepAlive();
            expect(socketManager.keepAliveInterval).toBeNull();
        });

        it('should send ping when connected', () => {
            socketManager.connect();
            socketManager.ping();
            expect(mockWebSocket.send).toHaveBeenCalledWith(JSON.stringify({ type: 'ping' }));
        });
    });

    describe('Connection state', () => {
        it('should be connected after connect', () => {
            socketManager.connect();
            expect(socketManager.isConnected()).toBe(true);
        });

        it('should be disconnected initially', () => {
            expect(socketManager.isConnected()).toBe(false);
        });

        it('should disconnect after disconnect', () => {
            socketManager.connect();
            socketManager.disconnect();
            expect(socketManager.isConnected()).toBe(false);
        });
    });
});
