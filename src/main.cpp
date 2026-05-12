// Per README instructions, to prevent conflicts with cpp-httplib
#ifdef _WIN32
#include <Windows.h>
#endif

#include "ContextIndexer.h"
#include "AssistantRole.h" // Include the new class
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <locale>
#include <filesystem> // For fs::current_path
#include "Logger.h" // Include the new logger header
#include "Config.h" // Include the new config header

namespace fs = std::filesystem;

void show_last_log_entries(const std::string& filePath, int linesToShow = 15)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return;
    }

    std::deque<std::string> lastLines;
    std::string line;

    while (std::getline(file, line))
    {
        lastLines.push_back(line);
        if (lastLines.size() > linesToShow)
        {
            lastLines.pop_front();
        }
    }

    std::cout << "\n--- Последние " << lastLines.size() << " записей логов ---" << std::endl;
    for (const auto& l : lastLines)
    {
        std::cout << l << std::endl;
    }
    std::cout << "--------------------------\n" << std::endl;
}


int main(int argc, char* argv[]) // Modified main signature
{
    setlocale(LC_ALL, ".UTF8");
#ifdef _WIN32
    // Set console to UTF-8 to correctly display Cyrillic characters from AI responses
    // This is a more reliable method on Windows than just setlocale.
    SetConsoleOutputCP(CP_UTF8);
#endif

    init_logger("app.log"); // Initialize the logger
    // show_last_log_entries("app.log"); // Now reads from the application log file
    
    Config config;
    if (!config.load("config.json")) {
        SPDLOG_CRITICAL("Не удалось загрузить конфигурацию. Завершение работы.");
        return 1;
    }

    std::string project_dir_str = "."; // Default to current directory
    if (argc > 1) {
        project_dir_str = argv[1];
        // Change current working directory if specified
        try {
            fs::current_path(project_dir_str);
            SPDLOG_INFO("Рабочий каталог изменен на: {}", fs::current_path().string());
        } catch (const fs::filesystem_error& e) {
            SPDLOG_CRITICAL("Ошибка при смене рабочего каталога на '{}': {}", project_dir_str, e.what());
            return 1;
        }
    } else {
        SPDLOG_INFO("Рабочий каталог по умолчанию: {}", fs::current_path().string());
    }

    try {
        // Check LLM server connection before proceeding
        EmbeddingClient temp_embedding_client_for_check(config); // Create a temporary client for connection check
        if (!temp_embedding_client_for_check.checkConnection()) {
            SPDLOG_CRITICAL("Не удалось подключиться к серверу LLM. Завершение работы.");
            return 1;
        }
        SPDLOG_INFO("Запуск Agent...");

        ContextIndexer indexer(config);
        indexer.indexDirectory(project_dir_str); // Pass the determined project_dir_str

        // Explicitly save the index after all indexing operations are complete.
        // This ensures data is safe before the interactive part begins.
        SPDLOG_INFO("Индексирование завершено. Сохранение индекса...");
        indexer.saveIndex();

        if (indexer.getEmbeddingsCount() > 0)
        {
            SPDLOG_INFO("\nСвязь с Llama.cpp установлена, получено эмбеддингов: {}", indexer.getEmbeddingsCount());

            AssistantRole assistant(config); // Create assistant once

            // Generate and display a creative greeting from the agent
            std::string greeting = assistant.generateProjectSummaryGreeting(indexer.getFileCount(), indexer.getEmbeddingsCount());
            SPDLOG_INFO("\n--- Сообщение от Агента ---");
            SPDLOG_INFO("{}", greeting);
            SPDLOG_INFO("---------------------------\n");

#ifdef _WIN32
            // --- Windows-specific robust input loop using WinAPI ---
            // This avoids the instability of std::cin with UTF-8 console mode.
            wchar_t buffer[4096];
            DWORD charsRead;
            HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);

            if (hInput == INVALID_HANDLE_VALUE) {
                SPDLOG_CRITICAL("Не удалось получить хэндл стандартного ввода (stdin). Error: {}", GetLastError());
                return 1;
            }

            while (true) {
                SPDLOG_INFO("\nВведите ваш вопрос о проекте (или 'exit' для выхода): ");
                
                if (!ReadConsoleW(hInput, buffer, sizeof(buffer)/sizeof(wchar_t) - 1, &charsRead, NULL)) {
                    SPDLOG_ERROR("Ошибка чтения из консоли (ReadConsoleW). Error: {}. Завершение работы.", GetLastError());
                    break;
                }

                // Null-terminate and remove trailing \r\n
                if (charsRead > 0) {
                    if (charsRead >= 2 && buffer[charsRead - 2] == L'\r' && buffer[charsRead - 1] == L'\n') {
                        buffer[charsRead - 2] = L'\0';
                    } else if (charsRead >= 1 && buffer[charsRead - 1] == L'\n') {
                         buffer[charsRead - 1] = L'\0';
                    } else {
                        buffer[charsRead] = L'\0';
                    }
                } else {
                    buffer[0] = L'\0';
                }

                // Convert wide string to UTF-8 std::string
                std::string query;
                std::wstring wquery(buffer);
                if (!wquery.empty()) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wquery[0], (int)wquery.size(), NULL, 0, NULL, NULL);
                    query.resize(size_needed);
                    WideCharToMultiByte(CP_UTF8, 0, &wquery[0], (int)wquery.size(), &query[0], size_needed, NULL, NULL);
                }

                if (query == "exit") {
                    break;
                }
                if (query.empty()) {
                    continue;
                }
#else
            // --- Standard C++ input loop for non-Windows platforms ---
            std::string query;
            SPDLOG_INFO("\nВведите ваш вопрос о проекте (или 'exit' для выхода): ");
            while (std::getline(std::cin, query)) {
                if (query == "exit") {
                    break;
                }
                if (query.empty()) {
                    SPDLOG_INFO("\nВведите ваш вопрос о проекте (или 'exit' для выхода): ");
                    continue;
                }
#endif
                SPDLOG_INFO("Ищу релевантный контекст в проекте...");
                auto topResults = indexer.findTopK(query, config.top_k_results);

                if (topResults.empty()) {
                    SPDLOG_WARN("К сожалению, я не смог найти релевантной информации по вашему запросу.");
                    continue;
                }

                // Log the files being used for context
                std::string sources_str;
                std::unordered_set<std::string> unique_sources;
                for(const auto& res : topResults) {
                    unique_sources.insert(fs::path(res.filePath).filename().string());
                }
                for(const auto& src : unique_sources) {
                    if (!sources_str.empty()) sources_str += ", ";
                    sources_str += src;
                }
                SPDLOG_INFO("Использую контекст из файлов: {}", sources_str);

                std::string analysis = assistant.answerWithContext(query, topResults);
                SPDLOG_INFO("\n--- Ответ Агента ---");
                SPDLOG_INFO("{}", analysis);
                SPDLOG_INFO("---------------------\n");
#ifndef _WIN32
                SPDLOG_INFO("\nВведите ваш вопрос о проекте (или 'exit' для выхода): ");
#endif
            }
        }
        else {
            SPDLOG_WARN("Индексация завершена, но не получено ни одного эмбеддинга. Возможно, сервер LLM не вернул эмбеддинги или все файлы были проигнорированы.");
        }

        SPDLOG_INFO("\nAgent finished.");
    }
    catch (const std::exception& e) {
        SPDLOG_CRITICAL("Перехвачено необработанное исключение: {}", e.what());
        show_last_log_entries("app.log", 30);
        return 1;
    }
    catch (...) {
        SPDLOG_CRITICAL("Перехвачено неизвестное необработанное исключение!");
        show_last_log_entries("app.log", 30);
        return 1;
    }

    return 0;
}