#include "IndexManager.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include "Logger.h"

using json = nlohmann::json;

IndexManager::IndexManager(const std::string& metadataPath, const std::string& indexPath)
    : metadataDbPath(metadataPath), hnswIndexPath(indexPath)
{
    ensureDataDir();
    SPDLOG_INFO("IndexManager инициализирован. Пути: meta={}, index={}", metadataDbPath, hnswIndexPath);
}

IndexManager::~IndexManager()
{
    SPDLOG_INFO("IndexManager уничтожается, сохраняем индекс...");
    try {
        save();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Перехвачено исключение во время сохранения индекса в деструкторе: {}", e.what());
    } catch (...) {
        SPDLOG_ERROR("Перехвачено неизвестное исключение во время сохранения индекса в деструкторе.");
    }
}

void IndexManager::ensureDataDir()
{
    if (!fs::exists(".shdata")) {
        fs::create_directory(".shdata");
        SPDLOG_INFO("Создана директория для данных индекса: .shdata");
    }
}

void IndexManager::initialize(size_t embedding_dim, size_t max_elements)
{
    std::lock_guard lock(mtx);
    if (index) {
        SPDLOG_WARN("Индекс уже инициализирован. Пропуск повторной инициализации.");
        return;
    }

    this->embedding_dim = embedding_dim;
    this->current_max_elements.store(max_elements);
    space = std::make_unique<hnswlib::L2Space>(embedding_dim);
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(
        space.get(), max_elements, 16, 200, 100, true
    );
    SPDLOG_INFO("HNSW index initialized with embedding dimension: {}, max_elements: {}", 
                embedding_dim, max_elements);
}

size_t IndexManager::addPoint(
    const std::string& path,
    const std::vector<float>& embedding,
    const ChunkLocation& location)
{
    std::lock_guard lock(mtx);

    if (embedding.empty()) {
        SPDLOG_ERROR("Попытка добавить пустой эмбеддинг");
        return -1;
    }

    // Инициализируем индекс если это первый элемент
    if (!index) {
        embedding_dim = embedding.size();
        space = std::make_unique<hnswlib::L2Space>(embedding_dim);
        index = std::make_unique<hnswlib::HierarchicalNSW<float>>(
            space.get(), 10000, 16, 200, 100, true
        );
        SPDLOG_INFO("HNSW index initialized with embedding dimension: {}", embedding_dim);
    }

    if (embedding.size() != embedding_dim) {
        SPDLOG_ERROR("Inconsistent embedding dimension! Expected {}, got {}. Skipping chunk.",
                     embedding_dim, embedding.size());
        return -1;
    }

    size_t new_id = current_max_elements.fetch_add(1);
    // Проверяем нужно ли расширить индекс
    if (new_id >= index->max_elements_) {
        SPDLOG_INFO("Увеличиваю размер индекса HNSW с {} на {}", 
                    index->max_elements_, index->max_elements_ * 2);
        index->resizeIndex(index->max_elements_ * 2);
    }

    index->addPoint(embedding.data(), new_id);
    id_to_chunk_map[new_id] = {path, location};

    return new_id;
}

void IndexManager::removePoint(size_t id)
{
    std::lock_guard lock(mtx);
    if (index && id_to_chunk_map.count(id)) {
        index->markDelete(id);
        id_to_chunk_map.erase(id);
    }
}

std::vector<std::pair<size_t, float>> IndexManager::searchKnn(
    const std::vector<float>& query,
    int k)
{
    std::lock_guard lock(mtx);

    if (!index || getEmbeddingCount() == 0) {
        SPDLOG_WARN("Индекс пуст или не инициализирован. Поиск невозможен.");
        return {};
    }

    if (query.size() != embedding_dim) {
        SPDLOG_ERROR("Размерность запроса ({}) не совпадает с размерностью индекса ({}).",
                     query.size(), embedding_dim);
        return {};
    }

    // Выполняем k-NN поиск
    auto result = index->searchKnn(query.data(), k);

    std::vector<std::pair<float, size_t>> result_pairs; // {distance, label}
    while (!result.empty()) {
        result_pairs.emplace_back(result.top());
        result.pop();
    }
    // Priority queue возвращает элементы от дальних к ближним. Разворачиваем.
    std::reverse(result_pairs.begin(), result_pairs.end());

    std::vector<std::pair<size_t, float>> output; // {id, distance}
    for (const auto& pair : result_pairs) {
        output.push_back({pair.second, pair.first});
    }

    return output;
}

std::vector<float> IndexManager::getDataByLabel(size_t id)
{
    std::lock_guard lock(mtx);
    if (!index) {
        return {};
    }
    return index->getDataByLabel<float>(id);
}

std::pair<std::string, ChunkLocation> IndexManager::getChunkById(size_t id)
{
    std::lock_guard lock(mtx);
    auto it = id_to_chunk_map.find(id);
    if (it != id_to_chunk_map.end()) {
        return it->second;
    }
    return {"", {0, 0}};
}

int IndexManager::getEmbeddingCount() const
{
    if (index) {
        return index->cur_element_count;
    }
    return 0;
}

void IndexManager::load()
{
    std::lock_guard lock(mtx);
    std::ifstream metaFile(metadataDbPath);
    if (!metaFile.is_open()) {
        SPDLOG_WARN("Файл метаданных '{}' не найден. Будет создан новый индекс.", metadataDbPath);
        return;
    }

    json meta;
    SPDLOG_INFO("IndexManager: Чтение метаданных из '{}'.", metadataDbPath);
    try {
        metaFile >> meta;
        embedding_dim = meta.value("embedding_dim", 0);
        current_max_elements.store(meta.value("max_elements", 0));

        if (embedding_dim == 0) {
            SPDLOG_INFO("Метаданные не содержат векторов. Индекс будет создан при добавлении данных.");
            return;
        }

        // Восстанавливаем id_to_chunk_map
        if (meta.contains("id_to_chunk_map")) {
            for (auto& [id_str, data] : meta["id_to_chunk_map"].items()) {
                size_t id = std::stoull(id_str);
                id_to_chunk_map[id] = std::make_pair(
                    data["path"].get<std::string>(),
                    ChunkLocation{data["start_byte"].get<size_t>(), data["length"].get<size_t>()}
                );
            }
        }
        SPDLOG_INFO("IndexManager: Загружено {} записей в id_to_chunk_map.", id_to_chunk_map.size());
    }
    catch (json::parse_error& e) {
        SPDLOG_ERROR("Error: Could not parse metadata file '{}'. Starting fresh. Error: {}", 
                     metadataDbPath, e.what());
        id_to_chunk_map.clear();
        embedding_dim = 0;
        current_max_elements.store(0);
        return;
    }

    if (!fs::exists(hnswIndexPath)) {
        SPDLOG_ERROR("Файл метаданных '{}' существует, но бинарный индекс '{}' отсутствует.",
                     metadataDbPath, hnswIndexPath);
        id_to_chunk_map.clear();
        embedding_dim = 0;
        current_max_elements.store(0);
        return;
    }

    // Загружаем HNSW индекс
    space = std::make_unique<hnswlib::L2Space>(embedding_dim);
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(
        space.get(), current_max_elements, 16, 200, 100, true
    );
    SPDLOG_INFO("IndexManager: Инициализация HNSWlib для загрузки.");
    SPDLOG_INFO("Загрузка бинарного индекса из '{}'...", hnswIndexPath);
    index->loadIndex(hnswIndexPath, space.get(), current_max_elements);
    SPDLOG_INFO("Загружено {} векторов из индекса.", index->cur_element_count.load());
}

void IndexManager::save()
{
    std::lock_guard lock(mtx);
    if (!index || getEmbeddingCount() == 0) {
        SPDLOG_INFO("IndexManager: Индекс пуст или не инициализирован, сохранение не требуется. Удаляю старые файлы.");
        // fs::remove(hnswIndexPath);
        // fs::remove(metadataDbPath);
        return;
    }

    SPDLOG_INFO("IndexManager: Сохранение HNSW индекса в бинарный файл: {}", hnswIndexPath);
    index->saveIndex(hnswIndexPath);

    SPDLOG_INFO("IndexManager: Сохранение метаданных в: {}", metadataDbPath);
    std::ofstream metaFile(metadataDbPath);
    if (!metaFile.is_open()) {
        SPDLOG_ERROR("Error: Could not open metadata file '{}' for writing.", metadataDbPath);
        return;
    }

    json meta;
    meta["embedding_dim"] = embedding_dim;
    meta["max_elements"] = current_max_elements.load();

    json id_map_json;
    for (const auto& [id, chunk_ptr] : id_to_chunk_map) {
        id_map_json[std::to_string(id)] = {
            {"path", chunk_ptr.first},
            {"start_byte", chunk_ptr.second.start_byte},
            {"length", chunk_ptr.second.length}
        };
    }
    meta["id_to_chunk_map"] = id_map_json;

    metaFile << meta.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
    SPDLOG_INFO("IndexManager: Сохранено {} записей в метаданные.", id_to_chunk_map.size());
}
