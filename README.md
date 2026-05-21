# Smart Hammer - Локальный AI-ассистент для анализа кода

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/iso-cpp-20)
[![CMake](https://img.shields.io/badge/CMake-3.15+-blue.svg)](https://cmake.org/)

**Smart Hammer** — это локальный AI-ассистент для анализа, поиска и редактирования кода. Работает на базе локального LLM-сервера с поддержкой эмбеддингов и tree-sitter парсинга. Предоставляет веб-интерфейс для взаимодействия с ассистентом.

---

## 📋 Содержание

- [Описание](#описание)
- [Функциональность](#функциональность)
- [Требования](#требования)
- [Установка](#установка)
- [Конфигурация](#конфигурация)
- [Использование](#использование)
- [Архитектура](#архитектура)
- [Инструменты](#инструменты)
- [Frontend](#frontend)
- [Разработка](#разработка)
- [Лицензия](#лицензия)

---

## Описание

Smart Hammer — это продвинутый локальный AI-ассистент, который помогает разработчикам анализировать, искать и редактировать код. Ассистент использует:

- **Tree-sitter** для семантического парсинга кода
- **Эмбеддинги** для семантического поиска в коде
- **WebSocket** для реального времени взаимодействия
- **Локальный LLM сервер** для генерации ответов

### Ключевые особенности

- ✅ Полная поддержка UTF-8 и кодировок
- ✅ Индексация проектов с поддержкой tree-sitter
- ✅ Семантический поиск по коду
- ✅ Редактирование файлов через AI
- ✅ Поддержка множественных языков программирования
- ✅ Веб-интерфейс для удобного взаимодействия
- ✅ Файловый мониторинг (FileWatcher)
- ✅ Модульная архитектура с инструментами

---

## Функциональность

### Основные возможности

1. **Анализ кода**
   - Семантический поиск по коду
   - Поиск по паттернам (grep)
   - Поиск по глобам (glob patterns)
   - Чтение и анализ файлов

2. **Редактирование кода**
   - Редактирование файлов
   - Применение diff-патчей
   - Проверка изменений

3. **Инструменты для разработки**
   - Чтение/запись файлов
   - Поиск в коде
   - Работа с Git
   - Web-поиск
   - Выполнение команд
   - Получение системной информации

4. **Управление сессиями**
   - Сохранение истории сессий
   - Управление контекстом
   - Индексация проектов

---

## Требования

### Системные требования

- **ОС**: Windows 7+, Linux, macOS
- **C++ Standard**: C++20
- **CMake**: 3.15+
- **Сеть**: Локальный LLM сервер с поддержкой эмбеддингов

### Зависимости

- **spdlog** — асинхронное логирование
- **cpp-httplib** — HTTP/WebSocket сервер
- **nlohmann/json** — JSON парсинг
- **hnswlib** — библиотека для эмбеддингов
- **tree-sitter** — парсеры для различных языков:
  - C/C++
  - CSS
  - Markdown
  - HTML
  - JavaScript

---

## Установка

### 1. Клонирование репозитория

```bash
git clone <repository-url>
cd llm_agent
```

### 2. Сборка с помощью CMake

```bash
# Windows
build.bat

# Linux/macOS
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 3. Запуск

```bash
# Windows
./Agent.exe

# Linux/macOS
./Agent
```

### 4. Запуск с указанием директории проекта

```bash
./Agent /путь/к/проекту
```

---

## Конфигурация

Конфигурация хранится в файле `.shdata/config.json`.

### Основные параметры

```json
{
  "server": {
    "host": "localhost",
    "port": 8080,
    "api_key": ""
  },
  "web_server": {
    "host": "localhost",
    "port": 9000
  },
  "embedding": {
    "model_name": "any",
    "max_text_length": 1500,
    "chunk_overlap": 200
  },
  "tools": {
    "enable_dangerous_tools": true
  },
  "logging": {
    "file_path": "agent.log",
    "console_level": "trace",
    "file_level": "trace"
  },
  "indexing": {
    "chunking_strategy": "tree-sitter-hybrid",
    "top_k_results": 5,
    "ignored_directories": [],
    "ignored_extensions": [],
    "ignored_files": []
  }
}
```

### Параметры сервера LLM

- **server_host**: Хост LLM сервера (по умолчанию: localhost)
- **server_port**: Порт LLM сервера (по умолчанию: 8080)
- **api_key**: API ключ для аутентификации
- **embedding_enabled**: Должен быть true для работы ассистента

### Параметры веб-сервера

- **web_server_host**: Хост веб-сервера (по умолчанию: localhost)
- **web_server_port**: Порт веб-сервера (по умолчанию: 9000)
- **enable_web_ui**: Включить веб-интерфейс

### Параметры индексации

- **chunking_strategy**: Стратегия разбивки:
  - `fixed` — фиксированный размер
  - `tree-sitter` — по синтаксическим узлам
  - `tree-sitter-hybrid` — гибридный подход
- **top_k_results**: Количество результатов поиска
- **ignored_directories**: Директории для игнорирования
- **ignored_extensions**: Расширения для игнорирования
- **ignored_files**: Файлы для игнорирования

---

## Использование

### Запуск в веб-режиме

```bash
./Agent /путь/к/проекту
```

После запуска откроется веб-интерфейс по адресу:
- **WebSocket**: `ws://localhost:9000/ws`
- **Frontend**: `http://localhost:9000/`

### Запуск в консольном режиме

```bash
./Agent /путь/к/проекту
```

### Остановка сервера

Нажмите `Ctrl+C` для грациозного завершения.

---

## Архитектура

### Структура проекта

```
llm_agent/
├── src/
│   ├── main.cpp                    # Точка входа
│   ├── Config.h/cpp                # Конфигурация
│   ├── ContextIndexer.h/cpp        # Индексация проектов
│   ├── ApiHandlers.h/cpp           # Обработчики API
│   ├── SessionManager.h/cpp        # Управление сессиями
│   ├── WebSocketServer.h/cpp       # WebSocket сервер
│   ├── FileWatcher.h/cpp           # Мониторинг файлов
│   ├── CodeParser.h/cpp            # Парсинг кода
│   ├── EmbeddingClient.h/cpp       # Клиент для эмбеддингов
│   ├── OpenAIProvider.h/cpp        # Провайдер LLM
│   ├── Logger.h/cpp                # Логирование
│   ├── ThreadPool.h/cpp            # Пул потоков
│   ├── tools/                      # Инструменты
│   │   ├── ReadFileTool.cpp
│   │   ├── WriteFileTool.cpp
│   │   ├── EditFileTool.cpp
│   │   ├── ApplyDiffTool.cpp
│   │   ├── CodeSearchTool.cpp
│   │   ├── GrepSearchTool.cpp
│   │   ├── FileGlobSearchTool.cpp
│   │   ├── WebSearchTool.cpp
│   │   ├── ExecuteShellCommandTool.cpp
│   │   └── ...
│   └── ContextIndexerHelper/       # Вспомогательные классы
│       ├── FileReader.cpp
│       ├── ChunkerStrategy.cpp
│       ├── IndexManager.cpp
│       ├── Searcher.cpp
│       └── FileIndexer.cpp
├── frontend/                       # Веб-интерфейс
│   ├── js/
│   │   ├── index.js
│   │   ├── core/
│   │   │   ├── socket.js
│   │   │   ├── session.js
│   │   │   └── storage.js
│   │   ├── ui/
│   │   │   ├── message.js
│   │   │   ├── widget.js
│   │   │   └── input.js
│   │   └── chat/
│   │       ├── chat.js
│   │       └── widgets/
│   ├── css/
│   ├── img/
│   └── index.html
├── .shdata/                        # Данные приложения
│   └── config.json
├── CMakeLists.txt                  # Конфигурация сборки
├── build.bat                       # Скрипт сборки для Windows
└── README.md                       # Документация
```

### Основные компоненты

#### ContextIndexer

Класс для индексации проектов:
- Индексация директорий
- Сохранение/загрузка индекса
- Поиск по эмбеддингам
- Управление файлами

#### ApiHandlers

Управляет HTTP/WebSocket сервером:
- Настройка маршрутов
- Обработка запросов
- Управление сессиями
- Мониторинг файлов

#### WebSocketServer

Обеспечивает реальное время взаимодействие:
- Подключение клиентов
- Отправка сообщений
- Обработка событий

#### FileWatcher

Мониторинг изменений файлов:
- Автоматическая реиндексация
- Обнаружение изменений
- Уведомления

---

## Инструменты

### Доступные инструменты

| Инструмент | Описание |
|------------|----------|
| `ReadFileTool` | Чтение файлов |
| `WriteFileTool` | Запись файлов |
| `EditFileTool` | Редактирование файлов |
| `ApplyDiffTool` | Применение diff-патчей |
| `CodeSearchTool` | Семантический поиск в коде |
| `GrepSearchTool` | Поиск по регулярным выражениям |
| `FileGlobSearchTool` | Поиск по glob-паттернам |
| `WebSearchTool` | Web-поиск |
| `ExecuteShellCommandTool` | Выполнение команд |
| `GetDateTimeTool` | Получение даты и времени |
| `GetSystemInfoTool` | Получение информации о системе |
| `GitHubSearchTool` | Поиск репозиториев GitHub |
| `ReadUrlTool` | Чтение URL |
| `OpenUrlTool` | Открытие URL в браузере |

### Использование инструментов

Инструменты вызваются через AI-ассистента в веб-интерфейсе. Ассистент автоматически выбирает подходящий инструмент для выполнения задачи.

---

## Frontend

### Структура

Frontend написан на JavaScript с модульной архитектурой:

```
frontend/
├── js/
│   ├── index.js              # Точка входа
│   ├── core/
│   │   ├── socket.js         # WebSocket управление
│   │   ├── session.js        # Управление сессией
│   │   └── storage.js        # localStorage operations
│   ├── ui/
│   │   ├── message.js        # Рендеринг сообщений
│   │   ├── widget.js         # Всплывающие виджеты
│   │   └── input.js          # Управление input
│   ├── chat/
│   │   ├── chat.js           # Основная логика чата
│   │   └── widgets/
│   │       ├── confirmation.js
│   │       ├── plan.js
│   │       └── error.js
│   └── doc.js                # Документация
├── tests/
│   ├── unit/
│   └── integration/
└── css/
```

### Запуск frontend

Frontend запускается автоматически вместе с backend. Открывается по адресу `http://localhost:9000/`.

### Рефакторинг

Frontend прошел рефакторинг на модульную архитектуру:
- ✅ Разделение на логические модули
- ✅ Изоляция зависимостей
- ✅ Четкая структура кода
- ✅ Unit-тесты для ключевых модулей

---

## Разработка

### Сборка

```bash
# Windows
build.bat

# Linux/macOS
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Добавление нового инструмента

1. Создайте файл в `src/tools/`
2. Добавьте в `CMakeLists.txt`
3. Реализуйте интерфейс `Tool`
4. Добавьте в `ToolManager`

### Добавление нового парсера tree-sitter

1. Добавьте грамматику в `CMakeLists.txt`
2. Добавьте исходники в `add_executable`
3. Добавьте путь к заголовочным файлам

### Frontend разработка

Frontend использует ES6 модули. Для разработки:

```bash
# Установка зависимостей
npm install

# Запуск тестов
npm test

# Запуск с покрытием
npm run test:coverage
```

---

## Лицензия

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

---

## Контакты

При возникновении вопросов обращайтесь к документации или к файлу `CHANGELOG.md`.

---

*Последнее обновление: 2024*
