# Инструкция по внедрению потоковой передачи LLM ответов (Streaming LLM Responses)

## 📋 Обзор изменений

Эта инструкция описывает пошаговое внедрение потоковой передачи ответов от LLM на клиентскую часть. Реализация позволит пользователям видеть процесс генерации ответа в реальном времени ("токен за токеном").

---

## 🗂️ Архитектура изменений

```
┌─────────────────────────────────────────────────────────────────┐
│                    Frontend (WebSocket)                          │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │  Обработка: "llm_token" → дописывание токена в DOM          │ │
│  │  Обработка: "stream_end" → завершение потока                │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              ↓ WebSocket
┌─────────────────────────────────────────────────────────────────┐
│                    Backend (C++)                                 │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │  WebSocketServer.cpp:                                      │ │
│  │    - send_stream_chunk callback                            │ │
│  │    - Отправка "llm_token" сообщений                        │ │
│  │    - Отправка "stream_end" сообщения                       │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                              ↓ processQuery                      │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │  AssistantRole.cpp:                                        │ │
│  │    - Передача send_stream_chunk в LLMProvider              │ │
│  │    - Обработка AssistantResponse без текста ответа         │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                              ↓ processChat                       │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │  OpenAIProvider.cpp:                                       │ │
│  │    - stream: true в запросе                                │ │
│  │    - httplib::Post с content_receiver                      │ │
│  │    - Парсинг SSE (data: ...)                               │ │
│  │    - Вызов send_stream_chunk для каждого токена            │ │
│  │    - Сбор полного ответа для AssistantResponse             │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📝 Пошаговая инструкция

### Шаг 1: Обновление интерфейса `LLMProvider.h`

**Файл:** `src/LLMProvider.h`

**Цель:** Добавить callback для потоковой передачи токенов в сигнатуру `processChat`.

```cpp
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include "AssistantResponse.h" // Для структуры AssistantResponse



class LLMProvider {
public:
    virtual ~LLMProvider() = default;

    struct ServerProperties {
        std::string name;
        std::string version;
        // Add other relevant properties
    };

    // ИЗМЕНЕНИЕ: Добавлен параметр send_stream_chunk
    virtual AssistantResponse processChat(
        const nlohmann::json& messages,
        const nlohmann::json& tools,
        const std::function<void(const std::string&)>& send_thought,
        const std::function<void(const std::string&)>& send_stream_chunk  // ← НОВЫЙ ПАРАМЕТР
    ) = 0;

    virtual nlohmann::json generatePlan(const std::string& user_query) = 0;

    virtual std::vector<float> createEmbedding(const std::string& text) = 0;

    virtual std::string generateChunkSummary(const std::string& code_chunk, const std::string& chunk_name) = 0;

    virtual std::optional<ServerProperties> fetchServerProperties() const = 0;
};
```

---

### Шаг 2: Обновление интерфейса `AssistantRole.h`

**Файл:** `src/AssistantRole.h`

**Цель:** Добавить callback для потоковой передачи токенов в сигнатуру `processQuery`.

```cpp
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

#include "ContextIndexer.h"
#include "ToolManager.h"
#include "LLMProvider.h"
#include "ContextIndexerHelper/Searcher.h"

#include "AssistantResponse.h"
#include "Config.h"

class AssistantRole {
public:
    explicit AssistantRole(std::shared_ptr<LLMProvider> llmProvider, const Config& config);
    ~AssistantRole();

    // ИЗМЕНЕНИЕ: Добавлен параметр send_stream_chunk
    AssistantResponse processQuery(
        const std::string& userQuery,
        const std::vector<SearchResult>& initialContext,
        ContextIndexer& indexer,
        const nlohmann::json& continuation_history,
        const std::function<void(const std::string&)>& send_thought,
        const std::function<void(const std::string&)>& send_stream_chunk  // ← НОВЫЙ ПАРАМЕТР
    );

    nlohmann::json generatePlan(const std::string& user_query);

private:
    nlohmann::json parsePlanFromMarkdown(const std::string& text);

    const Config& config;
    std::shared_ptr<LLMProvider> llmProvider;
    std::unique_ptr<ToolManager> toolManager;
};
```

---

### Шаг 3: Реализация стриминга в `OpenAIProvider.cpp`

**Файл:** `src/OpenAIProvider.cpp`

**Цель:** Реализовать потоковую передачу через `httplib::Client::Post` с `content_receiver`.

```cpp
#include "OpenAIProvider.h"
#include "Logger.h"
#include <thread>
#include <chrono>

using json = nlohmann::json;

OpenAIProvider::OpenAIProvider(const Config& config)
    : config(config), cli(config.server_host, config.server_port)
{
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(config.chat_completion_timeout_sec, 0);
}

AssistantResponse OpenAIProvider::processChat(
    const nlohmann::json& messages,
    const nlohmann::json& tools,
    const std::function<void(const std::string&)>& send_thought,
    const std::function<void(const std::string&)>& send_stream_chunk  // ← НОВЫЙ ПАРАМЕТР
) {
    json body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        {"tools", tools},
        {"tool_choice", "auto"},
        {"temperature", 0.1},
        {"stream": true}  // ← ДОБАВИТЬ СТРИМИНГ
    };

    // ИЗМЕНЕНИЕ: Переменные для агрегации полного ответа
    std::string full_content;
    json tool_calls = json::array();
    // Вспомогательные переменные для сборки одного tool_call из нескольких чанков
    std::string current_tool_id;
    std::string current_tool_name;
    std::string current_tool_args;

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    // ИЗМЕНЕНИЕ: Использовать Post с content_receiver для стриминга и агрегации
    auto res = cli.Post("/v1/chat/completions", headers, body_str, "application/json",
        [&](const char *data, size_t data_length) {
            std::string chunk(data, data_length);
            std::istringstream stream(chunk);
            std::string line;

            while (std::getline(stream, line)) {
                if (line.rfind("data: ", 0) == 0) {
                    std::string data_str = line.substr(6);
                    if (data_str.empty() || data_str == "[DONE]") {
                        continue;
                    }
                    try {
                        json delta_json = json::parse(data_str);
                        if (delta_json.contains("choices") && !delta_json["choices"].empty()) {
                            const auto& delta = delta_json["choices"][0]["delta"];

                            // 1. Агрегируем и отправляем текстовые токены
                            if (delta.contains("content") && delta["content"].is_string()) {
                                std::string token = delta["content"].get<std::string>();
                                if (!token.empty()) {
                                    full_content += token;
                                    if (send_stream_chunk) {
                                        send_stream_chunk(token);
                                    }
                                }
                            }

                            // 2. Агрегируем tool_calls
                            if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
                                for (const auto& tool_call_delta : delta["tool_calls"]) {
                                    // Если пришел ID, значит начался новый tool_call
                                    if (tool_call_delta.contains("id")) {
                                        // Завершаем предыдущий tool_call, если он был
                                        if (!current_tool_id.empty()) {
                                            tool_calls.push_back({
                                                {"id", current_tool_id},
                                                {"type", "function"},
                                                {"function", {
                                                    {"name", current_tool_name},
                                                    {"arguments", current_tool_args}
                                                }}
                                            });
                                        }
                                        // Начинаем новый
                                        current_tool_id = tool_call_delta["id"];
                                        current_tool_name = "";
                                        current_tool_args = "";
                                    }
                                    if (tool_call_delta.contains("function")) {
                                        if (tool_call_delta["function"].contains("name")) {
                                            current_tool_name += tool_call_delta["function"]["name"].get<std::string>();
                                        }
                                        if (tool_call_delta["function"].contains("arguments")) {
                                            current_tool_args += tool_call_delta["function"]["arguments"].get<std::string>();
                                        }
                                    }
                                }
                            }
                        }
                    } catch (const json::exception& e) {
                        SPDLOG_WARN("Ошибка парсинга чанка стрима: {}", e.what());
                    }
                }
            }
            return true; // Продолжаем получать данные
        }
    );

    // Проверка результата запроса
    if (!cli.is_valid() || !res || res->status != 200) {
        SPDLOG_ERROR("Ошибка запроса к LLM API. Статус: {}", res ? res->status : -1);
        return { .step_failed = true, .error_message = "Ошибка запроса к LLM API." };
    }

    // ИЗМЕНЕНИЕ: После завершения стрима, собираем финальный ответ
    // Завершаем последний tool_call, если он был
    if (!current_tool_id.empty()) {
        tool_calls.push_back({
            {"id", current_tool_id},
            {"type", "function"},
            {"function", {
                {"name", current_tool_name},
                {"arguments", current_tool_args}
            }}
        });
    }

    // Собираем финальный JSON, имитирующий не-потоковый ответ
    json final_message = { {"role", "assistant"} };
    if (!full_content.empty()) {
        final_message["content"] = full_content;
    }
    if (!tool_calls.empty()) {
        final_message["tool_calls"] = tool_calls;
    }

    json final_response_json = {
        {"choices", json::array({{ "message", final_message }})}
    };

    return { .llm_response = final_response_json, .conversation_history = messages };
}
```

---

### Шаг 4: Обновление реализации `AssistantRole.cpp`

**Файл:** `src/AssistantRole.cpp`

**Цель:** Передавать новый callback в `llmProvider->processChat` и обрабатывать `AssistantResponse` без текста ответа.

```cpp
AssistantResponse AssistantRole::processQuery(
    const std::string& userQuery,
    const std::vector<SearchResult>& initialContext,
    ContextIndexer& indexer,
    const nlohmann::json& continuation_history,
    const std::function<void(const std::string&)>& send_thought,
    const std::function<void(const std::string&)>& send_stream_chunk  // ← НОВЫЙ ПАРАМЕТР
) {
    // 1. Initialize message history
    json messages = json::array();
    bool is_continuation_after_confirmation = !continuation_history.empty() && userQuery.empty();

    if (is_continuation_after_confirmation) {
        // This is a continuation call after a dangerous tool was confirmed.
        // The tool call is in the history. We need to execute it before calling the LLM again.
        messages = continuation_history;
        const auto& last_message = messages.back();

        if (last_message.value("role", "") == "assistant" && last_message.contains("tool_calls")) {
            SPDLOG_INFO("Продолжение после подтверждения. Выполнение отложенного инструмента...");
            const auto& tool_calls = last_message["tool_calls"];

            for (const auto& call : tool_calls) {
                const auto& function_call = call["function"];
                std::string tool_name = function_call["name"];
                json tool_args;
                const std::string& tool_id = call["id"];

                try {
                    // Handle cases where arguments are a stringified JSON or a direct JSON object
                    if (function_call["arguments"].is_string()) {
                        tool_args = json::parse(function_call["arguments"].get<std::string>());
                    } else {
                        tool_args = function_call["arguments"];
                    }
        } catch (const json::exception& e) { // Ошибка парсинга аргументов инструмента
                    SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
                    std::string error_content = "{\"error\": \"Invalid JSON in arguments: " + std::string(e.what()) + "\"}";
                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_id},
                        {"content", error_content}
                    });
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = "Не удалось разобрать аргументы для инструмента '" + tool_name + "': " + e.what(),
                .recovery_options = {"retry", "skip"}
            };
                }

                // Execute the tool
                if (send_thought) {
                    send_thought("Выполняю подтвержденный инструмент: " + tool_name + "...");
                }
                std::string result = toolManager->executeTool(tool_name, tool_args, &indexer);

        // Проверяем, вернул ли инструмент ошибку в формате {"error": "..."}
        try {
            json result_json = json::parse(result);
            if (result_json.contains("error")) {
                SPDLOG_ERROR("Инструмент '{}' завершился с ошибкой: {}", tool_name, result_json["error"].get<std::string>());
                messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", result}});
                return {
                    .text = "",
                    .is_final = true,
                    .conversation_history = messages,
                    .step_failed = true,
                    .error_message = "Инструмент '" + tool_name + "' завершился с ошибкой: " + result_json["error"].get<std::string>(),
                    .recovery_options = {"retry", "skip"}
                };
            }
        } catch (const json::exception& e) {
            // Если результат не является JSON или не содержит "error", это не ошибка инструмента в данном формате.
            // Продолжаем обработку как обычный результат.
        }

                // Add the tool's result to the history
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_id},
                    {"content", result}
                });
            }
        }
        // If it's a continuation, the user query and context are already in the history.
        // We don't add them again.
    } else {
        // This is a new query or a simple follow-up.
        json messages_for_llm = json::array();

        // 1. Find the original system prompt content, if it exists.
        std::string system_prompt_content;
        auto system_msg_it = std::find_if(continuation_history.begin(), continuation_history.end(), [](const json& msg){
            return msg.value("role", "") == "system";
        });

        if (system_msg_it != continuation_history.end()) {
            // Use existing system prompt
            system_prompt_content = system_msg_it->value("content", "");
        } else {
            // Create a new one if it's the start of a conversation
            system_prompt_content = "Ты — эксперт-программист и AI-ассистент. Твоя задача — отвечать на вопросы пользователя о кодовой базе.\n"
                                    "У тебя есть лимит на вызов инструментов: " + std::to_string(config.max_tool_calls) + " раз на один запрос.\n\n"
                                    "Твой план действий:\n"
                                    "1.  **Проанализируй контекст.** Тебе предоставлен релевантный контекст, найденный по вопросу пользователя. Внимательно изучи его.\n"
                                    "2.  **Оцени достаточность контекста.** Если предоставленные фрагменты кода полностью отвечают на вопрос, сформируй ответ на их основе.\n"
                                    "3.  **Используй инструменты, если необходимо.** Если контекст неполный, или тебе нужно увидеть файл целиком, чтобы понять общую картину, используй инструменты:\n"
                                    "4.  **Думай по шагам.** После каждого шага (вызова инструмента) анализируй полученную информацию и решай, что делать дальше, пока не соберешь достаточно данных для исчерпывающего ответа.\n"
                                    "5.  **Дай точный ответ.** В конце, ссылаясь на собранную информацию, дай пользователю точный и подробный ответ.";
        }

        // 2. Append the new RAG context to the system prompt content
        if (!initialContext.empty()) {
            std::stringstream context_ss;
            context_ss << "\n\n---\n# Дополнительный релевантный контекст для текущего запроса:\n";
            for (const auto& result : initialContext) {
                context_ss << "--- ИЗ ФАЙЛА: " << result.filePath << " ---\n"
                           << "```\n"
                           << result.chunkText << "\n"
                           << "```\n\n";
            }
            system_prompt_content += context_ss.str();
        }

        // 3. Build the final message list for the LLM
        messages_for_llm.push_back({{"role", "system"}, {"content", system_prompt_content}});
        for (const auto& msg : continuation_history) {
            if (msg.value("role", "") != "system") {
                messages_for_llm.push_back(msg);
            }
        }
        messages = messages_for_llm;
    }

    // Count tool calls already in history to respect the limit across continuations
    int tool_calls_made = 0;
    for(const auto& msg : messages) {
        if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
            tool_calls_made += msg["tool_calls"].size();
        }
    }

    for (int i = tool_calls_made; i < config.max_tool_calls; ++i) {
        SPDLOG_INFO("Итерация {}/{} цикла обработки запроса...", i + 1, config.max_tool_calls);
        // 2. Prepare request for LLM
        json body = {
            {"messages", messages},
            {"model", config.chat_model_name},
            {"tools", toolManager->getToolsSpecification()},
            {"tool_choice", "auto"},
            {"temperature", 0.1}
        };

        httplib::Headers headers;
        if (!config.api_key.empty()) {
            headers.emplace("Authorization", "Bearer " + config.api_key);
        }
        std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

        // 3. Call LLM - ИЗМЕНЕНИЕ: передаем send_stream_chunk
        AssistantResponse llm_response = llmProvider->processChat(
            messages, 
            toolManager->getToolsSpecification(), 
            send_thought,
            send_stream_chunk  // ← ПЕРЕДАЕМ НОВЫЙ CALLBACK
        );

        if (llm_response.step_failed) {
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = llm_response.error_message,
                .recovery_options = {"retry"}
            };
        }

        json response_json = llm_response.llm_response;

        if (!response_json.contains("choices") || response_json["choices"].empty()) {
            SPDLOG_ERROR("Ответ LLM не содержит 'choices'. Тело: {}", response_json.dump(2));
            return {
                .text = "",
                .is_final = true,
                .conversation_history = messages,
                .step_failed = true,
                .error_message = "Получен некорректный ответ от модели (отсутствует поле 'choices').",
                .recovery_options = {"retry"}
            };
        }

        const auto& choice = response_json["choices"][0];
        const auto& message = choice["message"];

        // ИЗМЕНЕНИЕ: При стриминге текст уже отправлен клиенту, поэтому не добавляем его в историю
        // Здесь обрабатываем только tool_calls и метаданные
        
        // Case 1: LLM provides a direct answer (редко при стриминге)
        if (message.contains("content") && message["content"].is_string() && !message["content"].get<std::string>().empty()) {
            SPDLOG_INFO("LLM предоставил прямой ответ. Завершение цикла.");
            std::string final_text = message["content"];

            // Add the final assistant message to the history
            messages.push_back({
                {"role", "assistant"},
                {"content", final_text}});

            // Simple heuristic to detect a follow-up question
            bool is_final = true;
            if (!final_text.empty() && final_text.back() == '?') {
                is_final = false;
            }
            
            return {
                .text = final_text, 
                .is_final = is_final, 
                .conversation_history = messages
            };
        }

        // Case 2: LLM wants to call tools
        if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
            SPDLOG_INFO("LLM запросил вызов инструментов.");
            // Add the assistant's response (with tool calls) to the history
            messages.push_back(message);

            const auto& tool_calls = message["tool_calls"];
            for (const auto& call : tool_calls) {
                const auto& function_call = call["function"];
                std::string tool_name = function_call["name"];
                json tool_args;
                const std::string& tool_id = call["id"];

                // --- DANGEROUS TOOL CHECK ---
                const auto& dangerous_tools = config.dangerous_tools;
                if (std::find(dangerous_tools.begin(), dangerous_tools.end(), tool_name) != dangerous_tools.end()) {
                    SPDLOG_WARN("Обнаружен опасный инструмент '{}', требующий подтверждения.", tool_name);

                    std::string prompt = "Я собираюсь использовать инструмент '" + tool_name + "'. Разрешить выполнение?";
                    
                    return {
                        .text = prompt,
                        .is_final = false,
                        .conversation_history = messages,
                        .requires_confirmation = true,
                        .pending_tool_call = call,
                        .plan_completed = true
                    };
                }

                try {
                    // Handle cases where arguments are a stringified JSON or a direct JSON object
                    if (function_call["arguments"].is_string()) {
                        tool_args = json::parse(function_call["arguments"].get<std::string>());
                    } else {
                        tool_args = function_call["arguments"];
                    }
                } catch (const json::exception& e) { // Ошибка парсинга аргументов инструмента
                    SPDLOG_ERROR("Failed to parse tool arguments for '{}': {}", tool_name, e.what());
                    std::string error_content = "{\"error\": \"Invalid JSON in arguments: " + std::string(e.what()) + "\"}";
                    messages.push_back({
                        {"role", "tool"},
                        {"tool_call_id", tool_id},
                        {"content", error_content}
                    });
                    return {
                        .text = "",
                        .is_final = true,
                        .conversation_history = messages,
                        .step_failed = true,
                        .error_message = "Не удалось разобрать аргументы для инструмента '" + tool_name + "': " + e.what(),
                        .recovery_options = {"retry", "skip"}
                    };
                }

                // Execute the tool
                if (send_thought) {
                    send_thought("Выполняю инструмент: " + tool_name + "...");
                }
                std::string result = toolManager->executeTool(tool_name, tool_args, &indexer);

                // Проверяем, вернул ли инструмент ошибку в формате {"error": "..."}
                try {
                    json result_json = json::parse(result);
                    if (result_json.contains("error")) {
                        SPDLOG_ERROR("Инструмент '{}' завершился с ошибкой: {}", tool_name, result_json["error"].get<std::string>());
                        messages.push_back({{"role", "tool"}, {"tool_call_id", tool_id}, {"content", result}});
                        return {
                            .text = "",
                            .is_final = true,
                            .conversation_history = messages,
                            .step_failed = true,
                            .error_message = "Инструмент '" + tool_name + "' завершился с ошибкой: " + result_json["error"].get<std::string>(),
                            .recovery_options = {"retry", "skip"}
                        };
                    }
                } catch (const json::exception& e) {
                    // Если результат не является JSON или не содержит "error", это не ошибка инструмента в данном формате.
                    // Продолжаем обработку как обычный результат.
                }

                // Add the tool's result to the history
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_id},
                    {"content", result}
                });
            }

            // Inform the model about remaining tool calls by appending to the last tool message
            int remaining_calls = config.max_tool_calls - (i + 1); // i is 0-indexed
            if (remaining_calls > 0) {
                if (!messages.empty() && messages.back()["role"] == "tool") {
                    std::string current_content = messages.back()["content"];
                    messages.back()["content"] = current_content + "\n\n[SYSTEM_NOTE]: Инструменты выполнены. У тебя осталось " + std::to_string(remaining_calls) + " вызовов.";
                }
            }
            // Continue to the next iteration of the loop
            continue;
        }

        // Fallback if the response is unexpected (e.g. content is null)
        SPDLOG_ERROR("Неожиданный формат ответа от LLM: {}", choice.dump(2));
        return {
            .text = "",
            .is_final = true,
            .conversation_history = messages,
            .step_failed = true,
            .error_message = "Получен неожиданный формат ответа от модели (не текст и не вызов инструмента).",
            .recovery_options = {"retry"}
        };
    }

    SPDLOG_WARN("Превышено максимальное количество вызовов инструментов ({}). Запрос финального ответа.", config.max_tool_calls);
    
    // Ask the model to summarize and give a final answer based on the conversation so far.
    messages.push_back({
        {"role", "user"}, // Use 'user' to force a response
        {"content", "Ты достиг максимального количества вызовов инструментов. Предоставь пользователю окончательный ответ, основываясь на уже собранной информации."}
    });

    json final_body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        // No "tools" or "tool_choice" here to force a direct answer
        {"temperature", 0.1}
    };

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    // Make one last call to the LLM
    auto final_llm_response = llmProvider->processChat(messages, json::array(), send_thought, send_stream_chunk);

    if (!final_llm_response.step_failed && final_llm_response.llm_response.contains("choices") && !final_llm_response.llm_response["choices"].empty()) {
        std::string final_text = final_llm_response.llm_response["choices"][0]["message"]["content"];
        messages.push_back({{"role", "assistant"}, {"content", final_text}});
        return {
            .text = final_text, 
            .is_final = true, 
            .conversation_history = messages
        };
    }

    return {
        .text = "",
        .is_final = true,
        .conversation_history = messages,
        .step_failed = true,
        .error_message = "Превышен лимит вызовов инструментов (" + std::to_string(config.max_tool_calls) + ").",
        .recovery_options = {"re-plan"}
    };
}
```

---

### Шаг 5: Обновление `WebSocketServer.cpp`

**Файл:** `src/WebSocketServer.cpp`

**Цель:** Создать callback для потоковой передачи и передавать его в `assistant.processQuery`.

```cpp
void WebSocketServer::processAgentLogic(std::shared_ptr<UserSession> session, const std::string& queryText, std::shared_ptr<SafeWsHandle> ws_handle) {
    std::string sessionId = session->id;
    try {
        // ИЗМЕНЕНИЕ: Создаем callback для потоковой передачи токенов
        auto send_stream_chunk = [this, ws_handle](const std::string& token) {
            sendMessage(ws_handle, {
                {"type", "llm_token"}, 
                {"data", {{"token", token}}}
            });
        };
        
        // send_thought остается для высокоуровневых сообщений (статусы инструментов)
        auto send_thought = [this, ws_handle](const std::string& thought) {
            sendMessage(ws_handle, {{"type", "agent_thought"}, {"data", {{"message", thought}}}});
        };
        
        // --- ЛОГИКА ВЫПОЛНЕНИЯ ПЛАНА ---
        if (session->status == AgentStatus::EXECUTING_PLAN) {
            // Сбор RAG-контекста выполняется только один раз в самом начале плана

            // Итерируемся по шагам плана
            while (session->current_plan_step < (int)session->plan_steps.size()) {
                int current_step_idx = session->current_plan_step;
                std::string task = session->plan_steps[current_step_idx].get<std::string>();

                SPDLOG_INFO("Сессия {}: выполнение шага {}/{} плана: {}", sessionId, current_step_idx + 1, session->plan_steps.size(), task);
                sendMessage(ws_handle, {{"type", "plan_update"}, {"data", {{"current_step", current_step_idx}, {"steps", session->plan_steps}}}});

                // Для первого шага ищем RAG-контекст по оригинальному запросу. Для последующих - не ищем.
                std::vector<SearchResult> context;
                if (current_step_idx == 0) {
                    auto& searcher = indexer.getSearcher();
                    auto& fileIndex = indexer.getFileIndexer().getFileIndex();
                    context = searcher.findTopK(session->original_user_query, config.top_k_results, fileIndex);
                }

                // ВАЖНО: Мы пушим ноту шага в историю ТОЛЬКО если мы не продолжаем работу после подтверждения опасного инструмента.
                // Если мы вернулись из handleConfirmation, вызов инструмента уже сидит на вершине истории, и пушить туда системную ноту нельзя.
                if (queryText != "CONTINUE_AFTER_TOOL" && queryText != "RETRY_STEP") {
                    session->history.push_back({{"role", "user"}, {"content", "[SYSTEM_NOTE]: Текущий шаг плана: \"" + task + "\". Используй инструменты для его реализации. По окончании шага переходи к следующему."}});
                }

                // ИЗМЕНЕНИЕ: Передаем оба callback'а в assistant.processQuery
                AssistantResponse response = assistant.processQuery("", context, indexer, session->history, send_thought, send_stream_chunk);
                session->history = response.conversation_history;

                // Если шаг плана завершился с ошибкой, прерываем выполнение всего плана
                if (response.step_failed) {
                    SPDLOG_ERROR("Шаг плана для сессии {} завершился с ошибкой: {}", sessionId, response.error_message);
                    session->status = AgentStatus::AWAITING_ERROR_RECOVERY_DECISION; // Не меняем на IDLE, ждем решения
                    sendMessage(ws_handle, {
                        {"type", "plan_error"}, 
                        {"data", {
                            {"error_message", response.error_message}, 
                            {"recovery_options", response.recovery_options}
                        }}
                    });
                    return; // Полностью выходим из потока пула, ждем решения.
                }

                // Перехват ручного подтверждения опасного инструмента
                if (response.requires_confirmation) {
                    SPDLOG_INFO("Выполнение плана для сессии {} приостановлено. Ожидание подтверждения операции.", sessionId);
                    session->status = AgentStatus::AWAITING_CONFIRMATION;
                    session->pending_tool_call = response.pending_tool_call;
                    sendMessage(ws_handle, {{"type", "action_required"}, {"data", {{"message", response.text}, {"tool_call", response.pending_tool_call}}}});
                    return; // Полностью выходим из потока пула. Ждем клика в UI.
                }

                // Шаг успешно закрыт моделью, инкрементируем счетчик плана
                session->current_plan_step++;
                // Сбрасываем флаг продолжения для следующих шагов в цикле while
                const_cast<std::string&>(queryText) = ""; 
            }

            // Все шаги плана успешно исчерпаны. Запрашиваем финальный аналитический отчет.
            SPDLOG_INFO("Все задачи плана для сессии {} выполнены. Сборка итогового отчета...", sessionId);
            session->history.push_back({{"role", "user"}, {"content", "Все пункты плана успешно реализованы. Подведи итог проделанной работы, перечисли измененные компоненты и предоставь финальный ответ пользователю."}});
            
            AssistantResponse final_response = assistant.processQuery("", {}, indexer, session->history, send_thought, send_stream_chunk);
            
            setSessionIdle(session);
            // ИЗМЕНЕНИЕ: Не отправляем query_response с answer, так как текст уже был отправлен потоком
            // Сохраняем логику для requires_confirmation, pending_tool_call и step_failed
            if (final_response.requires_confirmation) {
                session->status = AgentStatus::AWAITING_CONFIRMATION;
                session->pending_tool_call = final_response.pending_tool_call;
                sendMessage(ws_handle, {{"type", "action_required"}, {"data", {{"message", final_response.text}, {"tool_call", final_response.pending_tool_call}}}});
            } else if (final_response.step_failed) {
                session->status = AgentStatus::AWAITING_ERROR_RECOVERY_DECISION;
                sendMessage(ws_handle, {
                    {"type", "plan_error"}, 
                    {"data", {
                        {"error_message", final_response.error_message}, 
                        {"recovery_options", final_response.recovery_options}
                    }}
                });
            } else {
                // ИЗМЕНЕНИЕ: Отправляем stream_end вместо query_response
                sendMessage(ws_handle, {{"type", "stream_end"}});
            }

        } else { 
            // --- СИНГЛ-ШОТ РЕЖИМ (ОБЫЧНЫЙ ДИАЛОГ БЕЗ ПЛАНА) ---
            std::vector<SearchResult> context;
            if (!queryText.empty()) {
                auto& searcher = indexer.getSearcher();
                auto& fileIndex = indexer.getFileIndexer().getFileIndex();
                context = searcher.findTopK(queryText, config.top_k_results, fileIndex);
            }

            // ИЗМЕНЕНИЕ: Передаем send_stream_chunk в assistant.processQuery
            AssistantResponse response = assistant.processQuery(queryText, context, indexer, session->history, send_thought, send_stream_chunk);
            // Сохраняем сессию СРАЗУ после того, как ассистент обновил историю
            sessionManager.saveSessions();

            session->history = response.conversation_history;

            if (response.requires_confirmation) {
                session->status = AgentStatus::AWAITING_CONFIRMATION;
                session->pending_tool_call = response.pending_tool_call;
                sendMessage(ws_handle, {{"type", "action_required"}, {"data", {{"message", response.text}, {"tool_call", response.pending_tool_call}}}});
            } else if (response.step_failed) {
                setSessionIdle(session);
                sendMessage(ws_handle, {
                    {"type", "plan_error"}, 
                    {"data", {
                        {"error_message", response.error_message}, 
                        {"recovery_options", response.recovery_options}
                    }}
                });
            } else {
                setSessionIdle(session);
                // Сохраняем сессию еще раз после сброса статуса в IDLE
                sessionManager.saveSessions();
                // ИЗМЕНЕНИЕ: Отправляем stream_end вместо query_response с answer
                sendMessage(ws_handle, {{"type", "stream_end"}});
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Ошибка при обработке логики агента для сессии {}: {}", sessionId, e.what());
        setSessionIdle(session);
        sendMessage(ws_handle, {
            {"type", "error"},
            {"data", {{"message", "Внутренняя ошибка сервера в фоновом потоке: " + std::string(e.what())}}}
        });
    }
}
```

---

## 🔄 Последовательность реализации

| Шаг | Файл | Изменение |
|-----|------|-----------|
| 1 | `src/LLMProvider.h` | Добавить `send_stream_chunk` в `processChat` |
| 2 | `src/AssistantRole.h` | Добавить `send_stream_chunk` в `processQuery` |
| 3 | `src/OpenAIProvider.cpp` | Реализовать стриминг через `content_receiver` |
| 4 | `src/AssistantRole.cpp` | Передавать callback в `llmProvider->processChat` |
| 5 | `src/WebSocketServer.cpp` | Создать callback и отправлять сообщения |

---

## 📡 Новые типы WebSocket сообщений

### `llm_token`
```json
{
  "type": "llm_token",
  "data": {
    "token": "текст_токена"
  }
}
```
Отправляется клиенту для каждого полученного токена LLM.

### `stream_end`
```json
{
  "type": "stream_end"
}
```
Отправляется клиенту, когда LLM завершает генерацию ответа (получен `[DONE]` от API).

---

## 🎯 Что будет с `send_thought`?

Существующий `send_thought` callback будет использоваться для отправки **высокоуровневых мыслей** (high-level thoughts) о выполнении инструментов или других внутренних процессах агента, которые не являются непосредственно потоком токенов LLM. Клиент будет отображать их отдельно, возможно, в виде системных уведомлений или статусов.

---

## 📝 Frontend (будет реализован отдельно)

```javascript
// Пример обработки на клиенте
const ws = new WebSocket('ws://localhost:8080');

ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    
    if (message.type === 'llm_token') {
        // Дописываем токен в элемент DOM
        const answerElement = document.getElementById('answer');
        answerElement.textContent += message.data.token;
    }
    
    if (message.type === 'stream_end') {
        // Убираем индикатор загрузки
        document.getElementById('loading').style.display = 'none';
    }
    
    if (message.type === 'agent_thought') {
        // Отображаем статус выполнения инструментов
        console.log('Agent thought:', message.data.message);
    }
};
```

---

## ✅ Проверка реализации

После внедрения проверьте:

1. ✅ Токены LLM отображаются в реальном времени на клиенте
2. ✅ Сообщения `agent_thought` отображаются отдельно от токенов
3. ✅ Сообщение `stream_end` отправляется после завершения генерации
4. ✅ Обработка ошибок и подтверждений инструментов работает корректно
5. ✅ История диалога сохраняется корректно

---

## 📌 Примечания

- При стриминге полный текст ответа не отправляется через `query_response`, так как он уже передается потоком
- Обработка `tool_calls` и подтверждений инструментов остается без изменений
- `send_thought` используется для высокоуровневых статусов (например, "Выполняю инструмент X...")
- `send_stream_chunk` используется для передачи каждого токена LLM клиенту
