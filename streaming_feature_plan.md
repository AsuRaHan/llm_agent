# План реализации потоковой передачи LLM ответов (Streaming LLM Responses)

## 1. Обзор

Цель: Реализовать потоковую передачу ответов от Large Language Model (LLM) на клиентскую часть (frontend) по принципу "токен за токеном". Это позволит пользователям видеть процесс генерации ответа в реальном времени, улучшая интерактивность.

Текущая ситуация:
*   Backend (C++) взаимодействует с LLM (предположительно, OpenAI-совместимый API) через `LLMProvider`.
*   Ответы от LLM в настоящее время получаются целиком, а затем отправляются на клиент.
*   Существует callback `send_thought`, который используется для отправки высокоуровневых сообщений о действиях агента (например, "Выполняю инструмент X..."). Этот механизм будет переиспользован для передачи потоковых токенов, но с новым типом сообщения.

## 2. Детальный план действий

### 2.1. Изменения в `src/OpenAIProvider.cpp` (LLM API Interaction)

Это ядро изменений для включения стриминга.

*   **Добавление параметра `stream` в запрос:**
    *   В функции `OpenAIProvider::processChat` необходимо модифицировать JSON-тело запроса к `/v1/chat/completions`.
    *   Добавить поле `"stream": true` к `body` запроса.
*   **Использование `httplib::Client::Post` с `content_receiver`:**
    *   Заменить текущий вызов `cli.Post` на версию, которая принимает лямбда-функцию `content_receiver`. Эта лямбда будет вызываться `httplib` для каждой части потокового ответа.
    *   **Реализация `content_receiver`:**
        *   Парсинг Server-Sent Events (SSE) формата: Ответ от OpenAI API при стриминге приходит в формате SSE, где каждая строка начинается с `data: `.
        *   Обработка `data: [DONE]` сообщения для определения конца стрима.
        *   Извлечение JSON-содержимого из каждой строки `data:`.
        *   Анализ JSON: каждая часть (`delta`) содержит либо текстовый токен (`delta.content`), либо части вызова инструмента (`delta.tool_calls`).
        *   Для текстовых токенов: Вызвать новую callback-функцию `send_stream_chunk` (которая будет передана в `processChat`) с полученным токеном.
        *   Для `tool_calls`: необходимо будет собрать все `delta.tool_calls` части воедино, чтобы получить полную спецификацию вызова инструмента, когда стрим завершится.
        *   Накопление полного ответа: `OpenAIProvider` должен собрать полный ответ от LLM (текст + вызовы инструментов) для возврата в `AssistantRole` после завершения стрима.
*   **Обработка ошибок и таймаутов:** Убедиться, что логика обработки ошибок и повторных попыток (retry logic) корректно работает со стримингом.
*   **`AssistantResponse` для стриминга:** Модифицировать `AssistantResponse` так, чтобы он мог возвращать собранный полный `llm_response` после завершения стрима, включая потенциальные `tool_calls` и признак `is_final`.

### 2.2. Изменения в `src/LLMProvider.h` (Interface Definition)

*   **Обновление сигнатуры `processChat`:**
    *   Добавить новый параметр `std::function<void(const std::string&)>& send_stream_chunk` к функции `processChat`.
    *   `AssistantResponse processChat(const nlohmann::json& messages, const nlohmann::json& tools, const std::function<void(const std::string&)>& send_thought, const std::function<void(const std::string&)>& send_stream_chunk) override;`

### 2.3. Изменения в `src/AssistantRole.h` и `src/AssistantRole.cpp` (Business Logic)

*   **Обновление сигнатуры `processQuery`:**
    *   Добавить новый параметр `const std::function<void(const std::string&)>& send_stream_chunk` к функции `processQuery`.
    *   `AssistantResponse processQuery(..., const std::function<void(const std::string&)>& send_thought, const std::function<void(const std::string&)>& send_stream_chunk);`
*   **Передача callback'а:**
    *   Внутри `AssistantRole::processQuery`, при вызове `llmProvider->processChat`, передавать в него новый `send_stream_chunk` callback.
*   **Обработка `AssistantResponse`:**
    *   Так как при стриминге текстовый ответ уже отправляется клиенту, `AssistantRole` не должен отправлять его снова через `query_response`.
    *   Логика обработки `AssistantResponse` должна сосредоточиться на `tool_calls` или других метаданных, а не на тексте ответа.

### 2.4. Изменения в `src/WebSocketServer.cpp` (Frontend Communication)

*   **Создание `send_stream_chunk` callback'а:**
    *   В `WebSocketServer::processAgentLogic` (и, возможно, в `processPlanGeneration`, если планы тоже будут стримиться, хотя это менее приоритетно), определить лямбда-функцию для `send_stream_chunk`.
    *   Эта лямбда будет вызывать `sendMessage` с новым типом сообщения.
    *   `auto send_stream_chunk = [this, ws_handle](const std::string& token) { sendMessage(ws_handle, {{"type", "llm_token"}, {"data", {{"token", token}}}}); };`
    *   `auto send_thought_hl = [this, ws_handle](const std::string& thought) { sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", thought}}}}); };`
*   **Передача callback'ов в `AssistantRole::processQuery`:**
    *   При вызове `assistant.processQuery`, передать оба callback'а: `send_thought_hl` (для высокоуровневых мыслей) и `send_stream_chunk` (для токенов).
*   **Завершение стрима:**
    *   После того как `assistant.processQuery` завершится и вернет `AssistantResponse` (что означает конец LLM-генерации), `WebSocketServer` должен отправить специальное сообщение клиенту, сигнализирующее о завершении стрима.
    *   `sendMessage(ws_handle, {{"type", "stream_end"}});`
    *   Это сообщение должно быть отправлено перед любой дальнейшей обработкой (например, перед запросом подтверждения инструмента).
*   **Корректировка отправки финального ответа:**
    *   Удалить отправку `query_response` с полем `answer`, так как текст уже был отправлен потоком.
    *   Сохранить логику для `requires_confirmation`, `pending_tool_call` и `step_failed`.

## 3. Новые типы сообщений для WebSocket

*   **`llm_token`:**
    *   `{"type": "llm_token", "data": {"token": "текст_токена"}}`
    *   Отправляется клиенту для каждого полученного токена.
*   **`stream_end`:**
    *   `{"type": "stream_end"}`
    *   Отправляется клиенту, когда LLM завершает генерацию ответа (т.е. получен `[DONE]` от API).

## 4. Что будет с `send_thought`?

Существующий `send_thought` callback будет переименован (например, в `send_thought_hl` для "high-level thoughts") и будет по-прежнему использоваться для отправки сообщений о выполнении инструментов или других внутренних процессах агента, которые не являются непосредственно потоком токенов LLM. Клиент будет отображать их отдельно, возможно, в виде системных уведомлений или статусов.

## 5. Изменения на стороне Frontend (Conceptual - will implement later)

*   **Обработка `llm_token`:**
    *   JavaScript-код на клиенте будет слушать сообщения с `type: "llm_token"`.
    *   Для каждого такого сообщения извлекать `data.token` и дописывать его в элемент DOM, отвечающий за отображение ответа.
*   **Обработка `stream_end`:**
    *   Клиент будет слушать сообщения с `type: "stream_end"`.
    *   Это будет сигналом, что текстовый поток завершен, и можно, например, убрать индикатор загрузки токенов.
*   **Совместимость с `agent_thought`:**
    *   Убедиться, что `agent_thought` сообщения (для статусов выполнения инструментов) отображаются корректно и не конфликтуют с потоковым выводом.

## 6. Последовательность реализации

1.  Обновить `LLMProvider.h` с новой сигнатурой `processChat`.
2.  Обновить `AssistantRole.h` с новой сигнатурой `processQuery`.
3.  Изменить `OpenAIProvider.cpp` для реализации стриминга и использования `send_stream_chunk`.
4.  Изменить `AssistantRole.cpp` для передачи нового callback'а в `llmProvider->processChat`.
5.  Изменить `WebSocketServer.cpp` для создания `send_stream_chunk` лямбды, передачи ее в `assistant.processQuery` и добавления `stream_end` сообщения.
6.  (Позднее) создать аналог `frontend/js/chat.js` для обработки новых типов сообщений.