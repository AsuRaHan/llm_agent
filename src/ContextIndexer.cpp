#include "ContextIndexer.h"
#include <iostream>
#include <filesystem>
#include "Logger.h"
#include "Config.h"
#include "EmbeddingClient.h"

namespace fs = std::filesystem;

ContextIndexer::ContextIndexer(const Config& config)
    : config(config),
      embeddingClient(std::make_unique<EmbeddingClient>(config)),
      indexManager(std::make_unique<IndexManager>()),
      fileIndexer(std::make_unique<FileIndexer>(config, *indexManager, *embeddingClient)),
      searcher(std::make_unique<Searcher>(*indexManager, *embeddingClient))
{
    SPDLOG_INFO("ContextIndexer инициализирован...");
    loadIndex();
}

ContextIndexer::~ContextIndexer()
{
    SPDLOG_INFO("ContextIndexer уничтожен.");
}

void ContextIndexer::loadIndex()
{
    std::lock_guard lock(mtx);
    indexManager->load();
}

void ContextIndexer::setIgnoredDirectories(const std::vector<std::string>& ignoredDirs)
{
    std::lock_guard lock(mtx);
    fileIndexer->setIgnoredDirectories(ignoredDirs);
}

void ContextIndexer::setIgnoredExtensions(const std::vector<std::string>& ignoredExts)
{
    std::lock_guard lock(mtx);
    fileIndexer->setIgnoredExtensions(ignoredExts);
}

void ContextIndexer::indexDirectory(const fs::path& directoryPath)
{
    // No lock here, FileIndexer manages its own threading.
    fileIndexer->indexDirectory(directoryPath);
}

int ContextIndexer::getEmbeddingsCount() const
{
    std::lock_guard lock(mtx);
    return indexManager->getEmbeddingCount();
}

int ContextIndexer::getFileCount() const
{
    std::lock_guard lock(mtx);
    return fileIndexer->getFileCount();
}

void ContextIndexer::saveIndex()
{
    std::lock_guard lock(mtx);
    indexManager->save();
}

std::vector<SearchResult> ContextIndexer::findTopK(const std::string& queryText, int k)
{
    // No lock here, Searcher manages its own threading.
    return searcher->findTopK(queryText, k, fileIndexer->getFileIndex());
}

void ContextIndexer::reindexFile(const std::string& path)
{
    // No lock here, FileIndexer manages its own threading.
    fileIndexer->reindexFile(path);
}

void ContextIndexer::removeFileFromIndex(const std::string& path)
{
    // No lock here, FileIndexer manages its own threading.
    fileIndexer->removeFileFromIndex(path);
}
