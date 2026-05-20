#include "ContextIndexer.h"
#include "Logger.h"

ContextIndexer::ContextIndexer(const Config& config)
    : config(config),
      embeddingClient(config), // Инициализируем EmbeddingClient первым
      indexManager(".shdata/index_meta.json", ".shdata/index.bin"), // Инициализируем IndexManager
      fileIndexer(config, indexManager, embeddingClient), // Передаем IndexManager по ссылке
      searcher(indexManager, embeddingClient) // Передаем IndexManager по ссылке
{
    SPDLOG_INFO("ContextIndexer инициализирован.");
    loadIndex(); // Загружаем данные IndexManager и FileIndexer
}

ContextIndexer::~ContextIndexer() {
    SPDLOG_INFO("ContextIndexer уничтожен. Сохраняю индекс...");
    saveIndex(); // Убеждаемся, что оба компонента сохранены при уничтожении ContextIndexer
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

void ContextIndexer::reindexFile(const std::string& path) {
    fileIndexer.reindexFile(path);
}

void ContextIndexer::removeFileFromIndex(const std::string& path) {
    fileIndexer.removeFileFromIndex(path);
}

int ContextIndexer::getEmbeddingsCount() const {
    return indexManager.getEmbeddingCount();
}

int ContextIndexer::getFileCount() const {
    return fileIndexer.getFileCount();
}