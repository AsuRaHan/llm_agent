/**
 * Input Handler
 * Обработка ввода пользователя
 */

class InputHandler {
    constructor(inputElement, sendButtonElement, previewContainerElement) {
        this.input = inputElement;
        this.sendButton = sendButtonElement;
        this.previewContainer = previewContainerElement;
        this.images = [];
        this.fileInput = null;
    }

    /**
     * Инициализация file input
     */
    initFileInput(fileInputElement) {
        this.fileInput = fileInputElement;
        
        // Обработка выбора файлов
        this.fileInput.addEventListener('change', (e) => {
            this.handleFileSelect(e.target.files);
        });
    }

    /**
     * Обработка выбора файлов
     */
    handleFileSelect(files) {
        if (!files || files.length === 0) return;

        for (var i = 0; i < files.length; i++) {
            var file = files[i];
            this.addImage(file);
        }

        // Сброс file input
        if (this.fileInput) {
            this.fileInput.value = '';
        }
    }

    /**
     * Добавление изображения
     */
    addImage(file) {
        // Проверка типа файла
        if (!file.type.startsWith('image/')) {
            console.warn('Не поддерживаемый тип файла:', file.name);
            return;
        }

        // Проверка размера (макс 5MB)
        if (file.size > 5 * 1024 * 1024) {
            alert('Файл слишком большой. Максимальный размер: 5MB');
            return;
        }

        // Чтение файла
        var reader = new FileReader();
        reader.onload = (e) => {
            this.images.push({
                url: e.target.result,
                name: file.name,
                size: file.size
            });
            this.renderPreview();
        };
        reader.onerror = () => {
            console.error('Ошибка чтения файла:', file.name);
        };
        reader.readAsDataURL(file);
    }

    /**
     * Рендеринг превью изображений
     */
    renderPreview() {
        this.previewContainer.innerHTML = '';

        for (var i = 0; i < this.images.length; i++) {
            var imgData = this.images[i];
            var preview = document.createElement('div');
            preview.className = 'image-preview relative';
            preview.dataset.imageIndex = i;

            var img = document.createElement('img');
            img.src = imgData.url;
            img.alt = imgData.name;
            img.className = 'max-w-xs max-h-48 object-cover';

            var removeBtn = document.createElement('button');
            removeBtn.className = 'remove-btn';
            removeBtn.innerHTML = '<i class="fas fa-times"></i>';
            removeBtn.title = 'Удалить';
            removeBtn.addEventListener('click', () => this.removeImage(i));

            preview.appendChild(img);
            preview.appendChild(removeBtn);
            this.previewContainer.appendChild(preview);
        }

        // Показать/скрыть контейнер
        if (this.images.length > 0) {
            this.previewContainer.classList.remove('hidden');
        } else {
            this.previewContainer.classList.add('hidden');
        }
    }

    /**
     * Удаление изображения по индексу
     */
    removeImage(index) {
        this.images.splice(index, 1);
        this.renderPreview();
    }

    /**
     * Удаление последнего изображения
     */
    removeLastImage() {
        if (this.images.length > 0) {
            this.images.pop();
            this.renderPreview();
        }
    }

    /**
     * Очистка всех изображений
     */
    clearImages() {
        this.images = [];
        this.renderPreview();
    }

    /**
     * Получить данные изображений
     */
    getImages() {
        return this.images;
    }

    /**
     * Проверка: есть ли изображения
     */
    hasImages() {
        return this.images.length > 0;
    }

    /**
     * Получить текст ввода
     */
    getText() {
        return this.input.value.trim();
    }

    /**
     * Очистка текста
     */
    clearText() {
        this.input.value = '';
        this.input.style.height = 'auto';
        this.sendButton.disabled = true;
    }

    /**
     * Установить текст
     */
    setText(text) {
        this.input.value = text;
        this.autoResize();
        this.updateSendButton();
    }

    /**
     * Авто-расширение textarea
     */
    autoResize() {
        this.input.style.height = 'auto';
        this.input.style.height = Math.min(this.input.scrollHeight, 200) + 'px';
    }

    /**
     * Обновить состояние кнопки отправки
     */
    updateSendButton() {
        var hasContent = this.getText().length > 0 || this.hasImages();
        this.sendButton.disabled = !hasContent;
    }

    /**
     * Отправка сообщения
     */
    send() {
        var text = this.getText();
        var images = this.getImages();

        // Очистка
        this.clearText();
        this.clearImages();

        // Возврат данных
        return {
            text: text,
            images: images
        };
    }

    /**
     * Отмена отправки
     */
    cancel() {
        this.clearText();
        this.clearImages();
    }

    /**
     * Фокус на input
     */
    focus() {
        this.input.focus();
    }

    /**
     * Размытие фокуса
     */
    blur() {
        this.input.blur();
    }

    /**
     * Получить значение input
     */
    getValue() {
        return this.input.value;
    }

    /**
     * Установить значение input
     */
    setValue(value) {
        this.input.value = value;
        this.autoResize();
        this.updateSendButton();
    }

    /**
     * Добавить слушатель события
     */
    on(event, callback) {
        if (event === 'input') {
            this.input.addEventListener('input', (e) => {
                this.autoResize();
                this.updateSendButton();
                callback(e);
            });
        } else if (event === 'keydown') {
            this.input.addEventListener('keydown', (e) => {
                callback(e);
            });
        } else if (event === 'send') {
            this.input.addEventListener('click', () => {
                callback();
            });
        }
    }

    /**
     * Удалить слушатель события
     */
    off(event, callback) {
        if (event === 'input') {
            this.input.removeEventListener('input', callback);
        } else if (event === 'keydown') {
            this.input.removeEventListener('keydown', callback);
        }
    }
}

// Экспорт для использования в других модулях
if (typeof module !== 'undefined' && module.exports) {
    module.exports = InputHandler;
}
