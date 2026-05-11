#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include "EmbeddingClient.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

// Struct to hold a search result
struct SearchResult {
    std::string filePath;
    std::string chunkText;
    double score;
};

// Struct to hold file metadata
struct Chunk {
    std::string text;
    std::vector<float> embedding;
};
struct FileRecord
{
    fs::file_time_type last_write_time;
    std::vector<Chunk> chunks; // Теперь тут список чанков
};

class ContextIndexer
{
public:
    ContextIndexer();
    ~ContextIndexer();

    void setIgnoredDirectories(const std::vector<std::string>& ignoredDirs);
    void setIgnoredExtensions(const std::vector<std::string>& ignoredExts);
    void indexDirectory(const fs::path& directoryPath);
    int getEmbeddingsCount() const;
    std::pair<std::string, std::string> findMostSimilar(const std::string& queryText);

private:
    std::string readFileContent(const fs::path& path);
    void loadIndex();
    void saveIndex();
    double cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    std::vector<std::string> chunkText(const std::string& text, size_t chunkSize = 1000, size_t overlap = 200);


    EmbeddingClient embeddingClient;
    std::unordered_set<std::string> ignoredDirectories;
    std::unordered_set<std::string> ignoredExtensions;
    std::unordered_map<std::string, FileRecord> fileIndex;
    int embeddings_count = 0;
    const std::string dbPath = "index.json";
};
