#include "ContextIndexer.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <vector>
#include <regex>
#include <atomic>
#include "Logger.h" // Include the new logger header
#include "Config.h"
#include "AssistantRole.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

ContextIndexer::ContextIndexer(const Config& config)
    : config(config), embeddingClient(config), codeParser(nullptr), summarizerAssistant(nullptr)
{
    ignoredDirectories.insert(config.ignored_directories.begin(), config.ignored_directories.end());
    ignoredExtensions.insert(config.ignored_extensions.begin(), config.ignored_extensions.end());
    ignoredFiles.insert(config.ignored_files.begin(), config.ignored_files.end());
    SPDLOG_INFO("ContextIndexer инициализирован...");

    if (config.chunking_strategy == "tree-sitter" || config.chunking_strategy == "tree-sitter-hybrid") {
        codeParser = std::make_unique<CodeParser>(config);
    }
    if (config.chunking_strategy == "tree-sitter-hybrid") {
        summarizerAssistant = std::make_unique<AssistantRole>(config);
    }

    // Ensure the .shdata directory exists
    if (!fs::exists(".shdata")) {
        fs::create_directory(".shdata");
        SPDLOG_INFO("Создана директория для данных индекса: .shdata");
    }
    loadIndex();
}

ContextIndexer::~ContextIndexer()
{
    SPDLOG_INFO("ContextIndexer уничтожен."); // unique_ptr сделает всю работу по очистке
}

int ContextIndexer::getEmbeddingsCount() const {
    std::lock_guard lock(mtx);
    if (index) {
        return index->cur_element_count;
    }
    return 0;
}

void ContextIndexer::loadIndex()
{
    std::lock_guard lock(mtx);
    std::ifstream metaFile(metadataDbPath);
    if (!metaFile.is_open()) {
        SPDLOG_WARN("Файл метаданных '{}' не найден. Будет создан новый индекс.", metadataDbPath);
        return;
    }

    json meta;
    try {
        metaFile >> meta;

        embedding_dim = meta.value("embedding_dim", 0);
        current_max_elements.store(meta.value("max_elements", 0));

        if (embedding_dim == 0) { // Can be 0 if index was empty
            SPDLOG_INFO("Метаданные не содержат векторов. Индекс будет создан при добавлении данных.");
            return;
        }

        // Restore fileIndex
        if (meta.contains("file_index")) {
            for (auto& [path, value] : meta["file_index"].items()) {
                long long time_count = value["last_write_time"];
                auto duration_since_epoch = fs::file_time_type::duration(time_count);
                std::vector<ChunkInfo> chunks;
                if (value.contains("chunks")) {
                    for (const auto& chunk_json : value["chunks"]) {
                        chunks.push_back({
                            chunk_json["id"],
                            {chunk_json["start_byte"], chunk_json["length"]}
                        });
                    }
                }
                fileIndex[path] = { fs::file_time_type(duration_since_epoch), chunks };
            }
        }

        // Restore id_to_chunk_map
        if (meta.contains("id_to_chunk_map")) {
            for (auto& [id_str, data] : meta["id_to_chunk_map"].items()) {
                size_t id = std::stoull(id_str);
                id_to_chunk_map[id] = std::make_pair(
                    data["path"].get<std::string>(),
                    ChunkLocation{data["start_byte"].get<size_t>(), data["length"].get<size_t>()}
                );
            }
        }

    }
    catch (json::parse_error& e) {
        SPDLOG_ERROR("Error: Could not parse metadata file '{}'. Starting fresh. Error: {}", metadataDbPath, e.what());
        fileIndex.clear(); // Start with a clean slate if JSON is corrupt
        id_to_chunk_map.clear();
        embedding_dim = 0;
        current_max_elements.store(0);
        return;
    }

    if (!fs::exists(hnswIndexPath)) {
        SPDLOG_ERROR("Файл метаданных '{}' существует, но бинарный индекс '{}' отсутствует. Создание нового индекса.", metadataDbPath, hnswIndexPath);
        fileIndex.clear();
        id_to_chunk_map.clear();
        embedding_dim = 0;
        current_max_elements.store(0);
        return;
    }

    // Now load the HNSW index
    space = std::make_unique<hnswlib::L2Space>(embedding_dim);
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), current_max_elements, 16, 200, 100, true);
    SPDLOG_INFO("Загрузка бинарного индекса из '{}'...", hnswIndexPath);
    index->loadIndex(hnswIndexPath, space.get(), current_max_elements);
    SPDLOG_INFO("Загружено {} векторов из индекса.", index->cur_element_count.load());
}

void ContextIndexer::saveIndex()
{
    std::lock_guard lock(mtx);
    if (!index || getEmbeddingsCount() == 0) {
        SPDLOG_INFO("Индекс пуст или не инициализирован, сохранение не требуется.");
        // Clean up old files if they exist
        fs::remove(hnswIndexPath);
        fs::remove(metadataDbPath);
        return;
    }

    SPDLOG_INFO("Сохранение индекса в бинарный файл: {}", hnswIndexPath);
    index->saveIndex(hnswIndexPath);

    SPDLOG_INFO("Сохранение метаданных в: {}", metadataDbPath);
    std::ofstream metaFile(metadataDbPath);
    if (!metaFile.is_open()) {
        SPDLOG_ERROR("Error: Could not open metadata file '{}' for writing.", metadataDbPath);
        return;
    }

    json meta;
    meta["embedding_dim"] = embedding_dim;
    meta["max_elements"] = current_max_elements.load();

    json file_index_json;
    for (const auto& [path, record] : fileIndex) {
        json chunks_json = json::array();
        for(const auto& chunk_info : record.chunks) {
            chunks_json.push_back({
                {"id", chunk_info.id},
                {"start_byte", chunk_info.location.start_byte},
                {"length", chunk_info.location.length}
            });
        }

        file_index_json[path] = {
            {"last_write_time", record.last_write_time.time_since_epoch().count()},
            {"chunks", chunks_json}
        };
    }
    meta["file_index"] = file_index_json;

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
    SPDLOG_INFO("Сохранено {} записей в метаданные.", fileIndex.size());
}

double ContextIndexer::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) {
        return 0.0;
    }

    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    double norm_a_sqrt = std::sqrt(norm_a);
    double norm_b_sqrt = std::sqrt(norm_b);

    if (norm_a_sqrt == 0.0 || norm_b_sqrt == 0.0) {
        return 0.0;
    }

    return dot_product / (norm_a_sqrt * norm_b_sqrt);
}

std::vector<ChunkLocation> ContextIndexer::fixedSizeChunkText(const std::string& text, size_t chunkSize, size_t overlap) {
    std::vector<ChunkLocation> chunks;
    if (text.empty()) return chunks;

    // Защита от бесконечного цикла при некорректном оверлапе
    if (overlap >= chunkSize) {
        overlap = chunkSize > 1 ? chunkSize / 2 : 0;
    }

    size_t start = 0;
    while (start < text.length()) {
        size_t current_chunk_size = text.length() - start;
        if (start + chunkSize >= text.length()) {
            chunks.push_back({start, current_chunk_size});
            break;
        }

        size_t target_end = start + chunkSize;
        // Ищем ближайший перенос строки с конца окна, чтобы не рвать абзацы
        size_t newline_pos = text.rfind('\n', target_end);

        size_t actual_end = target_end;
        if (newline_pos != std::string::npos && newline_pos > start + (chunkSize - overlap)) {
            actual_end = newline_pos + 1; // Режем аккуратно по концу строки
        } else {
            // Если переноса строки нет, ищем хотя бы пробел между словами
            size_t space_pos = text.rfind(' ', target_end);
            if (space_pos != std::string::npos && space_pos > start) {
                actual_end = space_pos;
            }
        }

        current_chunk_size = actual_end - start;
        chunks.push_back({start, current_chunk_size});

        // Сдвигаемся вперед с учетом overlap
        size_t step = current_chunk_size;
        if (step <= overlap) {
            start = actual_end; // Предотвращаем зависание, если шаг слишком мал
        } else {
            start = actual_end - overlap;
        }
    }
    return chunks;
}

std::vector<SearchResult> ContextIndexer::findTopK(const std::string& queryText, int k)
{
    std::vector<float> queryEmbedding = embeddingClient.getEmbedding(queryText, "query");
    if (queryEmbedding.empty()) {
        SPDLOG_WARN("Не удалось сгенерировать эмбеддинг для запроса. Поиск невозможен.");
        return {};
    }
    if (queryEmbedding.size() != embedding_dim) {
        SPDLOG_ERROR("Размерность эмбеддинга запроса ({}) не совпадает с размерностью индекса ({}).", queryEmbedding.size(), embedding_dim);
        return {};
    }

    std::lock_guard lock(mtx);
    if (!index || getEmbeddingsCount() == 0) {
        SPDLOG_WARN("Индекс пуст. Поиск невозможен.");
        return {};
    }


    // Perform k-NN search
    auto result = index->searchKnn(queryEmbedding.data(), k);

    std::vector<SearchResult> topResults;
    std::vector<std::pair<float, size_t>> result_pairs; // The pair is <distance, label>
    while (!result.empty()) {
        result_pairs.emplace_back(result.top());
        result.pop();
    }
    // The priority queue from searchKnn is a max-heap on distance. Popping gives elements from furthest to nearest.
    // We reverse to get them in order of nearest to furthest (best to worst score).
    std::reverse(result_pairs.begin(), result_pairs.end());

    SPDLOG_DEBUG("HNSW search returned {} results (distance, label):", result_pairs.size());
    for(const auto& pair : result_pairs) {
        // pair.first is L2 distance, smaller is better.
        SPDLOG_DEBUG("  - dist={:.4f}, id={}", pair.first, pair.second);
    }

    for(const auto& pair : result_pairs) {
        size_t id = pair.second; // ID is the second element
        auto it = id_to_chunk_map.find(id);
        if (it != id_to_chunk_map.end()) {
            const auto& chunk_ptr = it->second;
            std::string chunk_text = readChunkContent(chunk_ptr.first, chunk_ptr.second);
            if (chunk_text.empty()) continue;

            // To calculate cosine similarity, we need the original vector.
            std::vector<float> data_vec = index->getDataByLabel<float>(id);
            double score = cosineSimilarity(queryEmbedding, data_vec);
            topResults.push_back({chunk_ptr.first, chunk_text, score});
        }
    }

    // Sort by score descending
    std::sort(topResults.begin(), topResults.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    // --- Intelligent Keyword Boost ---
    // If the query mentions a filename or class name, this extracts it
    // and finds source files with a matching stem, forcing them into the context.
    std::unordered_set<size_t> seenChunkIds;
    for(const auto& pair : result_pairs) {
        seenChunkIds.insert(pair.second);
    }

    std::unordered_set<std::string> keywords;

    // 1. Find keywords that look like filenames (e.g., "config.json") and extract the stem ("config")
    std::regex re_filename(R"(([\w\.-]+)\.[\w]+)");
    auto fn_begin = std::sregex_iterator(queryText.begin(), queryText.end(), re_filename);
    auto fn_end = std::sregex_iterator();
    for (std::sregex_iterator i = fn_begin; i != fn_end; ++i) {
        fs::path query_path((*i).str(0));
        keywords.insert(query_path.stem().string());
    }

    // 2. Find keywords that look like PascalCase or UPPERCASE identifiers (e.g., "ContextIndexer", "URL")
    // This regex looks for words with at least two capital letters or all-caps words of 2+ length.
    std::regex re_identifier(R"(\b([A-Z][a-z0-9_]*[A-Z][a-zA-Z0-9_]*|[A-Z]{2,})\b)");
    auto id_begin = std::sregex_iterator(queryText.begin(), queryText.end(), re_identifier);
    auto id_end = std::sregex_iterator();
    for (std::sregex_iterator i = id_begin; i != id_end; ++i) {
        keywords.insert((*i).str(0));
    }

    // Умный поиск ключевых слов ВНУТРИ текста чанков кодовой базы
    for (const std::string& keyword : keywords) {
        if (keyword.length() < 3) continue; // Игнорируем слишком короткие слова

        std::string lower_keyword = keyword;
        std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(),
            [](unsigned char c){ return std::tolower(c); });

        for (const auto& [path, record] : fileIndex) {
            for (const auto& chunk_info : record.chunks) {
                // Если этот чанк модель уже и так нашла через эмбеддинги — пропускаем
                if (seenChunkIds.count(chunk_info.id)) continue;

                std::string chunk_text = readChunkContent(path, chunk_info.location);
                if (chunk_text.empty()) continue;

                std::string lower_chunk_text = chunk_text;
                std::transform(lower_chunk_text.begin(), lower_chunk_text.end(), lower_chunk_text.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                // Проверяем вхождение ключевого слова (класса, функции) прямо в тело кода чанка
                if (lower_chunk_text.find(lower_keyword) != std::string::npos) {
                    SPDLOG_DEBUG("Keyword boost: Найдено совпадение для '{}' внутри чанка файла '{}'", keyword, path);
                    
                    // Извлекаем оригинальный вектор чанка из HNSW по его ID
                    std::vector<float> data_vec = index->getDataByLabel<float>(chunk_info.id);
                    double base_score = cosineSimilarity(queryEmbedding, data_vec);
                    
                    // Мягко бустим score, но не позволяем ему улететь в стратосферу выше 1.0
                    double boosted_score = std::min(1.0, base_score + 0.15); 

                    topResults.push_back({path, chunk_text, boosted_score});
                    seenChunkIds.insert(chunk_info.id);
                }
            }
        }
    }

    // Финальная сортировка по убыванию score и обрезка до k результатов
    std::sort(topResults.begin(), topResults.end(), [](const SearchResult& a, const SearchResult& b) { 
        return a.score > b.score; 
    });
    
    if (topResults.size() > (size_t)k) {
        topResults.resize(k);
    }

    return topResults;
}

void ContextIndexer::setIgnoredDirectories(const std::vector<std::string>& ignoredDirs)
{
    std::lock_guard lock(mtx);
    ignoredDirectories.clear();
    for (const auto& dir : ignoredDirs)
    {
        ignoredDirectories.insert(dir);
    }
}

void ContextIndexer::setIgnoredExtensions(const std::vector<std::string>& ignoredExts)
{
    std::lock_guard lock(mtx);
    ignoredExtensions.clear();
    for (const auto& ext : ignoredExts)
    {
        ignoredExtensions.insert(ext);
    }
}

std::string ContextIndexer::readFileContent(const fs::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        SPDLOG_ERROR("Не удалось открыть файл: {}", path.string());
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ContextIndexer::readChunkContent(const std::string& path, const ChunkLocation& location)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        SPDLOG_ERROR("Не удалось открыть файл для чтения чанка: {}", path);
        return "";
    }
    file.seekg(location.start_byte);
    std::string content(location.length, '\0');
    file.read(&content[0], location.length);
    return content;
}

void ContextIndexer::addChunk(const std::string& path, const std::string& text, const std::vector<float>& embedding, const ChunkLocation& location) {
    // This is a private method called by public locked methods, so no lock needed here.
    if (embedding.empty()) return;

    // Initialize index if this is the first element. This part is already under a lock.
    if (!index) {
        embedding_dim = embedding.size();
        space = std::make_unique<hnswlib::L2Space>(embedding_dim);
        // Start with a reasonable max size, allow resizing and replacing deleted elements.
        index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), 10000, 16, 200, 100, true);
        SPDLOG_INFO("HNSW index initialized with embedding dimension: {}", embedding_dim);
    }

    if (embedding.size() != embedding_dim) {
        SPDLOG_ERROR("Inconsistent embedding dimension! Expected {}, got {}. Skipping chunk.", embedding_dim, embedding.size());
        return;
    }

    size_t new_id = current_max_elements.fetch_add(1);
    // Check if we need to resize the index
    if (new_id >= index->max_elements_) {
        SPDLOG_INFO("Изменил размер индекса HNSW с {} на {}", index->max_elements_, index->max_elements_ * 2);
        index->resizeIndex(index->max_elements_ * 2);
    }

    index->addPoint(embedding.data(), new_id);
    id_to_chunk_map[new_id] = {path, location};
    fileIndex[path].chunks.push_back({new_id, location});
}

void ContextIndexer::indexDirectory(const fs::path& directoryPath)
{
    SPDLOG_INFO("Начало сканирования каталога: {}", directoryPath.string());

    // --- Фаза 1: Сканирование диска и определение изменений (минимальная блокировка) ---
    std::unordered_set<std::string> files_on_disk;
    std::vector<std::string> files_to_reindex;
    std::vector<std::string> files_to_remove;
    int newFiles = 0, updatedFiles = 0;

    // Сначала сканируем все файлы на диске (блокировка не нужна)
    try {
        for (auto it = fs::recursive_directory_iterator(directoryPath); it != fs::recursive_directory_iterator(); ++it) {
            const auto& entry = *it;
            if (entry.is_directory() && ignoredDirectories.count(entry.path().filename().string())) {
                it.disable_recursion_pending();
                continue;
            }
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                if (ignoredExtensions.count(path.extension().string()) || ignoredFiles.count(path.filename().string()) ||
                    path.filename() == metadataDbPath || path.filename() == hnswIndexPath) {
                    continue;
                }
                auto canonicalPath = fs::weakly_canonical(path).string();
                std::replace(canonicalPath.begin(), canonicalPath.end(), '\\', '/');
                files_on_disk.insert(canonicalPath);
            }
        }
    } catch (const fs::filesystem_error& e) {
        SPDLOG_ERROR("Ошибка при сканировании директории {}: {}", directoryPath.string(), e.what());
        return;
    }

    // Теперь захватываем короткую блокировку для сравнения с текущим состоянием индекса
    {
        std::lock_guard lock(mtx);
        for (const auto& path_str : files_on_disk) {
            auto lastWriteTime = fs::last_write_time(fs::path(path_str));
            auto it = fileIndex.find(path_str);
            bool isNew = (it == fileIndex.end());
            bool isModified = !isNew && (it->second.last_write_time != lastWriteTime);
            if (isNew || isModified) {
                files_to_reindex.push_back(path_str);
                if (isNew) newFiles++; else updatedFiles++;
            }
        }
        for (const auto& [path, record] : fileIndex) {
            if (files_on_disk.find(path) == files_on_disk.end()) {
                files_to_remove.push_back(path);
            }
        }
    }

    // --- Фаза 2: Обработка изменений (без глобальной блокировки, методы блокируются внутри) ---
    for (const auto& path : files_to_reindex) {
        reindexFile(path); // Этот метод выполняет сетевой ввод-вывод, а затем блокируется внутри для обновления
    }
    for (const auto& path : files_to_remove) {
        removeFileFromIndex(path); // Этот метод блокируется внутри
    }

    SPDLOG_INFO("Завершено индексирование. Новых файлов: {}, Изменённых файлов: {}, Удалено файлов: {}",
                newFiles, updatedFiles, files_to_remove.size());
}

void ContextIndexer::reindexFile(const std::string& path) {
    fs::path fs_path(path);
    if (!fs::exists(fs_path) || !fs::is_regular_file(fs_path)) {
        removeFileFromIndex(path);
        return;
    }

    if (ignoredExtensions.count(fs_path.extension().string()) ||
        ignoredFiles.count(fs_path.filename().string()))
    {
        return;
    }

    std::string content = readFileContent(fs_path);
    if (content.empty()) {
        removeFileFromIndex(path); // removeFileFromIndex handles locking
        return;
    }

    std::vector<CodeChunk> semanticChunks;
    bool use_semantic = false;
    if ((config.chunking_strategy == "tree-sitter" || config.chunking_strategy == "tree-sitter-hybrid") && codeParser) {
        semanticChunks = codeParser->parse(content, fs_path.extension().string());
        use_semantic = !semanticChunks.empty();
    }

    struct ChunkToAdd {
        std::string text;
        std::vector<float> embedding;
        ChunkLocation location;
    };
    std::vector<ChunkToAdd> chunks_to_add;

    // This part contains network calls and is performed outside the lock
    if (use_semantic) {
        for (size_t i = 0; i < semanticChunks.size(); ++i) {
            const auto& chunk = semanticChunks[i];
            ChunkLocation location = {chunk.start_byte, chunk.length};
            std::string chunkName = path + " [#chunk " + std::to_string(i) + "]";
            std::string text_for_embedding = chunk.text;
            
            if (config.chunking_strategy == "tree-sitter-hybrid" && summarizerAssistant) {
                std::string summary = summarizerAssistant->generateChunkSummary(chunk.text, chunkName);
                text_for_embedding = "[SUMMARY]: " + summary + "\n[CODE]:\n" + chunk.text;
            }
            auto emb = embeddingClient.getEmbedding(text_for_embedding, chunkName);
            if (!emb.empty()) {
                chunks_to_add.push_back({chunk.text, std::move(emb), location});
            }
        }
    } else { // Fallback to fixed size
        auto fixedChunks = fixedSizeChunkText(content, config.embedding_max_text_length, config.embedding_chunk_overlap);
        for (size_t i = 0; i < fixedChunks.size(); ++i) {
            const auto& location = fixedChunks[i];
            std::string chunk_text = content.substr(location.start_byte, location.length);
            std::string chunkName = path + " [#chunk " + std::to_string(i) + "]";
            auto emb = embeddingClient.getEmbedding(chunk_text, chunkName);
            if (!emb.empty()) {
                chunks_to_add.push_back({chunk_text, std::move(emb), location});
            }
        }
    }

    // Now, acquire the lock and modify the index state
    std::lock_guard lock(mtx);

    auto it = fileIndex.find(path);
    if (it != fileIndex.end()) {
        for (const auto& chunk_data : it->second.chunks) {
            if (index) index->markDelete(chunk_data.id);
            id_to_chunk_map.erase(chunk_data.id);
        }
    }

    fileIndex[path].last_write_time = fs::last_write_time(fs_path);
    fileIndex[path].chunks.clear();

    for (const auto& chunk_data : chunks_to_add) {
        addChunk(path, chunk_data.text, chunk_data.embedding, chunk_data.location);
    }

    if (fileIndex.count(path) && fileIndex.at(path).chunks.empty()) {
        fileIndex.erase(path);
    }
}

void ContextIndexer::removeFileFromIndex(const std::string& path) {
    std::lock_guard lock(mtx);
    auto it = fileIndex.find(path);
    if (it != fileIndex.end()) {
        SPDLOG_INFO("Удаление файла из индекса: {}", path);
        for (const auto& chunk_data : it->second.chunks) {
            if (index) index->markDelete(chunk_data.id);
            id_to_chunk_map.erase(chunk_data.id);
        }
        fileIndex.erase(it);
    }
}
