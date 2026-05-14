#include "Logger.h"
#include "Config.h" // Include Config for logger settings
#include <iostream> // For std::cerr
#include <vector>

static std::shared_ptr<spdlog::logger> global_logger;

// Вспомогательная функция для преобразования строки в уровень логирования spdlog
spdlog::level::level_enum level_from_string(const std::string& level_str) {
    std::string lower_level = level_str;
    std::transform(lower_level.begin(), lower_level.end(), lower_level.begin(), [](unsigned char c){ return std::tolower(c); });
    if (lower_level == "trace") return spdlog::level::trace;
    if (lower_level == "debug") return spdlog::level::debug;
    if (lower_level == "info") return spdlog::level::info;
    if (lower_level == "warn") return spdlog::level::warn;
    if (lower_level == "error") return spdlog::level::err;
    if (lower_level == "critical") return spdlog::level::critical;
    
    // Если значение некорректно, выводим предупреждение в stderr, так как логгер еще может быть не настроен
    std::cerr << "[Logger WARNING] Некорректное значение уровня логирования '" << level_str << "' в конфиге. Используется 'info' по умолчанию." << std::endl;
    return spdlog::level::info;
}

void init_logger(const Config& config) {
    std::vector<spdlog::sink_ptr> sinks;

    // Файловый логгер создается всегда
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file_path, true);
        file_sink->set_level(level_from_string(config.log_file_level)); // Пишем в файл с уровня, указанного в конфиге
        sinks.push_back(file_sink);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Ошибка инициализации файлового логгера: " << ex.what() << std::endl;
    }

    // Консольный логгер создается на основе конфига
    if (config.log_to_console) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        // Устанавливаем уровень логирования для консоли из конфига
        console_sink->set_level(level_from_string(config.log_console_level));
        sinks.push_back(console_sink);
    }

    global_logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
    global_logger->set_level(spdlog::level::trace); // Устанавливаем самый низкий уровень (trace), чтобы не фильтровать сообщения до того, как они попадут в sinks
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
