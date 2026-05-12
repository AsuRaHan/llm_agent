#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <memory>

#include "EmbeddingClient.h"
#include "nlohmann/json.hpp"
#include "hnswlib/hnswlib.h" // Include HNSWLib
#include "CodeParser.h" // Include our new parser

struct Config; // Forward declaration

namespace fs = std::filesystem;

// Struct to hold a search result
struct SearchResult {
    std::string filePath;
    std::string chunkText;
    double score;
};

// Struct to hold chunk metadata (without embedding)
struct ChunkData {
    std::string text;
    size_t id; // ID used in HNSW index
};
struct FileRecord
{
    fs::file_time_type last_write_time;
    std::vector<ChunkData> chunks; // Now stores chunk metadata
};

class ContextIndexer
{
public:
    explicit ContextIndexer(const Config& config);
    ~ContextIndexer();

    void setIgnoredDirectories(const std::vector<std::string>& ignoredDirs);
    void setIgnoredExtensions(const std::vector<std::string>& ignoredExts);
    void indexDirectory(const fs::path& directoryPath);
    int getEmbeddingsCount() const;
    int getFileCount() const { return fileIndex.size(); }
    void saveIndex();
    std::vector<SearchResult> findTopK(const std::string& queryText, int k);

private:
    std::string readFileContent(const fs::path& path);
    void loadIndex();
    // cosineSimilarity is no longer needed for search, but can be useful for score calculation
    double cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    std::vector<std::string> fixedSizeChunkText(const std::string& text, size_t chunkSize, size_t overlap);
    void addChunk(const std::string& path, const std::string& text, const std::vector<float>& embedding);

    const Config& config;
    EmbeddingClient embeddingClient;
    std::unique_ptr<CodeParser> codeParser; // The parser instance
    std::unordered_set<std::string> ignoredDirectories;
    std::unordered_set<std::string> ignoredExtensions;
    std::unordered_set<std::string> ignoredFiles;
    std::unordered_map<std::string, FileRecord> fileIndex;

    // --- HNSWLib members ---
    const std::string hnswIndexPath = ".shdata/index.bin";
    const std::string metadataDbPath = ".shdata/index_meta.json";
    size_t embedding_dim = 0; // To be determined from the first embedding
    hnswlib::L2Space* space = nullptr;
    hnswlib::HierarchicalNSW<float>* index = nullptr;
    std::unordered_map<size_t, std::pair<std::string, std::string>> id_to_chunk_map; // map ID -> {filePath, chunkText}
    size_t current_max_elements = 0;
};
