/**
 * Unit-тесты для SocketManager
 * 
 * @module tests/unit/socket.test
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

// Mock window.location
global.window = {
    location: {
        protocol: 'https:'
    }
};

// Mock WebSocket глобально
global.WebSocket = function(url) {
    return mockWebSocket;
};

import { SocketManager } from '../js/core/socket.js';

describe('SocketManager', () => {
    let socketManager;

    beforeEach(() => {
        socketManager = new SocketManager('test-session-id');
    });

    afterEach(() => {
        if (socketManager) {
            socketManager.disconnect();
        }
    });

    describe('constructor', () => {
        it('should initialize with null socket', () => {
            expect(socketManager.socket).toBeNull();
        });

        it('should store sessionId', () => {
            expect(socketManager.sessionId).toBe('test-session-id');
        });

        it('should initialize reconnectAttempts to 0', () => {
            expect(socketManager.reconnectAttempts).toBe(0);
        });

        it('should initialize maxReconnectAttempts to 5', () => {
            expect(socketManager.maxReconnectAttempts).toBe(5);
        });

        it('should initialize keepAliveInterval to null', () => {
            expect(socketManager.keepAliveInterval).toBeNull();
        });
    });

    describe('connect', () => {
        it('should create WebSocket connection', () => {
            socketManager.connect();
            expect(mockWebSocket).toBeDefined();
        });

        it('should call onopen handler', (done) => {
            socketManager.connect();
            expect(mockWebSocket.onopen).toBeDefined();
        });

        it('should call onmessage handler', (done) => {
            socketManager.connect();
            expect(mockWebSocket.onmessage).toBeDefined();
        });

        it('should call onclose handler', (done) => {
            socketManager.connect();
            expect(mockWebSocket.onclose).toBeDefined();
        });

        it('should call onerror handler', (done) => {
            socketManager.connect();
            expect(mockWebSocket.onerror).toBeDefined();
        });
    });

    describe('send', () => {
        it('should send message when socket is open', () => {
            socketManager.connect();
            socketManager.send('test_type', { data: 'test' });
            expect(mockWebSocket.send).toHaveBeenCalled();
        });

        it('should not send message when socket is closed', () => {
            mockWebSocket.readyState = 3;
            socketManager.send('test_type', { data: 'test' });
            expect(mockWebSocket.send).not.toHaveBeenCalled();
        });
    });

    describe('onMessage', () => {
        it('should call custom onMessage handler', () => {
            const customHandler = jest.fn();
            socketManager.onMessage = customHandler;
            socketManager.onMessage({ type: 'test' });
            expect(customHandler).toHaveBeenCalledWith({ type: 'test' });
        });
    });

    describe('ping', () => {
        it('should send ping when socket is open', () => {
            socketManager.connect();
            socketManager.ping();
            expect(mockWebSocket.send).toHaveBeenCalledWith(JSON.stringify({ type: 'ping' }));
        });
    });

    describe('startKeepAlive', () => {
        it('should start keep-alive interval', () => {
            socketManager.startKeepAlive(30000);
            expect(socketManager.keepAliveInterval).toBeDefined();
        });
    });

    describe('stopKeepAlive', () => {
        it('should stop keep-alive interval', () => {
            socketManager.startKeepAlive(30000);
            socketManager.stopKeepAlive();
            expect(socketManager.keepAliveInterval).toBeNull();
        });
    });

    describe('disconnect', () => {
        it('should disconnect socket', () => {
            socketManager.connect();
            socketManager.disconnect();
            expect(socketManager.socket).toBeNull();
        });
    });

    describe('isConnected', () => {
        it('should return true when connected', () => {
            socketManager.connect();
            expect(socketManager.isConnected()).toBe(true);
        });

        it('should return false when not connected', () => {
            expect(socketManager.isConnected()).toBe(false);
        });
    });
});
