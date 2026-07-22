#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Holds properties discovered from the LLM server
struct ServerProperties {
    bool embedding_enabled = false;
    int context_size = 4096;
    std::string model_path;
    std::string chat_template;
};

struct Config {
    // Server settings
    std::string server_host = "localhost";
    int server_port = 8080;
    std::string api_key = ""; // API key for remote services (e.g., "Bearer sk-...")

    // Network settings
    int retry_count = 3;
    int retry_delay_ms = 500;

    // Timeout settings
    int embedding_timeout_sec = 300;
    int chat_completion_timeout_sec = 300;
    int summary_generation_timeout_sec = 1600;
    int max_tool_calls = 150;    
    std::string embedding_model_name = "any";
    std::string chat_model_name = "any";
    int embedding_max_text_length = 1500;
    int embedding_chunk_overlap = 200;

    // Tools settings
    bool enable_dangerous_tools = true; // Set to true to enable tools that can modify the filesystem (e.g., WriteFileTool)
    std::vector<std::string> dangerous_tools;
    
    bool enable_self_correction = true; // Включает самокоррекцию модели
    // Web Search settings
    std::string web_search_api_key = ""; // API key for a search service like Serper.dev

    // Logging settings
    std::string log_file_path = "agent.log";
    bool log_to_console = true;
    std::string log_file_level = "trace";   // "trace", "debug", "info", "warn", "error", "critical"
    std::string log_console_level = "trace"; // "trace", "debug", "info", "warn", "error", "critical"

    // HTTP/WebSocket Server settings (for Web UI)
    std::string web_server_host = "localhost";
    int web_server_port = 9000;
    bool enable_web_ui = true; // This is not used anywhere, but kept for consistency
    std::string web_server_root_dir = "frontend";

    // Project settings
    std::string project_dir = "."; // Директория проекта для индексации

    // Indexing settings
    // size_t chunk_size = 10000; // Устарело, используется embedding_max_text_length
    // size_t chunk_overlap = 2000; // Устарело, используется embedding_chunk_overlap
    int top_k_results = 5;
    int initial_index_size = 50000;
    std::string chunking_strategy = "tree-sitter-hybrid"; // "fixed", "tree-sitter", or "tree-sitter-hybrid"
    std::vector<std::string> ignored_directories;
    std::vector<std::string> ignored_extensions;
    std::vector<std::string> ignored_files;

    /**
     * @brief Loads configuration from a JSON file.
     * If the file doesn't exist, it creates a default one.
     * @param filepath Path to the configuration file.
     * @return true if loading was successful, false otherwise.
     */
    bool load(const std::string& filepath);

private:
    void create_default(const std::string& filepath) const;
};