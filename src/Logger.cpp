#include "Logger.h"
#include "Config.h" // Include Config for logger settings
#include <iostream> // For std::cerr
#include <vector>

static std::shared_ptr<spdlog::logger> global_logger;

void init_logger(const Config& config) {
    std::vector<spdlog::sink_ptr> sinks;

    // Файловый логгер создается всегда
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file_path, true);
        file_sink->set_level(spdlog::level::debug); // Пишем в файл все, начиная с debug
        sinks.push_back(file_sink);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Ошибка инициализации файлового логгера: " << ex.what() << std::endl;
    }

    // Консольный логгер создается на основе конфига
    if (config.log_to_console) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        // В консоль выводим только предупреждения и ошибки, чтобы не засорять диалог
        console_sink->set_level(spdlog::level::warn);
        sinks.push_back(console_sink);
    }

    global_logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
    global_logger->set_level(spdlog::level::debug); // Устанавливаем самый низкий уровень для самого логгера
    spdlog::set_default_logger(global_logger);
}

std::shared_ptr<spdlog::logger> get_logger() {
    // Если логгер не был инициализирован, spdlog вернет логгер по умолчанию,
    // который пишет в консоль. Это безопасный фолбэк.
    if (!global_logger) {
        return spdlog::default_logger();
    }
    return global_logger;
}
