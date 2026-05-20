#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <filesystem>

#include "hnswlib/hnswlib.h"
#include "ChunkerStrategy.h"

namespace fs = std::filesystem;

/**
 * IndexManager: Управляет HNSW индексом и метаданными (id -> {path, location})
 */
class IndexManager
{
public:
    explicit IndexManager(
        const std::string& metadataPath = ".shdata/index_meta.json",
        const std::string& indexPath = ".shdata/index.bin"
    );
    ~IndexManager();

    /**
     * Инициализирует HNSW индекс с заданной размерностью
     */
    void initialize(size_t embedding_dim, size_t max_elements = 10000);

    /**
     * Добавляет точку (вектор) в индекс
     * @param path Путь к исходному файлу
     * @param embedding Вектор эмбеддинга
     * @param location Расположение в файле
     * @return ID точки в индексе
     */
    size_t addPoint(
        const std::string& path,
        const std::vector<float>& embedding,
        const ChunkLocation& location
    );

    /**
     * Удаляет точку из индекса
     */
    void removePoint(size_t id);

    /**
     * Поиск K ближайших соседей
     * @param query Вектор запроса
     * @param k Количество результатов
     * @return Вектор пар {ID, расстояние}
     */
    std::vector<std::pair<size_t, float>> searchKnn(
        const std::vector<float>& query,
        int k
    );

    /**
     * Получает оригинальный вектор по ID
     */
    std::vector<float> getDataByLabel(size_t id);

    /**
     * Получает информацию о чанке по его ID
     */
    std::pair<std::string, ChunkLocation> getChunkById(size_t id);

    /**
     * Возвращает количество векторов в индексе
     */
    int getEmbeddingCount() const;

    /**
     * Загружает индекс с диска
     */
    void load();

    /**
     * Сохраняет индекс на диск
     */
    void save();

    // Мьютекс для синхронизации доступа
    mutable std::recursive_mutex mtx;

private:
    const std::string metadataDbPath;
    const std::string hnswIndexPath;

    size_t embedding_dim = 0;
    std::unique_ptr<hnswlib::L2Space> space;
    std::unique_ptr<hnswlib::HierarchicalNSW<float>> index;
    std::unordered_map<size_t, std::pair<std::string, ChunkLocation>> id_to_chunk_map;
    std::atomic<size_t> current_max_elements = 0;

    void ensureDataDir();
};
