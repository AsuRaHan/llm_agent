#include "ContextIndexer.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include "Logger.h" // Include the new logger header

using json = nlohmann::json;
namespace fs = std::filesystem;

ContextIndexer::ContextIndexer()
{
    ignoredDirectories = { "build", ".git", ".vscode", "CMakeFiles" };
    ignoredFiles = { ".gitignore" };
    ignoredExtensions = {
        ".exe", ".obj", ".pdb", ".ilk", ".sln", ".vcxproj", ".filters", ".user",
        ".recipe", ".tlog", ".lastbuildstate", ".bin", ".stamp", ".cmake",
        ".json", // Ignore our own database
        ".log" // Ignore log files
    };
    SPDLOG_INFO("ContextIndexer инициализирован...");
    loadIndex();
    // After loading, calculate initial count
    resetEmbeddingsCount();
    for (const auto& [path, record] : fileIndex) {
        for (const auto& chunk : record.chunks) {
            if (!chunk.embedding.empty()) {
                incrementEmbeddingsCount();
            }
        }
    }
}

ContextIndexer::~ContextIndexer()
{
    saveIndex();
    SPDLOG_INFO("ContextIndexer уничтожен.");
}

void ContextIndexer::loadIndex()
{
    std::ifstream dbFile(dbPath);
    if (!dbFile.is_open())
    {
        SPDLOG_WARN("Индексный файл '{}' не найден. Будет создан новый.", dbPath);
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
        SPDLOG_ERROR("Error: Could not parse index file '{}'. Starting fresh. Error: {}", dbPath, e.what());
        fileIndex.clear(); // Start with a clean slate if JSON is corrupt
    }

    SPDLOG_INFO("Загружено {} записей из индекса.", fileIndex.size());
}

void ContextIndexer::saveIndex()
{
    std::ofstream dbFile(dbPath);
    if (!dbFile.is_open())
    {
        SPDLOG_ERROR("Error: Could not open index file '{}' for writing.", dbPath);
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
    // Dump with an error handler to replace invalid UTF-8 sequences
    dbFile << j.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
    SPDLOG_INFO("Сохранено {} записей в индекс.", fileIndex.size());
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

std::vector<SearchResult> ContextIndexer::findTopK(const std::string& queryText, int k)
{
    std::vector<float> queryEmbedding = embeddingClient.getEmbedding(queryText, "query");
    if (queryEmbedding.empty()) return {};

    if (k == 1) {
        double max_score = -1.0; // Cosine similarity is between -1 and 1
        std::string best_filePath = "";
        std::string best_chunkText = "";

        for (const auto& [path, record] : fileIndex) {
            for (const auto& chunk : record.chunks) {
                double sim = cosineSimilarity(queryEmbedding, chunk.embedding);
                if (sim > max_score) {
                    max_score = sim;
                    best_filePath = path;
                    best_chunkText = chunk.text;
                }
            }
        }
        if (max_score > -1.0) { // If a valid result was found
            return {{best_filePath, best_chunkText, max_score}};
        }
        return {};
    } else {
        std::vector<SearchResult> allResults;
        for (const auto& [path, record] : fileIndex) {
            for (const auto& chunk : record.chunks) {
                double sim = cosineSimilarity(queryEmbedding, chunk.embedding);
                allResults.push_back({path, chunk.text, sim});
            }
        }
        std::sort(allResults.begin(), allResults.end(), [](const SearchResult& a, const SearchResult& b) { return a.score > b.score; });
        if (allResults.size() > (size_t)k) { allResults.resize(k); }
        return allResults;
    }
}

std::pair<std::string, std::string> ContextIndexer::findMostSimilar(const std::string& queryText)
{
    auto topResults = findTopK(queryText, 1);
    if (topResults.empty()) {
        SPDLOG_WARN("Не удалось найти похожие файлы для запроса.");
        return {"", ""};
    }

    const auto& bestResult = topResults[0];
    SPDLOG_INFO("Наиболее похожий файл: {} (схожесть: {})", bestResult.filePath, bestResult.score);

    // The main loop expects the full file content, not just the chunk.
    std::string fileContent = readFileContent(bestResult.filePath);
    return { bestResult.filePath, fileContent };
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
        SPDLOG_ERROR("Не удалось открыть файл: {}", path.string());
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void ContextIndexer::indexDirectory(const fs::path& directoryPath)
{
    SPDLOG_INFO("\nStarting filtered indexing of directory: {}", directoryPath.string());
    auto iterator = fs::recursive_directory_iterator(directoryPath);

    std::unordered_set<std::string> files_in_index;
    for (const auto& [path, record] : fileIndex) {
        files_in_index.insert(path);
    }

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
            if (ignoredExtensions.count(path.extension().string()) ||
                ignoredFiles.count(path.filename().string()) ||
                path.filename() == dbPath)
            {
                continue;
            }
            
            auto canonicalPath = fs::weakly_canonical(path).string();
            std::replace(canonicalPath.begin(), canonicalPath.end(), '\\', '/');
            files_in_index.erase(canonicalPath); // Mark file as present on disk

            auto lastWriteTime = fs::last_write_time(path);

            auto it = fileIndex.find(canonicalPath);
            bool isNew = (it == fileIndex.end());
            bool isModified = !isNew && (it->second.last_write_time != lastWriteTime);

            if (isNew || isModified) {
                if(isNew) {
                    newFiles++;
                    SPDLOG_INFO("Обнаружен новый файл: {}", canonicalPath);
                } else {
                    updatedFiles++;
                    SPDLOG_INFO("Обнаружен изменённый файл: {}", canonicalPath);
                    for (const auto& chunk : it->second.chunks) {
                        if (!chunk.embedding.empty()) { decrementEmbeddingsCount(); }
                    }
                }

                std::string content = readFileContent(path);
                if (content.empty()) {
                    SPDLOG_DEBUG("  Skipping empty file: {}", canonicalPath);
                    continue;
                }
                // If it was a modified file, and now it's empty, we should remove it from index or mark it.
                // For now, let's just not add new chunks.
                if (!isNew && content.empty()) { fileIndex.erase(it); continue; }
                
                std::vector<std::string> textChunks = chunkText(content);
                std::vector<Chunk> indexedChunks;

                for (size_t i = 0; i < textChunks.size(); ++i) {
                    std::string chunkName = canonicalPath + " [#chunk " + std::to_string(i) + "]";
                    SPDLOG_DEBUG("  Получение эмбеддинга для чанка: {}", chunkName);
                    auto emb = embeddingClient.getEmbedding(textChunks[i], chunkName);
                    
                    if (!emb.empty()) {
                        indexedChunks.push_back({ textChunks[i], emb });
                        incrementEmbeddingsCount(); // Increment for new valid chunk
                    }
                    // Если программа падает, это сообщение не появится в логе
                }

                if (!indexedChunks.empty()) {
                    fileIndex[canonicalPath] = { lastWriteTime, indexedChunks };
                } else {
                    // If we can't get an embedding for any chunk, remove the file from index if it was existing,
                    // or add it with empty chunks if it's new.
                    if (!isNew) { fileIndex.erase(it); }
                    else { fileIndex[canonicalPath] = { lastWriteTime, {} }; }
                }
            }
        }
    }

    int deletedFiles = 0;
    for (const auto& deleted_path : files_in_index) {
        auto it = fileIndex.find(deleted_path);
        if (it != fileIndex.end()) {
            for (const auto& chunk : it->second.chunks) {
                if (!chunk.embedding.empty()) { decrementEmbeddingsCount(); }
            }
            fileIndex.erase(it);
            deletedFiles++;
            SPDLOG_INFO("Удален из индекса отсутствующий файл: {}", deleted_path);
        }
    }

    SPDLOG_INFO("Завершено индексирование. Новых файлов: {}, Изменённых файлов: {}, Удалено файлов: {}",
                newFiles, updatedFiles, deletedFiles);
}
