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
            auto duration_since_epoch = fs::file_time_type::duration(time_count);
            
            std::vector<Chunk> chunks;
            if (value.contains("chunks")) {
                for(const auto& chunk_json : value["chunks"]) {
                    chunks.push_back({
                        chunk_json["text"],
                        chunk_json["embedding"]
                    });
                }
            }
            fileIndex[path] = { fs::file_time_type(duration_since_epoch), chunks };
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
        json chunks_json = json::array();
        for(const auto& chunk : record.chunks) {
            chunks_json.push_back({
                {"text", chunk.text},
                {"embedding", chunk.embedding}
            });
        }

        j[path] = {
            {"last_write_time", record.last_write_time.time_since_epoch().count()},
            {"chunks", chunks_json}
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

std::vector<std::string> ContextIndexer::chunkText(const std::string& text, size_t chunkSize, size_t overlap) {
    std::vector<std::string> chunks;
    if (text.empty()) return chunks;

    size_t start = 0;
    while (start < text.length()) {
        size_t end = std::min(start + chunkSize, text.length());
        chunks.push_back(text.substr(start, end - start));
        
        if (end == text.length()) break;
        start += (chunkSize - overlap); // Сдвигаемся с учетом нахлеста
    }
    return chunks;
}

std::vector<SearchResult> ContextIndexer::findTopK(const std::string& queryText, int k) {
    std::vector<SearchResult> allResults;
    
    std::vector<float> queryEmbedding = embeddingClient.getEmbedding(queryText, "query");
    if (queryEmbedding.empty()) return {};

    for (const auto& [path, record] : fileIndex) {
        for (const auto& chunk : record.chunks) {
            double sim = cosineSimilarity(queryEmbedding, chunk.embedding);
            allResults.push_back({path, chunk.text, sim});
        }
    }

    // Сортируем по убыванию сходства
    std::sort(allResults.begin(), allResults.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    // Берем только первые K
    if (allResults.size() > (size_t)k) {
        allResults.resize(k);
    }

    return allResults;
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
    int count = 0;
    for (const auto& pair : fileIndex) {
        count += std::count_if(pair.second.chunks.begin(), pair.second.chunks.end(), [](const auto& chunk){
            return !chunk.embedding.empty();
        });
    }
    return count;
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
                
                std::vector<std::string> textChunks = chunkText(content);
                std::vector<Chunk> indexedChunks;

                for (size_t i = 0; i < textChunks.size(); ++i) {
                    std::string chunkName = canonicalPath + " [#chunk " + std::to_string(i) + "]";
                    auto emb = embeddingClient.getEmbedding(textChunks[i], chunkName);
                    
                    if (!emb.empty()) {
                        indexedChunks.push_back({ textChunks[i], emb });
                    }
                }

                if (!indexedChunks.empty()) {
                    fileIndex[canonicalPath] = { lastWriteTime, indexedChunks };
                } else {
                    // If we can't get an embedding, still record the file, but with an empty vector of chunks
                    fileIndex[canonicalPath] = { lastWriteTime, {} };
                }
            }
        }
    }
    embeddings_count = getEmbeddingsCount();
    std::cout << "Завершено индексирование. Новых файлов: " << newFiles << ", Изменённых файлов: " << updatedFiles << std::endl;
}

