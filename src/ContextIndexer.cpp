#include "ContextIndexer.h"
#include "Logger.h"

ContextIndexer::ContextIndexer(std::shared_ptr<LLMProvider> provider, 
                               const Config& config,
                               const std::string& index_meta_path,
                               const std::string& index_bin_path,
                               const std::string& file_indexer_meta_path)
    : config(config), // Initialize config first
      embeddingClient(provider),
      indexManager(index_meta_path, index_bin_path),
      fileIndexer(config, indexManager, embeddingClient, provider, file_indexer_meta_path),
      searcher(indexManager, embeddingClient) // searcher also needs config indirectly via fileIndexer
{
    SPDLOG_INFO("ContextIndexer инициализирован.");
    loadIndex();
}

ContextIndexer::~ContextIndexer() {
    SPDLOG_INFO("ContextIndexer уничтожен.");
}

void ContextIndexer::loadIndex() {
    SPDLOG_INFO("ContextIndexer: Загрузка индекса...");
    indexManager.load(); // Загружаем HNSW индекс и id_to_chunk_map
    fileIndexer.load();  // Загружаем метаданные файлов (fileIndex)

    // Опционально: Добавьте проверку согласованности здесь, если необходимо, например:
    if (indexManager.getEmbeddingCount() > 0 && fileIndexer.getFileCount() == 0) {
        SPDLOG_WARN("IndexManager загрузил эмбеддинги, но FileIndexer не загрузил метаданные файлов. Это может указывать на несогласованность.");
    }
}

void ContextIndexer::saveIndex() {
    SPDLOG_INFO("ContextIndexer: Сохранение индекса...");
    indexManager.save();
    fileIndexer.save();
}

void ContextIndexer::indexDirectory(const std::filesystem::path& directoryPath) {
    fileIndexer.indexDirectory(directoryPath);
}

bool ContextIndexer::isPathIgnored(const std::filesystem::path& path) const {
    // 1. Check if the file itself is in the ignored_files list
    if (std::find(config.ignored_files.begin(), config.ignored_files.end(), path.filename().string()) != config.ignored_files.end()) {
        SPDLOG_DEBUG("Игнорирование файла '{}' по имени.", path.string());
        return true;
    }

    // 2. Check if the file has an ignored_extension
    if (path.has_extension()) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // Case-insensitive check
        if (std::find(config.ignored_extensions.begin(), config.ignored_extensions.end(), ext) != config.ignored_extensions.end()) {
            SPDLOG_DEBUG("Игнорирование файла '{}' по расширению '{}'.", path.string(), ext);
            return true;
        }
    }

    // 3. Check if any part of the path is an ignored_directory
    for (const auto& ignored_dir : config.ignored_directories) {
        // Convert ignored_dir to path for proper comparison
        std::filesystem::path current_path = path;
        while (current_path != current_path.root_path()) {
            if (current_path.filename() == ignored_dir) {
                SPDLOG_DEBUG("Игнорирование файла '{}' из-за родительской директории '{}'.", path.string(), ignored_dir);
                return true;
            }
            current_path = current_path.parent_path();
        }
    }
    return false;
}

void ContextIndexer::reindexFile(const std::string& path) {
    if (isPathIgnored(path)) {
        SPDLOG_DEBUG("Пропуск реиндексации игнорируемого файла: {}", path);
        return;
    }
    fileIndexer.reindexFile(path);
}

void ContextIndexer::removeFileFromIndex(const std::string& path) {
    if (isPathIgnored(path)) {
        SPDLOG_DEBUG("Пропуск удаления игнорируемого файла из индекса: {}", path);
        return;
    }
    fileIndexer.removeFileFromIndex(path);
}

void ContextIndexer::reindexProject() {
    SPDLOG_INFO("Начата полная реиндексация проекта...");
    // Пересобираем индекс
    fileIndexer.indexDirectory(std::filesystem::path(config.project_dir));
    // Сохраняем новый индекс
    saveIndex();
    SPDLOG_INFO("Реиндексация проекта завершена. Эмбеддингов: {}, Файлов: {}", 
                getEmbeddingsCount(), getFileCount());
}

int ContextIndexer::getEmbeddingsCount() const {
    return indexManager.getEmbeddingCount();
}

int ContextIndexer::getFileCount() const {
    return fileIndexer.getFileCount();
}