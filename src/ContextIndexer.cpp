#include "ContextIndexer.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <numeric>
#include <cmath>
#include <algorithm>

using json = nlohmann::json;
namespace fs = std::filesystem;

ContextIndexer::ContextIndexer()
{
    ignoredDirectories = { "build", ".git", ".vscode", "CMakeFiles" };
    ignoredExtensions = {
        ".exe", ".obj", ".pdb", ".ilk", ".sln", ".vcxproj", ".filters", ".user",
        ".recipe", ".tlog", ".lastbuildstate", ".bin", ".stamp", ".cmake",
        ".json" // Ignore our own database
    };
    std::cout << "ContextIndexer инициализирован..." << std::endl;
    loadIndex();
}

ContextIndexer::~ContextIndexer()
{
    saveIndex();
    std::cout << "ContextIndexer уничтожен." << std::endl;
}

void ContextIndexer::loadIndex()
{
    std::ifstream dbFile(dbPath);
    if (!dbFile.is_open())
    {
        std::cout << "Индексный файл '" << dbPath << "' не найден. Будет создан новый." << std::endl;
        return;
    }

    json j;
    try {
        dbFile >> j;
        for (auto& [path, value] : j.items()) {
            long long time_count = value["last_write_time"];
            std::vector<float> embedding = value["embedding"];

            auto duration_since_epoch = fs::file_time_type::duration(time_count);
            fileIndex[path] = { fs::file_time_type(duration_since_epoch), embedding };
        }
    }
    catch (json::parse_error& e) {
        std::cerr << "Error: Could not parse index file '" << dbPath << "'. Starting fresh. Error: " << e.what() << std::endl;
        fileIndex.clear(); // Start with a clean slate if JSON is corrupt
    }

    std::cout << "Загружено " << fileIndex.size() << " записей из индекса." << std::endl;
}

void ContextIndexer::saveIndex()
{
    std::ofstream dbFile(dbPath);
    if (!dbFile.is_open())
    {
        std::cerr << "Error: Could not open index file '" << dbPath << "' for writing." << std::endl;
        return;
    }

    json j;
    for (const auto& [path, record] : fileIndex)
    {
        j[path] = {
            {"last_write_time", record.last_write_time.time_since_epoch().count()},
            {"embedding", record.embedding}
        };
    }
    dbFile << j.dump(4); // Pretty-print with 4 spaces
    std::cout << "Сохранено " << fileIndex.size() << " записей в индекс." << std::endl;
}

double ContextIndexer::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) {
        return 0.0;
    }

    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    double norm_a_sqrt = std::sqrt(norm_a);
    double norm_b_sqrt = std::sqrt(norm_b);

    if (norm_a_sqrt == 0.0 || norm_b_sqrt == 0.0) {
        return 0.0;
    }

    return dot_product / (norm_a_sqrt * norm_b_sqrt);
}


std::pair<std::string, std::string> ContextIndexer::findMostSimilar(const std::string& queryText) {
    if (fileIndex.empty()) {
        return { "Индекс пуст. Нет файлов для сравнения.", "" };
    }

    std::vector<float> queryEmbedding = embeddingClient.getEmbedding(queryText, "query");
    if (queryEmbedding.empty()) {
        return { "Не удалось сгенерировать embedding для запроса.", "" };
    }

    std::string bestMatchPath = "";
    double maxSimilarity = -1.0;

    for (const auto& [path, record] : fileIndex) {
        if (record.embedding.empty()) {
            continue;
        }

        double similarity = cosineSimilarity(queryEmbedding, record.embedding);
        if (similarity > maxSimilarity) {
            maxSimilarity = similarity;
            bestMatchPath = path;
        }
    }

    if (bestMatchPath.empty()) {
        return { "Не удалось найти похожие файлы.", "" };
    }

    std::cout << "Лучшее совпадение: '" + bestMatchPath + "' с коэффициентом сходства: " + std::to_string(maxSimilarity) << std::endl;

    std::string content = readFileContent(bestMatchPath);
    return { bestMatchPath, content };
}

void ContextIndexer::setIgnoredDirectories(const std::vector<std::string>& ignoredDirs)
{
    ignoredDirectories.clear();
    for (const auto& dir : ignoredDirs)
    {
        ignoredDirectories.insert(dir);
    }
}

void ContextIndexer::setIgnoredExtensions(const std::vector<std::string>& ignoredExts)
{
    ignoredExtensions.clear();
    for (const auto& ext : ignoredExts)
    {
        ignoredExtensions.insert(ext);
    }
}

std::string ContextIndexer::readFileContent(const fs::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        std::cerr << "Не удалось открыть файл: " << path.string() << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int ContextIndexer::getEmbeddingsCount() const {
    // Return the total number of items in the index that have a non-empty embedding vector.
    return std::count_if(fileIndex.begin(), fileIndex.end(), [](const auto& pair) {
        return !pair.second.embedding.empty();
    });
}

void ContextIndexer::indexDirectory(const fs::path& directoryPath)
{
    std::cout << "\nStarting filtered indexing of directory: " << directoryPath.string() << std::endl;
    auto iterator = fs::recursive_directory_iterator(directoryPath);
    int updatedFiles = 0;
    int newFiles = 0;

    for (const auto& entry : iterator)
    {
        if (entry.is_directory() && ignoredDirectories.count(entry.path().filename().string()))
        {
            iterator.disable_recursion_pending();
            continue;
        }

        if (entry.is_regular_file())
        {
            const auto& path = entry.path();
            if (ignoredExtensions.count(path.extension().string()) || path.filename() == dbPath)
            {
                continue;
            }
            
            auto canonicalPath = fs::weakly_canonical(path).string();
            std::replace(canonicalPath.begin(), canonicalPath.end(), '\\', '/');
            auto lastWriteTime = fs::last_write_time(path);

            auto it = fileIndex.find(canonicalPath);
            bool isNew = (it == fileIndex.end());
            bool isModified = !isNew && (it->second.last_write_time != lastWriteTime);

            if (isNew || isModified) {
                if(isNew) {
                    newFiles++;
                    std::cout << "Обнаружен новый файл: " << canonicalPath << std::endl;
                } else {
                    updatedFiles++;
                    std::cout << "Обнаружен изменённый файл: " << canonicalPath << std::endl;
                }

                std::string content = readFileContent(path);
                if (content.empty()) {
                    std::cout << "  Skipping empty file." << std::endl;
                    continue;
                }

                auto embedding = embeddingClient.getEmbedding(content, canonicalPath);
                std::cout << "  Got embedding (size " << embedding.size() << ")" << std::endl;
                
                if (!embedding.empty()) {
                     fileIndex[canonicalPath] = { lastWriteTime, embedding };
                } else {
                    // If we can't get an embedding, still record the file, but with an empty vector
                    fileIndex[canonicalPath] = { lastWriteTime, {} };
                }
            }
        }
    }
    embeddings_count = getEmbeddingsCount();
    std::cout << "Завершено индексирование. Новых файлов: " << newFiles << ", Изменённых файлов: " << updatedFiles << std::endl;
}

