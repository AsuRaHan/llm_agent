#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct Config {
    // Server settings
    std::string server_host = "localhost";
    int server_port = 8080;

    // Network settings
    int retry_count = 3;
    int retry_delay_ms = 500;

    // Timeout settings
    int embedding_timeout_sec = 60;
    int chat_completion_timeout_sec = 300;
    int embedding_max_text_length = 1500;

    // Indexing settings
    size_t chunk_size = 1000;
    size_t chunk_overlap = 200;
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