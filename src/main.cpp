// Per README instructions, to prevent conflicts with cpp-httplib
#ifdef _WIN32
#include <Windows.h>
#endif

#include "ContextIndexer.h"
#include "AssistantRole.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <locale>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include "Logger.h"
#include "Config.h"
#include "FileWatcher.h"

namespace fs = std::filesystem;

// ============================================================================
// Utility Functions
// ============================================================================

void show_last_log_entries(const std::string& filePath, int linesToShow = 15)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    std::deque<std::string> lastLines;
    std::string line;

    while (std::getline(file, line)) {
        lastLines.push_back(line);
        if (lastLines.size() > linesToShow) {
            lastLines.pop_front();
        }
    }

    std::cout << "\n--- Последние " << lastLines.size() << " записей логов ---" << std::endl;
    for (const auto& l : lastLines) {
        std::cout << l << std::endl;
    }
    std::cout << "--------------------------\n" << std::endl;
}

void clear_screen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void print_separator(char ch = '=', int width = 80)
{
    for (int i = 0; i < width; ++i) {
        std::cout << ch;
    }
    std::cout << std::endl;
}

// ============================================================================
// Initialization Functions
// ============================================================================

/// Инициализирует приложение: логирование, конфиг, рабочий каталог
/// Возвращает true при успехе, false при ошибке
bool initializeApplication(const std::string& projectDir, Config& outConfig)
{
    setlocale(LC_ALL, ".UTF8");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8); // Устанавливаем кодовую страницу для ВВОДА
#endif

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
// Interface Functions
// ============================================================================

/// Получает кроссплатформенный ввод с подсказкой
std::string getUTF8Input(const std::string& prompt)
{
    std::cout << prompt;
    std::cout.flush();
    
#ifdef _WIN32
    // Используем Windows-специфичный API для надежного ввода UTF-8,
    // так как std::cin может некорректно работать с кодировками в консоли Windows.
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput == INVALID_HANDLE_VALUE) {
        return "";
    }

    wchar_t buffer[1024];
    DWORD charsRead = 0;
    if (ReadConsoleW(hInput, buffer, sizeof(buffer)/sizeof(wchar_t) - 1, &charsRead, NULL)) {
        // Обрезаем символы новой строки в конце
        if (charsRead > 0 && buffer[charsRead - 1] == L'\n') {
            charsRead--;
        }
        if (charsRead > 0 && buffer[charsRead - 1] == L'\r') {
            charsRead--;
        }
        buffer[charsRead] = L'\0';

        // Конвертируем wide string (UTF-16) в UTF-8
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
        if (utf8_len > 0) {
            std::string utf8_str(utf8_len - 1, 0); // -1 для исключения null-терминатора
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &utf8_str[0], utf8_len, NULL, NULL);
            return utf8_str;
        }
    }
    return "";
#else
    // Стандартная реализация для платформ, отличных от Windows
    std::string input;
    if (std::getline(std::cin, input)) {
        return input;
    }
    return "";
#endif
}

/// Выводит главное меню и возвращает выбор пользователя (1-4)
int showMainMenu()
{
    std::cout << "\n";
    print_separator('=', 60);
    std::cout << "  ГЛАВНОЕ МЕНЮ\n";
    print_separator('=', 60);
    std::cout << "  1. Задать вопрос о проекте\n";
    std::cout << "  2. Информация о проекте\n";
    std::cout << "  3. Справка\n";
    std::cout << "  4. Выход\n";
    print_separator('-', 60);
    
    while (true) {
        std::string choice = getUTF8Input("Выберите опцию (1-4): ");
        
        if (choice.length() == 1 && choice[0] >= '1' && choice[0] <= '4') {
            return choice[0] - '0';
        }
        
        std::cout << "Ошибка: введите число от 1 до 4.\n";
    }
}

/// Выводит справку
void showHelp()
{
    clear_screen();
    print_separator('=', 60);
    std::cout << "  СПРАВКА\n";
    print_separator('=', 60);
    std::cout << "\nЭто консольное приложение - локальный AI-ассистент для анализа кода.\n";
    std::cout << "\nВозможности:\n";
    std::cout << "  • Индексирование кодовой базы с помощью Tree-sitter\n";
    std::cout << "  • Поиск релевантного контекста через HNSW\n";
    std::cout << "  • Ответы на вопросы о проекте через локальный LLM\n";
    std::cout << "\nОпции меню:\n";
    std::cout << "  1 - Введите вопрос, и агент найдет релевантный код\n";
    std::cout << "  2 - Просмотрите информацию об индексировании проекта\n";
    std::cout << "  3 - Эта справка\n";
    std::cout << "  4 - Завершить работу\n";
    std::cout << "\n";
    getUTF8Input("Нажмите Enter для возврата в меню... ");
}

/// Выводит приветствие и информацию о проекте
void displayProjectSummary(AssistantRole& assistant, size_t fileCount, size_t embeddingsCount)
{
    clear_screen();
    print_separator('=', 60);
    std::cout << "  ИНФОРМАЦИЯ О ПРОЕКТЕ\n";
    print_separator('=', 60);
    
    SPDLOG_DEBUG("Связь с LLM установлена, получено эмбеддингов: {}", embeddingsCount);
    
    std::string greeting = assistant.generateProjectSummaryGreeting(fileCount, embeddingsCount);
    
    std::cout << "\n" << greeting << "\n\n";
    // std::cout << "Статистика индексирования:\n";
    // std::cout << "  • Обработано файлов: " << fileCount << "\n";
    // std::cout << "  • Создано эмбеддингов: " << embeddingsCount << "\n";
    print_separator('-', 60);
    
    getUTF8Input("Нажмите Enter для возврата в меню... ");
}

/// Обрабатывает вопрос пользователя
void processUserQuery(const std::string& query, ContextIndexer& indexer, 
                      AssistantRole& assistant, const Config& config)
{
    if (query.empty()) {
        return;
    }

    SPDLOG_DEBUG("Ищу релевантный контекст в проекте...");
    std::cout << "\n[*] Обработка запроса...\n";
    
    auto topResults = indexer.findTopK(query, config.top_k_results);

    if (topResults.empty()) {
        std::cout << "\nК сожалению, релевантной информации не найдено.\n";
        SPDLOG_WARN("По запросу '{}' не найдено релевантной информации.", query);
        return;
    }

    SPDLOG_DEBUG("Топ-{} релевантных чанков, переданных в контекст:", topResults.size());
    for (size_t i = 0; i < topResults.size(); ++i) {
        const auto& res = topResults[i];
        std::string text_preview = res.chunkText.substr(0, 100);
        text_preview.erase(std::remove(text_preview.begin(), text_preview.end(), '\n'), text_preview.end());
        SPDLOG_DEBUG("  - Чанк #{}: score={:.3f}, file='{}'", i + 1, res.score, res.filePath);
    }

    // Сбор уникальных источников
    std::string sources_str;
    std::unordered_set<std::string> unique_sources;
    for (const auto& res : topResults) {
        unique_sources.insert(fs::path(res.filePath).filename().string());
    }
    for (const auto& src : unique_sources) {
        if (!sources_str.empty()) sources_str += ", ";
        sources_str += src;
    }
    
    SPDLOG_DEBUG("Использую контекст из файлов: {}", sources_str);
    std::cout << "Контекст из: " << sources_str << "\n\n";

    AssistantResponse response = assistant.processQuery(query, topResults, indexer);
    
    print_separator('~', 60);
    std::cout << response.text << "\n";
    print_separator('~', 60);
    
    SPDLOG_DEBUG("Ответ на запрос: {}", response.text);

    // --- Conversational Loop ---
    while (!response.is_final) {
        std::cout << "\n";
        std::string follow_up_query = getUTF8Input("> ");
        if (follow_up_query.empty() || follow_up_query == "exit" || follow_up_query == "выход") {
            break;
        }
        std::cout << "\n[*] Обработка продолжения...\n";
        response = assistant.processQuery(follow_up_query, {}, indexer, response.conversation_history);

        print_separator('~', 60);
        std::cout << response.text << "\n";
        print_separator('~', 60);
        SPDLOG_DEBUG("Ответ на продолжение: {}", response.text);
    }
}
// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char* argv[])
{
    try {
        // ====== Инициализация ======
        Config config;
        std::string projectDir = (argc > 1) ? argv[1] : ".";

        if (!initializeApplication(projectDir, config)) {
            // Конфиг не загружен, используем путь по умолчанию
            // show_last_log_entries("agent.log", 30);
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
            // show_last_log_entries(config.log_file_path, 30);
            return 1;
        }

        if (indexer->getEmbeddingsCount() == 0) {
            SPDLOG_WARN("Индексирование завершено, но эмбеддинги не получены. Возможно, все файлы были игнорированы.");
            std::cout << "\nВнимание: эмбеддинги не получены. Проверьте конфиг и сервер LLM.\n";
            return 1;
        }

        SPDLOG_INFO("Запуск Agent...");

        // ====== Создание ассистента ======
        AssistantRole assistant(config);

        // ====== Запуск мониторинга файлов ======
        FileWatcher watcher(*indexer);
        watcher.start(projectDir);
        // ====== Главный цикл приложения ======
        clear_screen();
        
        bool running = true;
        while (running) {
            int choice = showMainMenu();

            switch (choice) {
                case 1: {
                    // Задать вопрос о проекте
                    std::cout << "\n";
                    std::string userQuery = getUTF8Input("Введите ваш вопрос:\n> ");
                    // SPDLOG_DEBUG("Получен запрос от пользователя: '{}'", userQuery);
                    if (!userQuery.empty()) {
                        processUserQuery(userQuery, *indexer, assistant, config);
                    } else {
                        std::cout << "\nВведите не пустой вопрос.\n";
                    }
                    // getUTF8Input("\nНажмите Enter для продолжения... ");
                    break;
                }
                case 2: {
                    // Информация о проекте
                    displayProjectSummary(assistant, indexer->getFileCount(), indexer->getEmbeddingsCount());
                    break;
                }
                case 3: {
                    // Справка
                    showHelp();
                    break;
                }
                case 4: {
                    // Выход
                    running = false;
                    break;
                }
            }
        }

        SPDLOG_INFO("Остановка файлового мониторинга...");
        watcher.stop();
        SPDLOG_INFO("Agent завершил работу.");
        clear_screen();
        std::cout << "\n";
        print_separator('=', 60);
        std::cout << "  До свидания! Спасибо за использование Smart Hammer.\n";
        print_separator('=', 60);
        std::cout << "\n";

    } catch (const std::exception& e) {
        SPDLOG_CRITICAL("Перехвачено необработанное исключение: {}", e.what());
        // show_last_log_entries("agent.log", 30); // config может быть недоступен
        return 1;
    } catch (...) {
        SPDLOG_CRITICAL("Перехвачено неизвестное необработанное исключение!");
        // show_last_log_entries("agent.log", 30); // config может быть недоступен
        return 1;
    }

    return 0;
}