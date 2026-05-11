#include "ContextIndexer.h"
#include "AssistantRole.h" // Include the new class
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <locale>

#ifdef _WIN32
#include <Windows.h>
#endif

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

    show_last_log_entries("ai_log.md");

    std::cout << "Запуск Agent..." << std::endl;

    ContextIndexer indexer;
    indexer.indexDirectory(".");

    if (indexer.getEmbeddingsCount() > 0)
    {
        std::cout << "\nСвязь с Llama.cpp установлена, получено эмбеддингов: " << indexer.getEmbeddingsCount() << std::endl;

        std::string query;
        while (true) {
            std::cout << "\nВведите поисковый запрос для нахождения наиболее похожего файла (или нажмите Enter для выхода): ";
            std::getline(std::cin, query);

            if (query.empty()) {
                break;
            }

            auto result = indexer.findMostSimilar(query);
            std::string filePath = result.first;
            std::string fileContent = result.second;

            if (fileContent.empty()) {
                std::cout << "Error or empty file: " << filePath << std::endl;
                continue;
            }

            std::cout << "Хочешь, чтобы я проанализировал этот файл? (y/n): ";
            std::string answer;
            std::getline(std::cin, answer);

            if (answer == "y" || answer == "Y") {
                AssistantRole assistant;
                std::string analysis = assistant.analyzeCode(filePath, fileContent, query);
                std::cout << "\n--- AI Analysis ---" << std::endl;
                std::cout << analysis << std::endl;
                std::cout << "---------------------\n" << std::endl;
            }
        }
    }

    std::cout << "\nAgent finished." << std::endl;

    return 0;
}