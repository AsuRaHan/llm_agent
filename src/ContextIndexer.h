#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <mutex>

#include "ContextIndexerHelper/IndexManager.h"
#include "ContextIndexerHelper/FileIndexer.h"
#include "ContextIndexerHelper/Searcher.h"

namespace fs = std::filesystem;

struct Config;

/**
 * ContextIndexer: Фасад, координирует работу всех компонентов
 * - IndexManager: управление HNSW индексом
 * - FileIndexer: сканирование директории и реиндексирование
 * - Searcher: поиск с эмбеддингами и keyword boost
 */
class ContextIndexer
{
public:
    explicit ContextIndexer(const Config& config);
    ~ContextIndexer();

    /**
     * Устанавливает директории для исключения из индексирования
     */
    void setIgnoredDirectories(const std::vector<std::string>& ignoredDirs);

    /**
     * Устанавливает расширения файлов для исключения из индексирования
     */
    void setIgnoredExtensions(const std::vector<std::string>& ignoredExts);

    /**
     * Индексирует директорию, определяет изменения и обновляет индекс
     */
    void indexDirectory(const fs::path& directoryPath);

    /**
     * Возвращает количество векторов в индексе
     */
    int getEmbeddingsCount() const;

    /**
     * Возвращает количество файлов в индексе
     */
    int getFileCount() const;

    /**
     * Сохраняет индекс на диск
     */
    void saveIndex();

    /**
     * Поиск топ K релевантных результатов по текстовому запросу
     */
    std::vector<SearchResult> findTopK(const std::string& queryText, int k);

    /**
     * Переиндексирует один файл
     */
    void reindexFile(const std::string& path);

    /**
     * Удаляет файл из индекса
     */
    void removeFileFromIndex(const std::string& path);

    // Мьютекс для синхронизации доступа между FileWatcher и другими компонентами
    mutable std::recursive_mutex mtx;

private:
    const Config& config;
    std::unique_ptr<EmbeddingClient> embeddingClient;
    std::unique_ptr<IndexManager> indexManager;
    std::unique_ptr<FileIndexer> fileIndexer;
    std::unique_ptr<Searcher> searcher;

    void loadIndex();
};
