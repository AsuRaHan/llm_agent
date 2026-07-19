# 🤖 Smart Hammer — AI Code Assistant

**Smart Hammer** — это высокопроизводительный, локальный AI-агент, разработанный на C++20, который автоматически анализирует ваш код, выполняет инструменты и взаимодействует с пользователем через WebSocket и веб-интерфейс. Агент использует семантический поиск по файлам проекта, интегрируется с внешними LLM-серверами и поддерживает многопользовательские сессии.

## ✨ Основные возможности

| Функция | Описание |
|--------|----------|
| 🔍 **Семантический поиск кода** | Индексация файлов с помощью HNSW-графов (hnswlib) и tree-sitter-грамматик. Поиск по смыслу, а не по точному совпадению слов. |
| 🔄 **ReAct-цикл** | Агент сам планирует действия, запрашивает у LLM, выполняет инструменты и обновляет контекст. |
| 🛠️ **Инструменты** | Чтение/запись/редактирование файлов, выполнение shell-команд, поиск в интернете, поиск в Git, получение времени/информации о системе. |
| 🌐 **WebSocket API** | Реальное время взаимодействие с фронтендом. Поддержка ping/pong, синхронизации сессий, потоковой отправки токенов. |
| 📊 **Управление сессиями** | Многопользовательская поддержка. Каждая сессия имеет свою историю, статус и независимый индекс. |
| 🔄 **Auto-Reindexing** | `FileWatcher` отслеживает изменения файлов и автоматически обновляет индекс без перезапуска агента. |
| 🖥️ **Веб-интерфейс** | Готовый HTML/CSS/JS фронтенд для взаимодействия с агентом. |

## 🏗️ Архитектура

```
┌─────────────┐     WebSocket      ┌──────────────────┐
│   Frontend  │ ◄────────────────► │  WebSocketServer │
└─────────────┘                    └────────┬─────────┘
                                            │
                    ┌───────────────────────┼───────────────────────┐
                    │                       │                       │
          ┌─────────▼─────────┐   ┌────────▼─────────┐   ┌────────▼─────────┐
          │   SessionManager  │   │   ApiHandlers     │   │  ContextIndexer  │
          │ (State & History) │   │ (ReAct Loop)     │   │ (HNSW + Chunks)  │
          └─────────┬─────────┘   └────────┬─────────┘   └────────┬─────────┘
                    │                       │                       │
          ┌─────────▼─────────┐   ┌────────▼─────────┐   ┌────────▼─────────┐
          │  ThreadPool       │   │  LLMProvider     │   │  CodeParser      │
          │ (Background Jobs) │   │ (OpenAI/Compatible)│ │ (tree-sitter)    │
          └─────────┬─────────┘   └────────┬─────────┘   └────────┬─────────┘
                    │                       │                       │
                    ▼                       ▼                       ▼
              ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
              │  External LLM    │   │  FileIndexer   │   │  EmbeddingClient │
              │  (v1/embeddings) │   │ (Disk Scan)    │   │ (Vector DB)      │
              └──────────────────┘   └──────────────────┘   └──────────────────┘
```

## 📦 Зависимости

| Библиотека | Назначение |
|------------|------------|
| `cpp-httplib` | HTTP/WebSocket сервер |
| `nlohmann/json` | Парсинг JSON |
| `spdlog` | Логирование |
| `hnswlib` | Векторная база данных (HNSW) |
| `tree-sitter` | Парсинг кода (C++, JS, HTML, CSS, Markdown) |
| `OpenSSL` | SSL/TLS для HTTPS |
| `std::filesystem` | Работа с файловой системой (C++17+) |

## 🛠️ Требования к системе

- **OS:** Windows 10/11, Linux, macOS
- **C++ Compiler:** GCC 11+, Clang 11+, MSVC 19.29+ (C++20)
- **CMake:** 3.15+
- **Git:** Для клонирования зависимостей
- **LLM Server:** Внешний сервер (OpenAI-compatible), запущенный с флагом `--embedding`
- **Node.js:** Для запуска фронтенда (опционально)

## 📖 Установка и настройка

### 1. Сборка
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)   # или cmake --build .
```

### 2. Конфигурация
Агент ищет `config.json` в папке `.shdata/`. Если файла нет, он создаётся автоматически.

**Пример `config.json`:**
```json
{
  "server": {
    "host": "127.0.0.1",
    "port": 8080,
    "api_key": "sk-your-openai-key",
    "retry_count": 3,
    "retry_delay_ms": 1000
  },
  "embedding": {
    "max_text_length": 8192,
    "chunk_overlap": 1024,
    "model_name": "text-embedding-3-small",
    "timeout_sec": 10
  },
  "assistant": {
    "chat_completion_timeout_sec": 30,
    "summary_generation_timeout_sec": 15,
    "max_tool_calls": 5,
    "model_name": "gpt-4o"
  },
  "tools": {
    "enable_dangerous_tools": true,
    "dangerous_tools": ["write_file", "edit_file", "apply_diff", "execute_shell_command"]
  },
  "web_search": {
    "api_key": "your-web-search-api-key"
  },
  "logging": {
    "log_file_path": ".shdata/agent.log",
    "log_to_console": true,
    "log_file_level": "INFO",
    "log_console_level": "DEBUG"
  },
  "web_server": {
    "host": "127.0.0.1",
    "port": 8080,
    "enable_web_ui": true,
    "root_dir": "frontend"
  },
  "indexing": {
    "top_k_results": 5,
    "initial_index_size": 10000,
    "chunking_strategy": "tree-sitter-hybrid",
    "ignored_directories": ["build", ".git", ".vscode", "CMakeFiles", ".shdata", "node_modules"],
    "ignored_extensions": [".exe", ".obj", ".pdb", ".dll", ".so", ".png", ".jpg", ".pdf", ".log"],
    "ignored_files": [".gitignore"]
  }
}
```

### 3. Запуск
```bash
./Agent.exe .          # Windows
./Agent .              # Linux/macOS
```
Агент запустит WebSocket-сервер и откроет веб-интерфейс по адресу `http://localhost:8080`.

## 🚀 Использование

### WebSocket API
```json
// Подключение
ws://localhost:8080/ws

// Синхронизация сессии
{"type": "sync_session", "session_id": "session_123"}

// Отправка запроса
{"type": "query", "session_id": "session_123", "data": {"text": "Найди функцию для валидации email и объясни, как она работает"}}

// Подтверждение опасного инструмента
{"type": "confirm_action", "session_id": "session_123", "data": {"confirmed": true}}

// Очистка истории
{"type": "clear_history", "session_id": "session_123"}
```

### Фронтенд
Откройте `frontend/index.html` в браузере. Интерфейс поддерживает:
- Отправка текстовых запросов
- Отправка изображений (мультимодальность)
- Потоковую отправку ответов
- Управление сессиями и историей

## ⚠️ Безопасность и ограничения

- **Опасные инструменты** (`write_file`, `edit_file`, `apply_diff`, `execute_shell_command`) требуют **явного подтверждения** пользователя перед выполнением.
- Агент **не хранит данные** на сервере LLM. Все эмбеддинги и индекс хранятся локально в `.shdata/`.
- Требуется **C++20** и **OpenSSL**.
- Для работы **обязательно** должен быть запущен LLM-сервер с поддержкой `/v1/embeddings`.
- Автоматическое индексирование может занимать время при первом запуске или при большом количестве файлов.

## 📜 Лицензия
MIT License (по умолчанию)

---
*Генерировано автоматически на основе анализа кодовой базы `src/`, `CMakeLists.txt`, `config.json` и архитектуры проекта.*  
Для поддержки или запросов на кастомизацию обращайтесь в `agent.log` или через WebSocket-интерфейс.