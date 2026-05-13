#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
// #include "Config.h"
// Forward declaration to avoid including the full Config header here
struct Config;

// Function to initialize the logger
void init_logger(const Config& config);

// Function to get the global logger instance
std::shared_ptr<spdlog::logger> get_logger();
