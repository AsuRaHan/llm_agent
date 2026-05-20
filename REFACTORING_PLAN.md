# План рефакторинга: Внедрение слоя абстракции LLM Provider

**Задача:** Провести рефакторинг кодовой базы для отделения логики взаимодействия с API языковой модели от основной бизнес-логики агента.

**Цель:** Создать архитектуру, которая позволит в будущем легко добавлять поддержку различных LLM (например, Google Gemini), реализуя для каждой свой "провайдер" без изменения основного кода агента.

---

## Шаг 1: Создание интерфейса `LLMProvider`

Это ядро нашей новой архитектуры.

1.  **Создать новый заголовочный файл `src/LLMProvider.h`**.
2.  В этом файле определить абстрактный базовый класс (интерфейс) `LLMProvider`.
3.  Добавить в него следующие чисто виртуальные (абстрактные) методы:
    ```cpp
    #pragma once

    #include <string>
    #include <vector>
    #include <functional>
    #include <nlohmann/json.hpp>
    #include "AssistantRole.h" // Для структуры AssistantResponse

    class LLMProvider {
    public:
        virtual ~LLMProvider() = default;

        virtual AssistantResponse processChat(
            const nlohmann::json& messages,
            const nlohmann::json& tools,
            const std::function<void(const std::string&)>& send_thought
        ) = 0;

        virtual nlohmann::json generatePlan(const std::string& user_query) = 0;

        virtual std::vector<float> createEmbedding(const std::string& text) = 0;
    };
    ```

## Шаг 2: Реализация `OpenAIProvider`

Этот класс будет содержать всю существующую логику для работы с OpenAI-совместимым API.

1.  **Создать новый заголовочный файл `src/OpenAIProvider.h`**.
    *   Определить класс `OpenAIProvider`, который публично наследуется от `LLMProvider`.
    *   Он должен содержать приватные члены: `httplib::Client`, `const Config&`.
    *   Конструктор должен принимать `const Config&`.

2.  **Создать новый файл `src/OpenAIProvider.cpp`**.
    *   Реализовать все три виртуальных метода из `LLMProvider`.
    *   **Перенести** существующую логику HTTP-запросов (`cli.Post(...)`) и обработки ответов из `AssistantRole.cpp` (из методов `processQuery` и `generatePlan`) в соответствующие методы `OpenAIProvider`.
    *   **Перенести** существующую логику HTTP-запросов из `EmbeddingClient.cpp` в метод `OpenAIProvider::createEmbedding`.

## Шаг 3: Рефакторинг `AssistantRole`

Теперь `AssistantRole` будет делегировать сетевые вызовы провайдеру.

1.  **Изменить `src/AssistantRole.h`**:
    *   Удалить `httplib::Client cli;`.
    *   Добавить `std::shared_ptr<LLMProvider> llmProvider;`.
    *   Изменить конструктор, чтобы он принимал `std::shared_ptr<LLMProvider> llmProvider` вместо `const Config&`.

2.  **Изменить `src/AssistantRole.cpp`**:
    *   Обновить конструктор для инициализации `llmProvider`.
    *   В методах `processQuery` и `generatePlan` **заменить** прямые вызовы `cli.Post(...)` на вызовы `llmProvider->processChat(...)` и `llmProvider->generatePlan(...)`.
    *   **Важно:** Логика формирования `messages`, обработки `tool_calls` и циклов остается в `AssistantRole`, так как это бизнес-логика агента, а не деталь API.
    *   Удалить закомментированные методы `generateProjectSummaryGreeting` и `generateChunkSummary`, так как их функциональность теперь будет частью провайдера.

## Шаг 4: Рефакторинг `EmbeddingClient`

Аналогично `AssistantRole`, `EmbeddingClient` будет использовать провайдер.

1.  **Изменить `src/EmbeddingClient.h`**:
    *   Удалить `httplib::Client cli;`.
    *   Добавить `std::shared_ptr<LLMProvider> llmProvider;`.
    *   Изменить конструктор, чтобы он принимал `std::shared_ptr<LLMProvider>`.

2.  **Изменить `src/EmbeddingClient.cpp`**:
    *   Обновить конструктор для инициализации `llmProvider`.
    *   В методе `createEmbedding` **заменить** вызов `cli.Post(...)` на `llmProvider->createEmbedding(...)`.

## Шаг 5: Обновление точек входа (Инъекция зависимостей)

Здесь мы "склеим" все вместе.

1.  **Изменить `src/ContextIndexer.h` и `src/ContextIndexer.cpp`**:
    *   Изменить конструктор `ContextIndexer`, чтобы он принимал `std::shared_ptr<LLMProvider> llmProvider`.
    *   При создании `embeddingClient` внутри конструктора `ContextIndexer` передавать ему этот `llmProvider`.

2.  **Изменить `src/ApiHandlers.h` и `src/ApiHandlers.cpp`**:
    *   Изменить конструктор `ApiHandlers`, чтобы он принимал `std::shared_ptr<LLMProvider>`.
    *   При создании `assistant` внутри конструктора `ApiHandlers` передавать ему этот `llmProvider`.

3.  **Изменить `src/main.cpp`**:
    *   В функции `main` создать единственный экземпляр провайдера: `auto llmProvider = std::make_shared<OpenAIProvider>(config);`.
    *   Передать этот `llmProvider` в конструкторы `ContextIndexer` и `ApiHandlers` при их создании.

## Шаг 6: Обновление системы сборки

1.  **Изменить `CMakeLists.txt`**:
    *   Добавить `src/OpenAIProvider.cpp` в список исходников для `add_executable(Agent ...)`.
    *   Убедиться, что `src` добавлена в `target_include_directories`, чтобы новые заголовочные файлы были найдены.

---

После выполнения этих шагов проект будет иметь гибкую архитектуру, готовую к добавлению новых LLM-провайдеров.