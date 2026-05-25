/**
 * Класс для управления формой ввода в чате, включая текст, прикрепление файлов и вставку из буфера обмена.
 */
class InputHandler {
    /**
     * @param {object} options - Опции для инициализации.
     * @param {HTMLTextAreaElement} options.messageInput - Поле для ввода текста.
     * @param {HTMLButtonElement} options.sendButton - Кнопка отправки.
     * @param {HTMLButtonElement} options.attachFileButton - Кнопка для прикрепления файлов.
     * @param {HTMLInputElement} options.fileInput - Скрытый input для выбора файлов.
     * @param {HTMLElement} options.imagePreviewContainer - Контейнер для предпросмотра изображений.
     * @param {(payload: {text: string, images: object[]}) => void} options.onSend - Callback-функция для отправки сообщения.
     */
    constructor(options) {
        this.elements = options;
        this.onSend = options.onSend;
        this.attachedImages = [];

        this._init();
    }

    /**
     * Инициализирует все обработчики событий.
     * @private
     */
    _init() {
        this.elements.sendButton.addEventListener('click', () => this._sendMessage());
        this.elements.messageInput.addEventListener('keydown', (event) => {
            if (event.key === 'Enter' && !event.shiftKey) {
                event.preventDefault();
                this._sendMessage();
            }
        });
        this.elements.messageInput.addEventListener('input', () => this._autoResize());
        this.elements.messageInput.addEventListener('paste', (event) => this._handlePaste(event));

        if (this.elements.attachFileButton) {
            this.elements.attachFileButton.addEventListener('click', () => {
                if (this.elements.fileInput) this.elements.fileInput.click();
            });
        }

        if (this.elements.fileInput) {
            this.elements.fileInput.addEventListener('change', (event) => this._handleFileSelect(event));
        }
    }

    /**
     * Собирает данные и вызывает onSend callback, затем очищает форму.
     * @private
     */
    _sendMessage() {
        const text = this.elements.messageInput.value.trim();
        if (!text && this.attachedImages.length === 0) {
            return;
        }

        if (typeof this.onSend === 'function') {
            this.onSend({ text, images: this.attachedImages });
        }

        this.clear();
    }
    
    /**
     * Очищает поле ввода и сбрасывает прикрепленные изображения.
     */
    clear() {
        this.elements.messageInput.value = '';
        this.attachedImages = [];
        this.elements.imagePreviewContainer.innerHTML = '';
        this._autoResize();
    }

    _autoResize() {
        this.elements.messageInput.style.height = 'auto';
        this.elements.messageInput.style.height = (this.elements.messageInput.scrollHeight) + 'px';
    }

    /**
     * Обрабатывает вставку из буфера обмена, ищет изображения.
     * @private
     */
    _handlePaste(event) {
        const items = (event.clipboardData || window.clipboardData).items;
        let imageFound = false;
        for (let i = 0; i < items.length; i++) {
            if (items[i].type.indexOf('image') !== -1) {
                const file = items[i].getAsFile();
                if (file) {
                    this._handleImageFile(file);
                    imageFound = true;
                }
            }
        }
        if (imageFound) {
            event.preventDefault(); // Предотвращаем вставку изображения как текста
        }
    }

    /**
     * Обрабатывает выбор файлов через диалоговое окно.
     * @private
     */
    _handleFileSelect(event) {
        const files = event.target.files;
        if (!files) return;

        for (const file of files) {
            this._handleImageFile(file);
        }
        this.elements.fileInput.value = '';
    }

    /**
     * Обрабатывает один файл изображения: читает, кодирует и создает превью.
     * @private
     */
    _handleImageFile(file) {
        if (!file.type.startsWith('image/')) {
            console.warn(`Файл ${file.name} не является изображением и был проигнорирован.`);
            return;
        }
        const reader = new FileReader();
        reader.onload = (e) => {
            const fullDataUrl = e.target.result;
            const base64String = fullDataUrl.split(',')[1];
            const imageData = { type: file.type, data: base64String };
            this.attachedImages.push(imageData);
            
            const onRemove = () => {
                const index = this.attachedImages.findIndex(img => img.data === imageData.data);
                if (index > -1) {
                    this.attachedImages.splice(index, 1);
                }
            };

            if (typeof createImagePreview === 'function') {
                createImagePreview(fullDataUrl, onRemove);
            }
        };
        reader.readAsDataURL(file);
    }
}