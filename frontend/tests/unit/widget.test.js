/**
 * Unit-тесты для WidgetFactory
 * 
 * @module tests/unit/widget.test
 */

import { WidgetFactory } from '../js/ui/widget.js';

describe('WidgetFactory', () => {
    let widgetFactory;

    beforeEach(() => {
        widgetFactory = new WidgetFactory();
    });

    describe('createConfirmationWidget', () => {
        it('should create confirmation widget', () => {
            const toolCall = { function: { name: 'test', args: {} } };
            const widget = widgetFactory.createConfirmationWidget('Confirm action', toolCall);
            
            expect(widget).toBeDefined();
            expect(widget.classList.contains('message')).toBe(true);
            expect(widget.classList.contains('agent')).toBe(true);
        });

        it('should include prompt text', () => {
            const toolCall = { function: { name: 'test' } };
            const widget = widgetFactory.createConfirmationWidget('Test message', toolCall);
            
            expect(widget.querySelector('p')).toBeTruthy();
        });

        it('should include tool call JSON', () => {
            const toolCall = { function: { name: 'test', args: { value: 123 } } };
            const widget = widgetFactory.createConfirmationWidget('Test', toolCall);
            
            const pre = widget.querySelector('pre');
            expect(pre).toBeTruthy();
        });

        it('should include Yes button', () => {
            const toolCall = { function: {} };
            const widget = widgetFactory.createConfirmationWidget('Test', toolCall);
            
            const yesBtn = widget.querySelector('button');
            expect(yesBtn).toBeTruthy();
            expect(yesBtn.textContent).toBe('Да');
        });

        it('should include No button', () => {
            const toolCall = { function: {} };
            const widget = widgetFactory.createConfirmationWidget('Test', toolCall);
            
            const noBtn = widget.querySelector('button');
            expect(noBtn).toBeTruthy();
            expect(noBtn.textContent).toBe('Нет');
        });
    });

    describe('createPlanWidget', () => {
        it('should create plan widget', () => {
            const steps = ['Step 1', 'Step 2', 'Step 3'];
            const widget = widgetFactory.createPlanWidget(steps);
            
            expect(widget).toBeDefined();
            expect(widget.classList.contains('message')).toBe(true);
        });

        it('should include plan list', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createPlanWidget(steps);
            
            const ol = widget.querySelector('ol');
            expect(ol).toBeTruthy();
        });

        it('should include approve button', () => {
            const steps = ['Step 1'];
            const widget = widgetFactory.createPlanWidget(steps);
            
            const approveBtn = widget.querySelector('.btn-approve-plan');
            expect(approveBtn).toBeTruthy();
            expect(approveBtn.textContent).toContain('Утвердить');
        });

        it('should include reject button', () => {
            const steps = ['Step 1'];
            const widget = widgetFactory.createPlanWidget(steps);
            
            const rejectBtn = widget.querySelector('.btn-reject-plan');
            expect(rejectBtn).toBeTruthy();
            expect(rejectBtn.textContent).toContain('Отмена');
        });

        it('should render all steps', () => {
            const steps = ['Step 1', 'Step 2', 'Step 3'];
            const widget = widgetFactory.createPlanWidget(steps);
            
            const liElements = widget.querySelectorAll('li');
            expect(liElements.length).toBe(3);
        });
    });

    describe('createErrorWidget', () => {
        it('should create error widget', () => {
            const recoveryOptions = ['retry', 'skip'];
            const widget = widgetFactory.createErrorWidget('Error message', recoveryOptions);
            
            expect(widget).toBeDefined();
            expect(widget.classList.contains('message')).toBe(true);
        });

        it('should include error message', () => {
            const recoveryOptions = ['retry'];
            const widget = widgetFactory.createErrorWidget('Test error', recoveryOptions);
            
            const p = widget.querySelector('p');
            expect(p).toBeTruthy();
        });

        it('should include recovery buttons', () => {
            const recoveryOptions = ['retry', 'skip'];
            const widget = widgetFactory.createErrorWidget('Test error', recoveryOptions);
            
            const buttons = widget.querySelectorAll('button');
            expect(buttons.length).toBeGreaterThan(0);
        });

        it('should include abort option by default', () => {
            const recoveryOptions = ['retry'];
            const widget = widgetFactory.createErrorWidget('Test error', recoveryOptions);
            
            const buttons = widget.querySelectorAll('button');
            const hasAbort = Array.from(buttons).some(btn => btn.textContent.includes('Отменить'));
            expect(hasAbort).toBe(true);
        });
    });

    describe('createProgressWidget', () => {
        it('should create progress widget', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createProgressWidget(1, steps);
            
            expect(widget).toBeDefined();
            expect(widget.classList.contains('message')).toBe(true);
        });

        it('should render completed steps with checkmark', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createProgressWidget(1, steps);
            
            const liElements = widget.querySelectorAll('li');
            expect(liElements[0].textContent).toContain('✅');
        });

        it('should render current step with gear icon', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createProgressWidget(1, steps);
            
            const liElements = widget.querySelectorAll('li');
            expect(liElements[1].textContent).toContain('⚙️');
        });

        it('should render pending steps with clock icon', () => {
            const steps = ['Step 1', 'Step 2'];
            const widget = widgetFactory.createProgressWidget(0, steps);
            
            const liElements = widget.querySelectorAll('li');
            expect(liElements[0].textContent).toContain('⏳');
        });
    });

    describe('_escapeHtml', () => {
        it('should escape HTML characters', () => {
            expect(widgetFactory._escapeHtml('<script>'))
                .not.toContain('<script>');
        });

        it('should handle empty string', () => {
            expect(widgetFactory._escapeHtml('')).toBe('');
        });
    });
});
