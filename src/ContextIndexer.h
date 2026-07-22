#pragma once

#include "Config.h"
#include "EmbeddingClient.h"
#include "ContextIndexerHelper/IndexManager.h"
#include "ContextIndexerHelper/FileIndexer.h"
#include "ContextIndexerHelper/Searcher.h"
#include "LLMProvider.h"
#include <memory>
#include <filesystem>

class ContextIndexer {
public:
    ContextIndexer(std::shared_ptr<LLMProvider> provider, 
                   const Config& config,
                   const std::string& index_meta_path,
                   const std::string& index_bin_path,
                   const std::string& file_indexer_meta_path);
    ~ContextIndexer();

    void indexDirectory(const std::filesystem::path& directoryPath);
    bool isPathIgnored(const std::filesystem::path& path) const;
    void reindexFile(const std::string& path);
    void removeFileFromIndex(const std::string& path);

    void saveIndex();
    void loadIndex();

    int getEmbeddingsCount() const;
    int getFileCount() const;

    Searcher& getSearcher() { return searcher; }
    FileIndexer& getFileIndexer() { return fileIndexer; }
    IndexManager& getIndexManager() { return indexManager; }
    EmbeddingClient& getEmbeddingClient() { return embeddingClient; }

private:
    EmbeddingClient embeddingClient;
    IndexManager indexManager;
    FileIndexer fileIndexer;
    Searcher searcher;
    const Config& config;
};