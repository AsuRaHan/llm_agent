/**
 * Unit-тесты для SessionManager
 * 
 * @module tests/unit/session.test
 */

import { SessionManager } from '../js/core/session.js';

describe('SessionManager', () => {
    let sessionManager;

    beforeEach(() => {
        sessionManager = new SessionManager('sessionId');
    });

    describe('constructor', () => {
        it('should initialize with default storageKey', () => {
            const sm = new SessionManager();
            expect(sm.storageKey).toBe('sessionId');
        });

        it('should initialize with custom storageKey', () => {
            const sm = new SessionManager('customKey');
            expect(sm.storageKey).toBe('customKey');
        });
    });

    describe('getSessionId', () => {
        it('should return existing sessionId from localStorage', () => {
            localStorage.setItem('sessionId', 'existing-id');
            const sm = new SessionManager('sessionId');
            expect(sm.getSessionId()).toBe('existing-id');
        });

        it('should create new sessionId if not exists', () => {
            const sm = new SessionManager('sessionId');
            const id = sm.getSessionId();
            expect(id).toMatch(/^sess_/);
            expect(id.length).toBeGreaterThan(0);
        });

        it('should store new sessionId in localStorage', () => {
            const sm = new SessionManager('sessionId');
            sm.getSessionId();
            expect(localStorage.getItem('sessionId')).toBeTruthy();
        });
    });

    describe('saveSession', () => {
        it('should save session data to localStorage', () => {
            const testData = { name: 'test', value: 123 };
            sessionManager.saveSession(testData);
            const saved = localStorage.getItem('session_sess_' + Math.random().toString(36).substr(2, 9));
            expect(saved).toBeTruthy();
        });

        it('should include timestamp in saved session', () => {
            const testData = { name: 'test' };
            sessionManager.saveSession(testData);
            const saved = localStorage.getItem('session_' + sessionManager.getSessionId());
            const parsed = JSON.parse(saved);
            expect(parsed.timestamp).toBeTruthy();
            expect(parsed.timestamp).toMatch(/^\d{4}-\d{2}-\d{2}T/);
        });
    });

    describe('loadSession', () => {
        it('should return null for non-existent session', () => {
            const result = sessionManager.loadSession('non-existent-id');
            expect(result).toBeNull();
        });

        it('should return session data for existing session', () => {
            const testData = { name: 'test', value: 123 };
            const sessionId = 'test-session-id';
            localStorage.setItem(`session_${sessionId}`, JSON.stringify(testData));
            const result = sessionManager.loadSession(sessionId);
            expect(result).toEqual(testData);
        });
    });

    describe('deleteSession', () => {
        it('should delete session from localStorage', () => {
            const sessionId = 'test-session-id';
            localStorage.setItem(`session_${sessionId}`, JSON.stringify({ name: 'test' }));
            sessionManager.deleteSession(sessionId);
            expect(localStorage.getItem(`session_${sessionId}`)).toBeNull();
        });
    });

    describe('clearAllSessions', () => {
        it('should remove all session keys from localStorage', () => {
            localStorage.setItem('session_1', JSON.stringify({}));
            localStorage.setItem('session_2', JSON.stringify({}));
            localStorage.setItem('other_key', JSON.stringify({}));
            
            sessionManager.clearAllSessions();
            
            expect(localStorage.getItem('session_1')).toBeNull();
            expect(localStorage.getItem('session_2')).toBeNull();
            expect(localStorage.getItem('other_key')).toBe(JSON.stringify({}));
        });
    });

    describe('getAllSessions', () => {
        it('should return empty array when no sessions exist', () => {
            const sessions = sessionManager.getAllSessions();
            expect(sessions).toEqual([]);
        });

        it('should return all sessions', () => {
            localStorage.setItem('session_1', JSON.stringify({ id: 1 }));
            localStorage.setItem('session_2', JSON.stringify({ id: 2 }));
            
            const sessions = sessionManager.getAllSessions();
            expect(sessions.length).toBe(2);
        });

        it('should skip invalid JSON sessions', () => {
            localStorage.setItem('session_invalid', 'not json');
            
            const sessions = sessionManager.getAllSessions();
            expect(sessions).not.toContainEqual({ id: 'session_invalid' });
        });
    });
});
