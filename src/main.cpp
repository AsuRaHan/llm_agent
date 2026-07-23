// Per README instructions, to prevent conflicts with cpp-httplib
#ifdef _WIN32
#include <Windows.h>
#endif

#include "ContextIndexer.h" // Теперь это наш фасад
#include "AssistantRole.h"
#include "WebSocketServer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <chrono>
#include <locale>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <csignal> // Для обработки сигналов (Ctrl+C)
#include "Logger.h"
#include "FileWatcher.h"
#include "ApiHandlers.h"
#include "EmbeddingClient.h"
#include "OpenAIProvider.h"
#include "httplib.h"

// Глобальный указатель на обработчик API для доступа из обработчика сигналов
// Это необходимо, чтобы мы могли вызвать stop() из статического контекста.
std::function<void()> g_stop_function;

namespace fs = std::filesystem;

/// Создает безопасное для файловой системы имя директории из пути проекта
std::string sanitize_path_for_dirname(const fs::path& p) {
    std::string sanitized = fs::weakly_canonical(p).string();
    std::replace(sanitized.begin(), sanitized.end(), '\\', '/'); // to unix-like
    std::replace(sanitized.begin(), sanitized.end(), ':', '_'); // C:_...
    std::replace(sanitized.begin(), sanitized.end(), '/', '_');
    // Убираем ведущие подчеркивания, которые могут получиться из C:_
    if (sanitized.starts_with("_")) sanitized = sanitized.substr(1);
    return sanitized;
}

/// Обработчик сигналов (например, Ctrl+C) для грациозного завершения
void signalHandler(int signum) {
    SPDLOG_INFO("Получен сигнал завершения (сигнал {}). Запуск грациозной остановки...", signum);
    if (g_stop_function) {
        g_stop_function();
    } else {
        // Если обработчик еще не инициализирован, просто выходим
        exit(signum);
    }
}
// ============================================================================
// Initialization Functions
// ============================================================================

/// Инициализирует папку .shdata
/// Возвращает true при успехе, false при ошибке
bool initializeDataDirectory(const fs::path& app_root_dir)
{
    std::string dataDir = ".shdata";
    std::string fullDataPath = (app_root_dir / dataDir).string();
    
    try {
        if (!fs::exists(fullDataPath) || !fs::is_directory(fullDataPath)) {
            fs::create_directories(fullDataPath);
            SPDLOG_INFO("Папка '.shdata' создана: {}", fs::absolute(fullDataPath).string());
        } else {
            SPDLOG_INFO("Папка '.shdata' уже существует: {}", fs::absolute(fullDataPath).string());
        }
        
        // Проверка наличия config.json
        // std::string configPath = fs::path(fullDataPath).append("config.json").string();
        // if (!fs::exists(configPath)) {
        //     SPDLOG_WARN("Файл конфигурации не найден: {}", configPath);
        //     return false;
        // }
        
        return true;
    } catch (const fs::filesystem_error& e) {
        SPDLOG_CRITICAL("Ошибка при инициализации папки '.shdata': {}", e.what());
        return false;
    }
}

/// Инициализирует приложение: логирование, конфиг, рабочий каталог
/// Возвращает true при успехе, false при ошибке
bool initializeApplication(const std::string& projectDir, Config& outConfig, const fs::path& app_root_dir)
{
    setlocale(LC_ALL, ".UTF8");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8); // Устанавливаем кодовую страницу для ВВОДА
#endif

    if (!initializeDataDirectory(app_root_dir)) {
        std::cerr << "CRITICAL: Не удалось инициализировать папку '.shdata'. Завершение работы." << std::endl;
        return false;
    }

    std::string config_path = (app_root_dir / ".shdata" / "config.json").string();
    if (!outConfig.load(config_path)) {
        // Логгер еще не инициализирован, используем cerr
        std::cerr << "CRITICAL: Не удалось загрузить конфигурацию '" << config_path << "'. Завершение работы." << std::endl;
        return false;
    }

    if (fs::path(outConfig.log_file_path).is_relative()) {
        outConfig.log_file_path = (app_root_dir / outConfig.log_file_path).string();
    }
    init_logger(outConfig);

    SPDLOG_INFO("Каталог проекта для индексации: {}", fs::absolute(projectDir).string());

    return true;
}


// ============================================================================
// Server-side helper functions
// ============================================================================
bool probeEmbeddingEndpoint(const Config& config) {
    SPDLOG_DEBUG("Проверка эндпоинта /v1/embeddings...");
    httplib::Client probe_cli(config.server_host, config.server_port);
    probe_cli.set_connection_timeout(2, 0);

    auto res = probe_cli.Post("/v1/embeddings", "{}", "application/json");

    if (!res) {
        SPDLOG_WARN("Проверка не удалась: не удалось подключиться к эндпоинту /v1/embeddings. Ошибка: {}", httplib::to_string(res.error()));
        return false;
    }

    if (res->status == 404) {
        SPDLOG_WARN("Проверка не удалась: эндпоинт /v1/embeddings не найден (сервер вернул 404).");
        return false;
    }
    
    SPDLOG_DEBUG("Проверка успешна: эндпоинт /v1/embeddings существует (сервер ответил статусом {}).", res->status);
    return true;
}

std::optional<ServerProperties> fetchServerProperties(const Config& config) {
    SPDLOG_INFO("Получение свойств с сервера LLM на {}:{}...", config.server_host, config.server_port);
    
    httplib::Client temp_cli(config.server_host, config.server_port);
    temp_cli.set_connection_timeout(5, 0);
    temp_cli.set_read_timeout(10, 0);

    httplib::Headers headers;
    if (!config.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config.api_key);
    }

    auto res = temp_cli.Get("/props", headers);

    if (!res) {
        SPDLOG_CRITICAL("Не удалось подключиться к серверу LLM для получения свойств. Ошибка: {}", httplib::to_string(res.error()));
        return std::nullopt;
    }

    if (res->status != 200) {
        SPDLOG_CRITICAL("Сервер LLM ответил ошибкой на запрос /props. Статус: {}.", res->status);
        return std::nullopt;
    }

    try {
        using json = nlohmann::json;
        json body = json::parse(res->body);
        ServerProperties props;

        props.model_path = body.value("model_path", "unknown");
        // props.chat_template = body.value("chat_template", "");
        props.context_size = body.value("default_generation_settings", json::object()).value("n_ctx", body.value("context_size", 4096));
        props.embedding_enabled = body.value("embedding", probeEmbeddingEndpoint(config));

        SPDLOG_INFO("Свойства сервера успешно получены:");
        SPDLOG_INFO("  - Модель: {}", props.model_path);
        // SPDLOG_INFO("  - Шаблон чата: '{}'", props.chat_template.empty() ? "не указан" : props.chat_template);
        SPDLOG_INFO("  - Размер контекста: {}", props.context_size);
        SPDLOG_INFO("  - Поддержка эмбеддингов: {}", props.embedding_enabled ? "Да" : "Нет");

        return props;
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_CRITICAL("Не удалось разобрать JSON-ответ от /props: {}", e.what());
        return std::nullopt;
    }
}


/// Индексирует директорию проекта и возвращает заполненный ContextIndexer
/// Возвращает nullptr при ошибке
std::unique_ptr<ContextIndexer> indexProject(const std::string& projectDir, const Config& config, std::shared_ptr<LLMProvider> provider, const fs::path& project_data_dir)
{
    try {
        std::string index_meta_path = (project_data_dir / "index_meta.json").string();
        std::string index_bin_path = (project_data_dir / "index.bin").string();
        std::string file_indexer_meta_path = (project_data_dir / "file_indexer_meta.json").string();

        auto indexer = std::make_unique<ContextIndexer>(provider, config, index_meta_path, index_bin_path, file_indexer_meta_path);
        
        SPDLOG_INFO("Запуск индексирования проекта...");
        indexer->indexDirectory(projectDir);
        
        return indexer;
    } catch (const std::exception& e) {
        SPDLOG_CRITICAL("Ошибка при индексировании: {}", e.what());
        return nullptr;
    }
}

// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char* argv[])
{
    SPDLOG_INFO("Запуск агента Smart Hammer...");
    // Регистрируем обработчик для сигнала SIGINT (Ctrl+C)
    signal(SIGINT, signalHandler);

    try {
        // --- Диагностическое логирование в файл ---
        // Это самый первый шаг, чтобы поймать ошибки, возникающие до инициализации spdlog.
        // Лог будет в файле agent_startup_debug.log рядом с exe.
        fs::path app_root_dir;
        if (argc > 0 && argv[0] != nullptr) {
            app_root_dir = fs::absolute(argv[0]).parent_path();
        } else {
            app_root_dir = fs::current_path();
        }

        // try {
        //     std::ofstream debug_log(app_root_dir / "agent_startup_debug.log", std::ios::app);
        //     debug_log << "--- Agent starting ---" << std::endl;
        //     debug_log << "argc: " << argc << std::endl;
        //     for (int i = 0; i < argc; ++i) {
        //         debug_log << "argv[" << i << "]: " << (argv[i] ? argv[i] : "nullptr") << std::endl;
        //     }
        //     debug_log.flush();
        // } catch (const std::exception& e) {
        //     // Не удалось даже создать лог, но выводим в консоль, если получится.
        //     std::cerr << "Early logging to file failed: " << e.what() << std::endl;
        // }
        // --- Конец диагностического логирования ---

        // ====== Инициализация ======
        Config config;
        std::string projectDir = (argc > 1 && argv[1] != nullptr) ? argv[1] : ".";
        if (projectDir.empty()) {
            projectDir = ".";
        }

        if (!initializeApplication(projectDir, config, app_root_dir)) {
            return 1;
        }

        // Отладочное логирование аргументов запуска
        SPDLOG_DEBUG("Приложение запущено с {} аргументами:", argc);
        for (int i = 0; i < argc; ++i) {
            SPDLOG_DEBUG("  argv[{}]: {}", i, (argv[i] ? argv[i] : "nullptr"));
        }
        SPDLOG_DEBUG("Определен корневой каталог приложения: {}", app_root_dir.string());
        SPDLOG_DEBUG("Определен каталог проекта для индексации: {}", projectDir);

        // Создаем уникальную директорию для данных этого проекта
        fs::path project_path_abs = fs::absolute(projectDir);
        if (!fs::exists(project_path_abs) || !fs::is_directory(project_path_abs)) {
            SPDLOG_CRITICAL("Указанный каталог проекта не существует или не является директорией: {}", project_path_abs.string());
            std::cerr << "CRITICAL: Project directory does not exist or is not a directory: " << project_path_abs.string() << std::endl;
            return 1;
        }

        std::string project_safe_name = sanitize_path_for_dirname(project_path_abs);
        fs::path project_data_dir = app_root_dir / ".shdata" / project_safe_name;

        try {
            if (!fs::exists(project_data_dir)) {
                fs::create_directories(project_data_dir);
                SPDLOG_INFO("Создана директория данных проекта: {}", project_data_dir.string());
            }
        } catch (const fs::filesystem_error& e) {
            SPDLOG_CRITICAL("Не удалось создать директорию данных проекта '{}': {}", project_data_dir.string(), e.what());
            return 1;
        }
        // ====== Проверка совместимости с LLM сервером ======
        auto server_props_opt = fetchServerProperties(config);

        if (!server_props_opt) {
            SPDLOG_CRITICAL("Не удалось получить свойства с LLM сервера. Проверьте, что сервер запущен и доступен.");
            return 1;
        }
        if (!server_props_opt->embedding_enabled) {
            SPDLOG_CRITICAL("Сервер LLM запущен без поддержки эмбеддингов (флаг --embedding). Агент не может работать без этой функции. Завершение работы.");
            return 1;
        }
        
        // ====== Создание LLM провайдера ======
        auto llmProvider = std::make_shared<OpenAIProvider>(config);

        // ====== Индексирование проекта ======
        auto indexer = indexProject(projectDir, config, llmProvider, project_data_dir);
        if (!indexer) {
            return 1;
        }

        if (indexer->getEmbeddingsCount() == 0) {
            SPDLOG_WARN("Индексирование завершено, но эмбеддинги не получены. Возможно, все файлы были игнорированы.");
            std::cout << "\nВнимание: Индекс пуст. Новые или измененные файлы будут проиндексированы автоматически.\n";
            // Не выходим, позволяем агенту работать с пустым проектом.
        }

        SPDLOG_INFO("Запуск Agent...");

        if (fs::path(config.web_server_root_dir).is_relative()) {
            config.web_server_root_dir = (app_root_dir / config.web_server_root_dir).string();
        }

        // ====== Запуск в режиме веб-сервера ======
        SPDLOG_INFO("Запуск в режиме веб-сервера...");
        // Очистка консоли перед запуском сервера
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
        std::cout << "\n";
        std::cout << "================================================================================\n";
        std::cout << "                    Smart Hammer - Web Server Mode\n";
        std::cout << "================================================================================\n";
        std::cout << "\nСервер запущен:\n";
        std::cout << "  • WebSocket: ws://" << config.web_server_host << ":" << config.web_server_port << "/ws\n";
        std::cout << "  • Frontend:  http://" << config.web_server_host << ":" << config.web_server_port << "/\n";
        std::cout << "  • Indexed files: " << indexer->getFileCount() << "\n";
        std::cout << "  • Embeddings:    " << indexer->getEmbeddingsCount() << "\n";
        std::cout << "  • Project Directory: " << projectDir << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << "\nНажмите Ctrl+C для остановки сервера.\n\n";

        // Создание и инициализация обработчиков API.
        ApiHandlers apiHandlers(llmProvider, config, *indexer, projectDir, app_root_dir);
        
        // Сохраняем указатель для глобального доступа из обработчика сигналов
        g_stop_function = [&apiHandlers]() { apiHandlers.stop(); };

        // Запуск сервера (этот вызов блокирует выполнение)
        apiHandlers.start(projectDir);

        // Небольшая задержка, чтобы дать время всем фоновым потокам (например, от httplib)
        // корректно завершиться после вызова stop().
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        SPDLOG_INFO("Server stopped.");
        std::cout << "\nСервер остановлен.\n";

    } catch (const std::exception& e) {
        // Если spdlog инициализирован, используем его.
        if (spdlog::default_logger()) {
            SPDLOG_CRITICAL("Перехвачено необработанное исключение в main: {}", e.what());
        } else {
            // Иначе, выводим в cerr и пытаемся записать в файл.
            std::cerr << "CRITICAL EXCEPTION (logger not initialized): " << e.what() << std::endl;
            try {
                fs::path crash_log_path = (argc > 0 && argv[0] != nullptr) ? fs::absolute(argv[0]).parent_path() : fs::current_path();
                std::ofstream crash_log(crash_log_path / "agent_CRASH.log");
                crash_log << "Exception: " << e.what() << std::endl;
            } catch(...) { /* Не удалось записать лог сбоя */ }
        }
        return 1;
    } catch (...) {
        if (spdlog::default_logger()) {
            SPDLOG_CRITICAL("Перехвачено неизвестное необработанное исключение в main!");
        } else {
            std::cerr << "UNKNOWN CRITICAL EXCEPTION (logger not initialized)" << std::endl;
            try {
                fs::path crash_log_path = (argc > 0 && argv[0] != nullptr) ? fs::absolute(argv[0]).parent_path() : fs::current_path();
                std::ofstream crash_log(crash_log_path / "agent_CRASH.log");
                crash_log << "Unknown exception caught in main." << std::endl;
            } catch(...) { /* Не удалось записать лог сбоя */ }
        }
        return 1;
    }

    return 0;
}