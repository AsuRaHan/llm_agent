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
    const std::function<void(const std::string&)>& send_stream_chunk
) {
    json body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        {"temperature", 0.1},
        {"stream", true}
    };

    if (!tools.is_null() && !tools.empty()) {
        body["tools"] = tools;
        body["tool_choice"] = "auto";
    }

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

    // Variables for aggregating the full response from stream chunks
    std::string full_content;
    json tool_calls = json::array();
    // Helper variables to assemble a single tool_call from multiple chunks
    std::string current_tool_id;
    std::string current_tool_name;
    std::string current_tool_args;

    std::string sse_buffer;

    auto content_receiver = [&](const char *data, size_t data_length) {
        sse_buffer.append(data, data_length);

        size_t pos;
        // An SSE event is terminated by double newlines.
        while ((pos = sse_buffer.find("\n\n")) != std::string::npos) { 
            std::string event_chunk = sse_buffer.substr(0, pos);
            sse_buffer.erase(0, pos + 2);

            std::istringstream stream(event_chunk);
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

                            // 1. Aggregate and stream text tokens
                            if (delta.contains("content") && delta["content"].is_string()) {
                                std::string token = delta["content"].get<std::string>();
                                if (!token.empty()) {
                                    full_content += token;
                                    if (send_stream_chunk) {
                                        send_stream_chunk(token);
                                    }
                                }
                            }

                            // 2. Aggregate tool_calls
                            if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
                                for (const auto& tool_call_delta : delta["tool_calls"]) {
                                    // If an ID arrives, it's a new tool_call
                                    if (tool_call_delta.contains("id")) {
                                        // Finalize the previous tool_call if it exists
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
                                        // Start a new one
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
                        SPDLOG_WARN("Failed to parse stream chunk: {}. Chunk: {}", e.what(), data_str);
                    }
                }
            }
        }
        return true;
    };

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/chat/completions", headers, body_str, "application/json", content_receiver);
        if (res) break;
        SPDLOG_ERROR("Attempt {} of {} for chat completion failed. Connection error: {}. Retrying...",
                     attempt, config.retry_count, httplib::to_string(res.error()));
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (!res || res->status != 200) {
        if (res) {
            // Log the full error body from the LLM for better diagnostics.
            SPDLOG_ERROR("Error from LLM. Status: {}. Body: {}", res->status, res->body);
        } else {
            SPDLOG_ERROR("Connection error to LLM: {}", httplib::to_string(res.error()));
        }
        // Provide a more specific error message to the user.
        return { .step_failed = true, .error_message = "Ошибка при обращении к языковой модели (HTTP " + std::to_string(res ? res->status : 0) + "). Проверьте лог-файл для деталей." };
    }

    // After stream is complete, finalize the last tool_call if it exists
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

    // Construct the final JSON response in the format AssistantRole expects
    json message = { {"role", "assistant"} };
    if (!full_content.empty()) {
        message["content"] = full_content;
    }
    if (!tool_calls.empty()) {
        message["tool_calls"] = tool_calls;
    }

    json final_response = {
        {"choices", {{{"message", message}}}}
    };

    return { .llm_response = final_response };
}

std::vector<float> OpenAIProvider::createEmbedding(const std::string& text) {
    SPDLOG_DEBUG("Generating embedding for text (size: {} chars)...", text.length());

    json body = {
        { "input", text },
        { "model", config.embedding_model_name }
    };
    
    std::string body_str = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    
    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    // Use a separate client for embedding with its own timeout
    httplib::Client embedding_cli(config.server_host, config.server_port);
    embedding_cli.set_connection_timeout(5, 0);
    embedding_cli.set_read_timeout(config.embedding_timeout_sec, 0);

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = embedding_cli.Post("/v1/embeddings", headers, body_str, "application/json");
        if (res) break;
        SPDLOG_WARN("Embedding attempt {}/{} failed. Connection error: {}. Retrying in {} ms...",
                    attempt, config.retry_count, httplib::to_string(res.error()), config.retry_delay_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (res && res->status == 200) {
        try {
            auto json_body = json::parse(res->body);
            if (json_body.contains("data") && !json_body["data"].empty()) {
                const auto& first_item = json_body["data"][0];
                if (first_item.contains("embedding")) {
                    return first_item["embedding"].get<std::vector<float>>();
                }
            }
            SPDLOG_ERROR("Unexpected JSON structure in embedding response. Body: {}", res->body);
        } catch (const json::exception& e) {
            SPDLOG_ERROR("Failed to parse JSON response for embedding. Details: {}", e.what());
        }
    } else {
        if (res) {
            SPDLOG_ERROR("Failed to get embedding. Status: {}. Body: {}", res->status, res->body);
        } else {
            SPDLOG_ERROR("Connection failed for embedding. Details: {}", httplib::to_string(res.error()));
        }
    }

    return {};
}

std::string OpenAIProvider::generateChunkSummary(const std::string& code_chunk, const std::string& chunk_name) {
    SPDLOG_DEBUG("Generating summary for chunk '{}'", chunk_name);
// return "";
    json messages = json::array({
        {
            {"role", "system"},
            {"content", "Ты — эксперт по программированию. Твоя задача — создать очень краткое, однострочное описание (summary) для предоставленного фрагмента кода на русском языке. Описание должно отражать основное назначение этого кода."}
        },
        {
            {"role", "user"},
            {"content", "Вот фрагмент кода для анализа:\n```\n" + code_chunk + "\n```"}
        }
    });

    json body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        {"temperature", 0.0},
        {"stream", false}
    };

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump();

    httplib::Client summary_cli(config.server_host, config.server_port);
    summary_cli.set_connection_timeout(5, 0);
    summary_cli.set_read_timeout(config.summary_generation_timeout_sec, 0); 

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = summary_cli.Post("/v1/chat/completions", headers, body_str, "application/json");
        if (res) break;
        SPDLOG_WARN("Summary generation attempt {}/{} failed. Connection error: {}. Retrying in {} ms...",
                    attempt, config.retry_count, httplib::to_string(res.error()), config.retry_delay_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (!res || res->status != 200) {
        if (res) {
            SPDLOG_ERROR("Failed to generate chunk summary for '{}'. Status: {}. Body: {}", chunk_name, res->status, res->body);
        } else {
            SPDLOG_ERROR("Connection failed during summary generation for '{}'. Details: {}", chunk_name, httplib::to_string(res.error()));
        }
        return "";
    }

    try {
        auto json_body = json::parse(res->body);
        if (json_body.contains("choices") && !json_body["choices"].empty()) {
            const auto& first_choice = json_body["choices"][0];
            if (first_choice.contains("message") && first_choice["message"].contains("content")) {
                std::string summary = first_choice["message"]["content"].get<std::string>();
                SPDLOG_DEBUG("Generated summary for '{}': {}", chunk_name, summary);
                return summary;
            }
        }
        SPDLOG_ERROR("Unexpected JSON structure in summary response for '{}'. Body: {}", chunk_name, res->body);
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Failed to parse JSON response for summary of '{}'. Details: {}. Body: {}", chunk_name, e.what(), res->body);
    }

    return "";
}

std::optional<LLMProvider::ServerProperties> OpenAIProvider::fetchServerProperties() const {
    SPDLOG_INFO("Fetching server properties from LLM provider...");

    httplib::Client props_cli(config.server_host, config.server_port);
    props_cli.set_connection_timeout(5, 0);
    props_cli.set_read_timeout(10, 0); // 10 seconds should be plenty

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    auto res = props_cli.Get("/v1/models", headers);

    if (!res || res->status != 200) {
        if (res) {
            SPDLOG_ERROR("Failed to fetch server properties. Status: {}. Body: {}", res->status, res->body);
        } else {
            SPDLOG_ERROR("Connection failed while fetching server properties. Details: {}", httplib::to_string(res.error()));
        }
        return std::nullopt;
    }

    try {
        auto json_body = json::parse(res->body);
        if (json_body.contains("data") && json_body["data"].is_array()) {
            LLMProvider::ServerProperties props;
            for (const auto& model_obj : json_body["data"]) {
                if (model_obj.contains("id") && model_obj["id"].is_string()) {
                    props.models.push_back(model_obj["id"].get<std::string>());
                }
            }
            SPDLOG_INFO("Successfully fetched {} models from the server.", props.models.size());
            return props;
        }
        SPDLOG_ERROR("Unexpected JSON structure in /v1/models response. Body: {}", res->body);
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Failed to parse JSON response from /v1/models. Details: {}. Body: {}", e.what(), res->body);
    }

    return std::nullopt;
}
