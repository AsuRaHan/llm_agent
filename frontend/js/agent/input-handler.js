/**
 * Input Handler
 * Обработка ввода пользователя
 * Vanilla JS + CommonJS
 */

class InputHandler {
    constructor(agent) {
        this.agent = agent;
        this.inputElement = null;
        this.fileInput = null;
        this.imageInput = null;
    }

    /**
     * Инициализация элементов ввода
     */
    init(inputElement, fileInput, imageInput) {
        this.inputElement = inputElement;
        this.fileInput = fileInput;
        this.imageInput = imageInput;

        // Обработчик Enter
        this.inputElement.addEventListener('keydown', (event) => {
            if (event.key === 'Enter' && !event.shiftKey) {
                event.preventDefault();
                this.handleSendClick();
            }
        });

        // Обработчик Shift+Enter
        this.inputElement.addEventListener('keydown', (event) => {
            if (event.key === 'Enter' && event.shiftKey) {
                event.preventDefault();
                this.handleNewline();
            }
        });

        // Обработчик загрузки файлов
        this.fileInput.addEventListener('change', (event) => {
            this.handleFileUpload(event);
        });

        // Обработчик загрузки изображений
        this.imageInput.addEventListener('change', (event) => {
            this.handleImageUpload(event);
        });
    }

    /**
     * Обработка отправки сообщения
     */
    handleSendClick() {
        var text = this.getInputValue().trim();
        if (!text) return;

        var images = this.getImageFiles();
        var message = {
            role: 'user',
            content: text,
            images: images
        };

        this.agent.sendMessage(message);
        this.clearInput();
    }

    /**
     * Обработка нового строка
     */
    handleNewline() {
        var text = this.getInputValue().trim();
        if (text) {
            var message = {
                role: 'user',
                content: text + '\n',
                images: []
            };
            this.agent.sendMessage(message);
            this.clearInput();
        }
    }

    /**
     * Обработка загрузки файлов
     */
    handleFileUpload(event) {
        var files = event.target.files;
        if (!files || files.length === 0) return;

        var message = {
            role: 'user',
            content: 'Загружено файлов: ' + files.length,
            images: []
        };

        for (var i = 0; i < files.length; i++) {
            var file = files[i];
            var reader = new FileReader();
            reader.onload = (e) => {
                message.images.push({
                    url: e.target.result,
                    name: file.name,
                    type: file.type
                });
            };
            reader.readAsDataURL(file);
        }

        this.agent.sendMessage(message);
        this.clearInput();
    }

    /**
     * Обработка загрузки изображений
     */
    handleImageUpload(event) {
        var files = event.target.files;
        if (!files || files.length === 0) return;

        var message = {
            role: 'user',
            content: 'Загружено изображений: ' + files.length,
            images: []
        };

        for (var i = 0; i < files.length; i++) {
            var file = files[i];
            var reader = new FileReader();
            reader.onload = (e) => {
                message.images.push({
                    url: e.target.result,
                    name: file.name,
                    type: file.type
                });
            };
            reader.readAsDataURL(file);
        }

        this.agent.sendMessage(message);
        this.clearInput();
    }

    /**
     * Обработка клика по кнопке очистки
     */
    handleClearClick() {
        this.clearInput();
    }

    /**
     * Обработка клика по виджету
     */
    handleWidgetClick(event) {
        var target = event.target;
        if (target.closest('[data-role="yes-btn"]')) {
            var widget = target.closest('.confirmation-widget');
            var data = this.parseWidgetData(widget);
            this.agent.confirmAction(data);
        } else if (target.closest('[data-role="no-btn"]')) {
            var widget = target.closest('.confirmation-widget');
            var data = this.parseWidgetData(widget);
            this.agent.rejectAction(data);
        } else if (target.closest('[data-role="approve-btn"]')) {
            var widget = target.closest('.plan-widget');
            var data = this.parseWidgetData(widget);
            this.agent.executePlan(data);
        } else if (target.closest('[data-role="edit-btn"]')) {
            var widget = target.closest('.plan-widget');
            var data = this.parseWidgetData(widget);
            this.agent.editPlan(data);
        } else if (target.closest('[data-role="error-btn"]')) {
            var widget = target.closest('.error-widget');
            var data = this.parseWidgetData(widget);
            this.agent.recoverFromError(data);
        }
    }

    /**
     * Получение значения ввода
     */
    getInputValue() {
        return this.inputElement ? this.inputElement.value : '';
    }

    /**
     * Установка значения ввода
     */
    setInputValue(value) {
        if (this.inputElement) {
            this.inputElement.value = value;
        }
    }

    /**
     * Очистка ввода
     */
    clearInput() {
        if (this.inputElement) {
            this.inputElement.value = '';
        }
    }

    /**
     * Получение файлов изображений
     */
    getImageFiles() {
        if (this.imageInput) {
            var files = this.imageInput.files;
            if (files && files.length > 0) {
                var images = [];
                for (var i = 0; i < files.length; i++) {
                    var file = files[i];
                    var reader = new FileReader();
                    reader.onload = (e) => {
                        images.push({
                            url: e.target.result,
                            name: file.name,
                            type: file.type
                        });
                    };
                    reader.readAsDataURL(file);
                }
                return images;
            }
        }
        return [];
    }

    /**
     * Парсинг данных из виджета
     */
    parseWidgetData(widget) {
        var data = {};
        var yesBtn = widget.querySelector('[data-role="yes-btn"]');
        var noBtn = widget.querySelector('[data-role="no-btn"]');
        var approveBtn = widget.querySelector('[data-role="approve-btn"]');
        var errorBtn = widget.querySelector('[data-role="error-btn"]');

        if (yesBtn) data.confirmed = true;
        if (noBtn) data.confirmed = false;
        if (approveBtn) data.approved = true;
        if (errorBtn) data.recovered = true;

        return data;
    }
}

// Экспорт через CommonJS
module.exports = InputHandler;
