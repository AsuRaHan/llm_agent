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

// Глобальный указатель на обработчик API для доступа из обработчика сигналов
// Это необходимо, чтобы мы могли вызвать stop() из статического контекста.
ApiHandlers* g_apiHandlers = nullptr;

namespace fs = std::filesystem;

/// Обработчик сигналов (например, Ctrl+C) для грациозного завершения
void signalHandler(int signum) {
    SPDLOG_INFO("Получен сигнал завершения (сигнал {}). Запуск грациозной остановки...", signum);
    if (g_apiHandlers) {
        g_apiHandlers->stop();
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
bool initializeDataDirectory(const std::string& projectDir)
{
    std::string dataDir = ".shdata";
    std::string fullDataPath = fs::path(projectDir).append(dataDir).string();
    
    try {
        if (!fs::exists(fullDataPath) || !fs::is_directory(fullDataPath)) {
            fs::create_directories(fullDataPath);
            SPDLOG_INFO("Папка '.shdata' создана: {}", fs::absolute(fullDataPath).string());
        } else {
            SPDLOG_INFO("Папка '.shdata' уже существует: {}", fs::absolute(fullDataPath).string());
        }
        
        // Проверка наличия config.json
        std::string configPath = fs::path(fullDataPath).append("config.json").string();
        if (!fs::exists(configPath)) {
            SPDLOG_WARN("Файл конфигурации не найден: {}", configPath);
            return false;
        }
        
        return true;
    } catch (const fs::filesystem_error& e) {
        SPDLOG_CRITICAL("Ошибка при инициализации папки '.shdata': {}", e.what());
        return false;
    }
}

/// Инициализирует приложение: логирование, конфиг, рабочий каталог
/// Возвращает true при успехе, false при ошибке
bool initializeApplication(const std::string& projectDir, Config& outConfig)
{
    setlocale(LC_ALL, ".UTF8");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8); // Устанавливаем кодовую страницу для ВВОДА
#endif

    if (!initializeDataDirectory(projectDir)) {
        std::cerr << "CRITICAL: Не удалось инициализировать папку '.shdata'. Завершение работы." << std::endl;
        return false;
    }

    if (!outConfig.load(".shdata/config.json")) {
        // Логгер еще не инициализирован, используем cerr
        std::cerr << "CRITICAL: Не удалось загрузить конфигурацию 'config.json'. Завершение работы." << std::endl;
        return false;
    }

    init_logger(outConfig);

    std::string workDir = (projectDir == ".") ? projectDir : projectDir;
    if (projectDir != ".") {
        try {
            fs::current_path(projectDir);
            SPDLOG_INFO("Рабочий каталог изменен на: {}", fs::current_path().string());
        } catch (const fs::filesystem_error& e) {
            SPDLOG_CRITICAL("Ошибка при смене рабочего каталога на '{}': {}", projectDir, e.what());
            return false;
        }
    } else {
        SPDLOG_INFO("Рабочий каталог по умолчанию: {}", fs::current_path().string());
    }

    return true;
}


/// Индексирует директорию проекта и возвращает заполненный ContextIndexer
/// Возвращает nullptr при ошибке
std::unique_ptr<ContextIndexer> indexProject(const std::string& projectDir, const Config& config)
{
    try {
        auto indexer = std::make_unique<ContextIndexer>(config);
        
        SPDLOG_INFO("Запуск индексирования проекта...");
        indexer->indexDirectory(projectDir);
        
        SPDLOG_INFO("Индексирование завершено. Сохранение индекса...");
        indexer->saveIndex();
        
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
    // Регистрируем обработчик для сигнала SIGINT (Ctrl+C)
    signal(SIGINT, signalHandler);

    try {
        // ====== Инициализация ======
        Config config;
        std::string projectDir = (argc > 1) ? argv[1] : ".";

        if (!initializeApplication(projectDir, config)) {
            return 1;
        }

        // ====== Проверка совместимости с LLM сервером ======
        EmbeddingClient conn_checker(config);
        auto server_props_opt = conn_checker.fetchServerProperties();

        if (!server_props_opt) {
            SPDLOG_CRITICAL("Не удалось получить свойства с LLM сервера. Проверьте, что сервер запущен и доступен.");
            return 1;
        }
        if (!server_props_opt->embedding_enabled) {
            SPDLOG_CRITICAL("Сервер LLM запущен без поддержки эмбеддингов (флаг --embedding). Агент не может работать без этой функции. Завершение работы.");
            return 1;
        }

        // ====== Индексирование проекта ======
        auto indexer = indexProject(projectDir, config);
        if (!indexer) {
            return 1;
        }

        if (indexer->getEmbeddingsCount() == 0) {
            SPDLOG_WARN("Индексирование завершено, но эмбеддинги не получены. Возможно, все файлы были игнорированы.");
            std::cout << "\nВнимание: эмбеддинги не получены. Проверьте конфиг и сервер LLM.\n";
            return 1;
        }

        SPDLOG_INFO("Запуск Agent...");


        // ====== Создание ассистента ======
        auto assistant = std::make_shared<AssistantRole>(config);

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
        // ApiHandlers будет владеть сервером и управлять жизненным циклом WebSocketServer.
        ApiHandlers apiHandlers(config, *assistant, *indexer);
        
        // Сохраняем указатель для глобального доступа из обработчика сигналов
        g_apiHandlers = &apiHandlers;

        // Запуск сервера (этот вызов блокирует выполнение)
        apiHandlers.start(projectDir);

        // Небольшая задержка, чтобы дать время всем фоновым потокам (например, от httplib)
        // корректно завершиться после вызова stop().
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        SPDLOG_INFO("Server stopped.");
        std::cout << "\nСервер остановлен.\n";

    } catch (const std::exception& e) {
        SPDLOG_CRITICAL("Перехвачено необработанное исключение: {}", e.what());
        return 1;
    } catch (...) {
        SPDLOG_CRITICAL("Перехвачено неизвестное необработанное исключение!");
        return 1;
    }

    return 0;
}