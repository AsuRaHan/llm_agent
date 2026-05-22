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

    std::string accumulated_content;
    json accumulated_tool_calls = json::array();
    std::map<int, json> tool_call_map; 
    
    std::string sse_buffer;

    auto content_receiver = [&](const char *data, size_t data_length) {
        sse_buffer.append(data, data_length);

        size_t pos;
        while ((pos = sse_buffer.find("\n\n")) != std::string::npos) {
            std::string event_str = sse_buffer.substr(0, pos);
            sse_buffer.erase(0, pos + 2); // consume the event and the terminator

            std::istringstream stream(event_str);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.rfind("data: ", 0) == 0) {
                    std::string data_str = line.substr(6);
                    if (data_str.find("[DONE]") != std::string::npos) {
                        return true;
                    }

                    try {
                        json chunk = json::parse(data_str);
                        if (chunk["choices"].empty()) {
                            continue;
                        }

                        json delta = chunk["choices"][0]["delta"];

                        // Handle content chunks
                        if (delta.contains("content") && delta["content"].is_string()) {
                            std::string token = delta["content"];
                            accumulated_content += token;
                            if (send_stream_chunk) {
                                send_stream_chunk(token);
                            }
                        }

                        // Handle tool call chunks
                        if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
                            for (const auto& tool_call_chunk : delta["tool_calls"]) {
                                int index = tool_call_chunk["index"];

                                if (tool_call_chunk.contains("id")) {
                                    tool_call_map[index]["id"] = tool_call_chunk["id"];
                                }
                                if (tool_call_chunk.contains("type")) {
                                    tool_call_map[index]["type"] = tool_call_chunk["type"];
                                }
                                if (tool_call_chunk.contains("function")) {
                                    if (tool_call_chunk["function"].contains("name")) {
                                        tool_call_map[index]["function"]["name"] = tool_call_chunk["function"]["name"];
                                    }
                                    if (tool_call_chunk["function"].contains("arguments")) {
                                        tool_call_map[index]["function"]["arguments"] = tool_call_map[index]["function"]["arguments"].get<std::string>() + tool_call_chunk["function"]["arguments"].get<std::string>();
                                    }
                                }
                            }
                        }
                    } catch (const json::exception& e) {
                        SPDLOG_ERROR("Failed to parse SSE chunk: {}. Chunk: {}", e.what(), data_str);
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
            SPDLOG_ERROR("Error from LLM. Status: {}. Body: {}", res->status, res->body);
        } else {
            SPDLOG_ERROR("Connection error to LLM: {}", httplib::to_string(res.error()));
        }
        return { .step_failed = true, .error_message = "Failed to get a response from the language model." };
    }

    // After stream is complete, assemble the final tool calls
    for (auto const& [index, val] : tool_call_map) {
        accumulated_tool_calls.push_back(val);
    }
    
    // Construct the final JSON response in the format AssistantRole expects
    json message = {
        {"role", "assistant"},
        {"content", accumulated_content}
    };

    if (!accumulated_tool_calls.empty()) {
        message["tool_calls"] = accumulated_tool_calls;
    }

    json final_response = {
        {"choices", {{{"message", message}}}}
    };

    return { .llm_response = final_response };
}

nlohmann::json OpenAIProvider::generatePlan(const std::string& user_query) {
    SPDLOG_INFO("Generating plan for query: '{}'", user_query);

    std::string system_prompt_text =
        "You are the Smart Hammer AI architect. Your task is to break down the user's request into "
        "minimal atomic steps. Each step should be accomplishable in one or two tool calls.\\n"
        "Provide the response STRICTLY in a JSON object format with a 'plan' key, which contains an array of strings. "
        "Do not add any other text or markdown formatting.\\n"
        "Example: {\\\"plan\\\": [\\\"Examine the structure of CodeParser.cpp\\\", \\\"Find memory leaks\\\", \\\"Apply a diff patch\\\"]}";

    json messages = {
        {{"role", "system"}, {"content", system_prompt_text}},
        {{"role", "user"}, {"content", "Task: " + user_query}}
    };

    json body = {
        {"messages", messages},
        {"model", config.chat_model_name},
        {"temperature", 0.0},
        {"response_format", { {"type", "json_object"} }}
    };

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }
    std::string body_str = body.dump(-1, ' ', false, json::error_handler_t::replace);

    httplib::Result res;
    for (int attempt = 1; attempt <= config.retry_count; ++attempt) {
        res = cli.Post("/v1/chat/completions", headers, body_str, "application/json");
        if (res) break;
        SPDLOG_ERROR("Attempt {} of {} for plan generation failed. Connection error: {}. Retrying...",
                    attempt, config.retry_count, httplib::to_string(res.error()));
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
    }

    if (!res || res->status != 200) {
        if (res) {
            SPDLOG_ERROR("Error from LLM on plan generation. Status: {}. Body: {}", res->status, res->body);
        } else {
            SPDLOG_ERROR("Connection error to LLM on plan generation: {}", httplib::to_string(res.error()));
        }
        return {{"error", "Failed to generate plan."}};
    }

    try {
        return json::parse(res->body);
    } catch (const json::exception& e) {
        SPDLOG_ERROR("Failed to parse plan generation response: {}", e.what());
        return {{"error", "Model returned invalid JSON."}};
    }
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
    SPDLOG_WARN("generateChunkSummary is not implemented for OpenAIProvider.");
    return "";
}

std::optional<LLMProvider::ServerProperties> OpenAIProvider::fetchServerProperties() const {
    SPDLOG_WARN("fetchServerProperties is not implemented for OpenAIProvider.");
    return std::nullopt;
}
