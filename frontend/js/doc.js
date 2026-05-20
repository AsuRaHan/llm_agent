/**
 * doc.js - Загрузка и отображение документации
 * 
 * @module doc/doc
 */

document.addEventListener('DOMContentLoaded', () => {
    const contentDiv = document.getElementById('content');

    // Инициализация MessageRenderer
    const messageRenderer = new MessageRenderer({
        useMarkdown: true
    });

    // Загружаем markdown файл из корня проекта
    fetch('/doc.md')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.text();
        })
        .then(markdown => {
            // Используем marked для парсинга
            const rawHtml = marked.parse(markdown);
            contentDiv.innerHTML = rawHtml;
        })
        .catch(error => {
            console.error('Error fetching or parsing documentation:', error);
            contentDiv.innerHTML = '<h2>Ошибка</h2><p>Не удалось загрузить файл документации `doc.md`. Убедитесь, что он находится в корневой директории проекта.</p>';
        });
});
