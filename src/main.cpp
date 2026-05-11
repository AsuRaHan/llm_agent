#include "ContextIndexer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <locale>

#ifdef _WIN32
#include <Windows.h>
#endif

/**
 * @brief Reads the last N lines from a file and displays them.
 * 
 * @param filePath The path to the file.
 * @param linesToShow The number of lines to display from the end of the file.
 */
void show_last_log_entries(const std::string& filePath, int linesToShow = 15)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        // Don't print an error if the file doesn't exist on the first run
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

    std::cout << "\n--- Last " << lastLines.size() << " Log Entries ---" << std::endl;
    for (const auto& l : lastLines)
    {
        std::cout << l << std::endl;
    }
    std::cout << "--------------------------\n" << std::endl;
}


int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::locale::global(std::locale(""));
    std::cout.imbue(std::locale());

    show_last_log_entries("ai_log.md");

    std::cout << "Starting Agent..." << std::endl;

    ContextIndexer indexer;

    // Example of how to customize ignores (optional, defaults are set in constructor)
    // indexer.setIgnoredDirectories({ "build", ".git", "docs" });
    // indexer.setIgnoredExtensions({ ".log", ".tmp" });

    indexer.indexDirectory(".");

    if (indexer.getEmbeddingsCount() > 0)
    {
        std::cout << "\nСвязь с Llama.cpp установлена, получено эмбеддингов: " << indexer.getEmbeddingsCount() << std::endl;

        std::string query;
        std::cout << "\nEnter a search query to find the most similar file (or press Enter to skip): ";
        std::getline(std::cin, query);

        if (!query.empty()) {
            std::string result = indexer.findMostSimilar(query);
            std::cout << result << std::endl;
        }
    }

    std::cout << "\nAgent finished." << std::endl;

    return 0;
}