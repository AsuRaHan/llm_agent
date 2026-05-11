#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>

// Function to initialize the logger
void init_logger(const std::string& log_file_path);

// Function to get the global logger instance
std::shared_ptr<spdlog::logger> get_logger();