#pragma once

#include "Config.h"
#include "EmbeddingClient.h"
#include "ContextIndexerHelper/IndexManager.h"
#include "ContextIndexerHelper/FileIndexer.h"
#include "ContextIndexerHelper/Searcher.h"
#include <memory>
#include <filesystem>

class ContextIndexer {
public:
    ContextIndexer(const Config& config);
    ~ContextIndexer();

    void indexDirectory(const std::filesystem::path& directoryPath);
    void reindexFile(const std::string& path); // Делегировано от FileIndexer
    void removeFileFromIndex(const std::string& path); // Делегировано от FileIndexer

    void saveIndex(); // Делегирует IndexManager и FileIndexer
    void loadIndex(); // Делегирует IndexManager и FileIndexer

    int getEmbeddingsCount() const;
    int getFileCount() const;

    // Предоставляет доступ к Searcher для внешнего использования (например, ApiHandlers)
    Searcher& getSearcher() { return searcher; }
    FileIndexer& getFileIndexer() { return fileIndexer; } // Для FileWatcher для взаимодействия
    IndexManager& getIndexManager() { return indexManager; } // Для Searcher для получения данных по метке
    EmbeddingClient& getEmbeddingClient() { return embeddingClient; } // Для Searcher для получения эмбеддингов запроса

private:
    const Config& config;
    EmbeddingClient embeddingClient; // Должен быть инициализирован первым
    IndexManager indexManager;       // Владеет HNSW индексом и картой ID
    FileIndexer fileIndexer;         // Владеет метаданными файла (пути, last_write_time, chunk_ids)
    Searcher searcher;               // Выполняет операции поиска
};