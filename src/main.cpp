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

        if (indexer.getEmbeddingsCount() > 0)
        {
            SPDLOG_INFO("\nСвязь с Llama.cpp установлена, получено эмбеддингов: {}", indexer.getEmbeddingsCount());

            AssistantRole assistant(config); // Create assistant once

            // Generate and display a creative greeting from the agent
            std::string greeting = assistant.generateProjectSummaryGreeting(indexer.getFileCount(), indexer.getEmbeddingsCount());
            SPDLOG_INFO("\n--- Сообщение от Агента ---");
            SPDLOG_INFO("{}", greeting);
            SPDLOG_INFO("---------------------------\n");

            std::string query;
            while (true) { // Main loop
                SPDLOG_INFO("\nВведите поисковый запрос для нахождения наиболее похожего файла (или нажмите Enter для выхода): ");
                std::getline(std::cin, query);

                if (query.empty()) {
                    break;
                }

                auto result = indexer.findMostSimilar(query);
                std::string filePath = result.first;
                std::string fileContent = result.second;

                if (fileContent.empty()) {
                    SPDLOG_ERROR("Error or empty file: {}", filePath);
                    continue;
                }

                SPDLOG_INFO("Хочешь, чтобы я проанализировал этот файл? (y/n): ");
                std::string answer;
                std::getline(std::cin, answer);

                if (answer == "y" || answer == "Y") {
                    std::string analysis = assistant.analyzeCode(filePath, fileContent, query);
                    SPDLOG_INFO("\n--- AI Analysis ---");
                    SPDLOG_INFO("{}", analysis);
                    SPDLOG_INFO("---------------------\n");
                }
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