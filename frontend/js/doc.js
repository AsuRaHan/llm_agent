document.addEventListener('DOMContentLoaded', () => {
    const contentDiv = document.getElementById('content');

    // Загружаем markdown файл из корня проекта
    fetch('/doc.md')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.text();
        })
        .then(markdown => {
            contentDiv.innerHTML = marked.parse(markdown);
        })
        .catch(error => {
            console.error('Error fetching or parsing documentation:', error);
            contentDiv.innerHTML = '<h2>Ошибка</h2><p>Не удалось загрузить файл документации `doc.md`. Убедитесь, что он находится в корневой директории проекта.</p>';
        });
});