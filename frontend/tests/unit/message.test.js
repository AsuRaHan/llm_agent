/**
 * Unit-тесты для MessageRenderer
 * 
 * @module tests/unit/message.test
 */

import { MessageRenderer } from '../js/ui/message.js';

describe('MessageRenderer', () => {
    let messageRenderer;

    beforeEach(() => {
        messageRenderer = new MessageRenderer();
    });

    describe('constructor', () => {
        it('should initialize with default options', () => {
            expect(messageRenderer.useMarkdown).toBe(false);
            expect(messageRenderer.marked).toBeNull();
        });

        it('should initialize with custom options', () => {
            const options = { useMarkdown: true, marked: null };
            const mr = new MessageRenderer(options);
            expect(mr.useMarkdown).toBe(true);
        });
    });

    describe('renderUserMessage', () => {
        it('should render user message', () => {
            const result = messageRenderer.renderUserMessage('Hello World');
            expect(result).toContain('message');
            expect(result).toContain('user');
        });

        it('should escape HTML in user message', () => {
            const result = messageRenderer.renderUserMessage('<script>alert("xss")</script>');
            expect(result).not.toContain('<script>');
        });
    });

    describe('renderAgentMessage', () => {
        it('should render agent message', () => {
            const result = messageRenderer.renderAgentMessage('Agent response');
            expect(result).toContain('message');
            expect(result).toContain('agent');
        });
    });

    describe('renderErrorMessage', () => {
        it('should render error message', () => {
            const result = messageRenderer.renderErrorMessage('Error occurred');
            expect(result).toContain('message');
            expect(result).toContain('error');
        });
    });

    describe('renderThoughtMessage', () => {
        it('should render thought message', () => {
            const result = messageRenderer.renderThoughtMessage('Thinking...');
            expect(result).toContain('message');
            expect(result).toContain('thought');
        });
    });

    describe('renderCodeBlock', () => {
        it('should render code block with wrapper', () => {
            const result = messageRenderer.renderCodeBlock('console.log("test");');
            expect(result).toContain('relative');
            expect(result).toContain('CODE');
            expect(result).toContain('console.log("test");');
        });

        it('should include copy button', () => {
            const result = messageRenderer.renderCodeBlock('test code');
            expect(result).toContain('Копировать');
        });

        it('should escape code content', () => {
            const result = messageRenderer.renderCodeBlock('<script>alert("xss")</script>');
            expect(result).not.toContain('<script>');
        });

        it('should include language in header', () => {
            const result = messageRenderer.renderCodeBlock('test', 'javascript');
            expect(result).toContain('javascript');
        });
    });

    describe('_escapeHtml', () => {
        it('should escape ampersand', () => {
            expect(messageRenderer._escapeHtml('&')).toBe('&amp;');
        });

        it('should escape less than', () => {
            expect(messageRenderer._escapeHtml('<')).toBe('&lt;');
        });

        it('should escape greater than', () => {
            expect(messageRenderer._escapeHtml('>')).toBe('&gt;');
        });

        it('should escape double quote', () => {
            expect(messageRenderer._escapeHtml('"')).toBe('&quot;');
        });

        it('should escape single quote', () => {
            expect(messageRenderer._escapeHtml("'")).toBe('&#039;');
        });

        it('should handle empty string', () => {
            expect(messageRenderer._escapeHtml('')).toBe('');
        });
    });

    describe('_sanitizeHtml', () => {
        it('should remove script tags', () => {
            const html = '<div><script>alert("xss")</script></div>';
            const result = messageRenderer._sanitizeHtml(html);
            expect(result).not.toContain('<script>');
        });

        it('should remove iframe tags', () => {
            const html = '<div><iframe src="evil.com"></iframe></div>';
            const result = messageRenderer._sanitizeHtml(html);
            expect(result).not.toContain('<iframe');
        });

        it('should preserve safe content', () => {
            const html = '<p>Hello <strong>World</strong></p>';
            const result = messageRenderer._sanitizeHtml(html);
            expect(result).toContain('Hello');
            expect(result).toContain('World');
        });
    });
});
