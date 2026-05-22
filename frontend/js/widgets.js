/**
 * Handles rendering interactive widgets in the chat interface.
 */
export class WidgetRenderer {
    constructor(messageList, socketManager, sessionManager) {
        this.messageList = messageList;
        this.socketManager = socketManager;
        this.sessionManager = sessionManager;
        this.activeWidgets = new Map(); // To keep track of active widgets
    }

    _appendWidget(widgetElement) {
        this.messageList.appendChild(widgetElement);
        this.messageList.scrollTop = this.messageList.scrollHeight;
    }

    _removeWidget(widgetId) {
        const widget = this.activeWidgets.get(widgetId);
        if (widget) {
            widget.remove();
            this.activeWidgets.delete(widgetId);
        }
    }

    /**
     * Renders a confirmation widget for a dangerous tool call.
     * @param {object} data - The data from the 'action_required' message.
     */
    renderConfirmation(data) {
        const template = document.getElementById('confirmation-widget-template');
        const widget = template.content.cloneNode(true).firstElementChild;

        widget.querySelector('[data-role="prompt-text"]').textContent = data.message;
        widget.querySelector('[data-role="tool-call-json"]').textContent = JSON.stringify(data.tool_call, null, 2);

        const yesButton = widget.querySelector('[data-role="yes-button"]');
        const noButton = widget.querySelector('[data-role="no-button"]');

        const handleConfirm = (confirmed) => {
            this.socketManager.send({
                type: 'confirm_action',
                session_id: this.sessionManager.getSessionId(),
                data: { confirmed }
            });
            widget.remove(); // Remove widget after action
        };

        yesButton.addEventListener('click', () => handleConfirm(true));
        noButton.addEventListener('click', () => handleConfirm(false));

        this._appendWidget(widget);
    }

    /**
     * Renders a plan confirmation and editor widget.
     * @param {object} data - The data from the 'plan_generated' message.
     */
    renderPlanConfirmation(data) {
        const template = document.getElementById('plan-confirmation-widget-template');
        const widget = template.content.cloneNode(true).firstElementChild;
        const planList = widget.querySelector('[data-role="plan-editor-list"]');

        const createStepItem = (stepText) => {
            const stepTemplate = document.getElementById('plan-step-item-template');
            const stepItem = stepTemplate.content.cloneNode(true).firstElementChild;
            stepItem.querySelector('.plan-step-text').textContent = stepText;
            stepItem.querySelector('.btn-remove-step').addEventListener('click', () => stepItem.remove());
            return stepItem;
        };

        data.steps.forEach(step => {
            planList.appendChild(createStepItem(step));
        });

        // Make list sortable
        if (typeof Sortable !== 'undefined') {
            new Sortable(planList, {
                animation: 150,
                handle: '.drag-handle',
            });
        }

        // Add step button
        widget.querySelector('[data-role="add-step-button"]').addEventListener('click', () => {
            planList.appendChild(createStepItem('Новый шаг...'));
        });

        // Confirmation buttons
        const approveButton = widget.querySelector('[data-role="approve-plan-button"]');
        const rejectButton = widget.querySelector('[data-role="reject-plan-button"]');

        const handleConfirm = (confirmed) => {
            const updatedSteps = confirmed ? Array.from(planList.querySelectorAll('.plan-step-text')).map(el => el.textContent.trim()) : [];
            this.socketManager.send({
                type: 'confirm_plan',
                session_id: this.sessionManager.getSessionId(),
                data: {
                    confirmed,
                    steps: updatedSteps
                }
            });
            widget.remove();
        };

        approveButton.addEventListener('click', () => handleConfirm(true));
        rejectButton.addEventListener('click', () => handleConfirm(false));

        this._appendWidget(widget);
    }

    /**
     * Renders or updates a plan progress widget.
     * @param {object} data - The data from the 'plan_update' message.
     */
    renderPlanProgress(data) {
        const widgetId = `plan-progress-${this.sessionManager.getSessionId()}`;
        this._removeWidget(widgetId); // Remove old progress widget if it exists

        const template = document.getElementById('plan-progress-widget-template');
        const widget = document.createElement('div');
        widget.className = 'message agent';
        widget.appendChild(template.content.cloneNode(true));

        const progressList = widget.querySelector('[data-role="progress-list"]');
        
        const statusIcons = {
            completed: '✅',
            current: '⏳',
            pending: '📋',
            failed: '❌'
        };

        data.steps.forEach(step => {
            const li = document.createElement('li');
            li.className = `flex items-start gap-2 text-sm step-${step.status}`;
            const icon = statusIcons[step.status] || '❓';
            li.innerHTML = `<span class="w-5">${icon}</span><span class="flex-1">${step.text}</span>`;
            progressList.appendChild(li);
        });

        this.activeWidgets.set(widgetId, widget);
        this._appendWidget(widget);
    }

    /**
     * Renders an error recovery widget.
     * @param {object} data - The data from the 'plan_error' message.
     */
    renderErrorRecovery(data) {
        const template = document.getElementById('error-recovery-widget-template');
        const widget = template.content.cloneNode(true).firstElementChild;

        widget.querySelector('[data-role="error-message"]').textContent = data.error_message;
        const buttonContainer = widget.querySelector('[data-role="button-container"]');

        data.recovery_options.forEach(option => {
            const button = document.createElement('button');
            button.className = 'px-3 py-1.5 rounded-md text-white bg-sky-600 hover:bg-sky-700 text-sm cursor-pointer';
            button.textContent = option;
            button.addEventListener('click', () => {
                this.socketManager.send({
                    type: 'confirm_error_recovery',
                    session_id: this.sessionManager.getSessionId(),
                    data: { option }
                });
                widget.remove();
            });
            buttonContainer.appendChild(button);
        });

        this._appendWidget(widget);
    }
}