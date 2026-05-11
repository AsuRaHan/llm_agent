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
#include "Logger.h" // Include the new logger header

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


int main(int argc, char* argv[])
{
    setlocale(LC_ALL, ".UTF8");
#ifdef _WIN32
    // Set console to UTF-8 to correctly display Cyrillic characters from AI responses
    // This is a more reliable method on Windows than just setlocale.
    SetConsoleOutputCP(CP_UTF8);
#endif

    init_logger("app.log"); // Initialize the logger
    // show_last_log_entries("app.log"); // Now reads from the application log file
    
    try {
        SPDLOG_INFO("Запуск Agent...");

        ContextIndexer indexer;
        indexer.indexDirectory(".");

        if (indexer.getEmbeddingsCount() > 0)
        {
            SPDLOG_INFO("\nСвязь с Llama.cpp установлена, получено эмбеддингов: {}", indexer.getEmbeddingsCount());

            AssistantRole assistant; // Create assistant once
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