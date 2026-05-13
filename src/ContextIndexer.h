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

struct AssistantRole;
struct Config; // Forward declaration

namespace fs = std::filesystem;

// Struct to hold a search result
struct SearchResult {
    std::string filePath;
    std::string chunkText;
    double score;
};

// Points to a specific location of a chunk within a file
struct ChunkLocation {
    size_t start_byte;
    size_t length;
};

// Stores information about an indexed chunk
struct ChunkInfo {
    size_t id; // ID used in HNSW index
    ChunkLocation location;
};
struct FileRecord
{
    fs::file_time_type last_write_time;
    std::vector<ChunkInfo> chunks;
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
    std::string readChunkContent(const std::string& path, const ChunkLocation& location);
    void loadIndex();
    // cosineSimilarity is no longer needed for search, but can be useful for score calculation
    double cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    std::vector<ChunkLocation> fixedSizeChunkText(const std::string& text, size_t chunkSize, size_t overlap);
    void addChunk(const std::string& path, const std::string& text, const std::vector<float>& embedding, const ChunkLocation& location);

    const Config& config;
    EmbeddingClient embeddingClient;
    std::unique_ptr<CodeParser> codeParser; // The parser instance
    std::unique_ptr<AssistantRole> summarizerAssistant;
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
    std::unordered_map<size_t, std::pair<std::string, ChunkLocation>> id_to_chunk_map; // map ID -> {filePath, location}
    size_t current_max_elements = 0;
};
