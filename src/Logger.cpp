#include "Logger.h"
#include <iostream> // For fallback error output

static std::shared_ptr<spdlog::logger> global_logger;

void init_logger(const std::string& log_file_path) {
    if (!global_logger) {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info); // Console logs INFO and above
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
            file_sink->set_level(spdlog::level::debug); // File logs DEBUG and above
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            global_logger = std::make_shared<spdlog::logger>("agent_logger", spdlog::sinks_init_list({console_sink, file_sink}));
            global_logger->set_level(spdlog::level::debug); // Overall logger level
            global_logger->flush_on(spdlog::level::warn); // Flush on warnings and errors
            spdlog::set_default_logger(global_logger);

            SPDLOG_INFO("Logger initialized. Logging to console (INFO+) and file '{}' (DEBUG+).", log_file_path);
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        }
    }
}

std::shared_ptr<spdlog::logger> get_logger() {
    return global_logger;
}