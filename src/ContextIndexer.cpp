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
#include "Logger.h" // Include the new logger header
#include "Config.h"
#include "AssistantRole.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

ContextIndexer::ContextIndexer(const Config& config)
    : config(config), embeddingClient(config), codeParser(nullptr), summarizerAssistant(nullptr), space(nullptr), index(nullptr)
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
    // Saving is now done explicitly in main() after indexing to ensure data safety.
    delete index;
    delete space;
    SPDLOG_INFO("ContextIndexer уничтожен.");
}

int ContextIndexer::getEmbeddingsCount() const {
    if (index) {
        return index->cur_element_count;
    }
    return 0;
}

void ContextIndexer::loadIndex()
{
    std::ifstream metaFile(metadataDbPath);
    if (!metaFile.is_open()) {
        SPDLOG_WARN("Файл метаданных '{}' не найден. Будет создан новый индекс.", metadataDbPath);
        return;
    }

    json meta;
    try {
        metaFile >> meta;

        embedding_dim = meta.value("embedding_dim", 0);
        current_max_elements = meta.value("max_elements", 0);

        if (embedding_dim == 0) { // Can be 0 if index was empty
            SPDLOG_INFO("Метаданные не содержат векторов. Индекс будет создан при добавлении данных.");
            return;
        }

        // Restore fileIndex
        if (meta.contains("file_index")) {
            for (auto& [path, value] : meta["file_index"].items()) {
                long long time_count = value["last_write_time"];
                auto duration_since_epoch = fs::file_time_type::duration(time_count);
                std::vector<ChunkData> chunks;
                if (value.contains("chunks")) {
                    for (const auto& chunk_json : value["chunks"]) {
                        chunks.push_back({
                            chunk_json["text"],
                            chunk_json["id"]
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
                id_to_chunk_map[id] = { data["path"], data["text"] };
            }
        }

    }
    catch (json::parse_error& e) {
        SPDLOG_ERROR("Error: Could not parse metadata file '{}'. Starting fresh. Error: {}", metadataDbPath, e.what());
        fileIndex.clear(); // Start with a clean slate if JSON is corrupt
        id_to_chunk_map.clear();
        embedding_dim = 0;
        current_max_elements = 0;
        return;
    }

    if (!fs::exists(hnswIndexPath)) {
        SPDLOG_ERROR("Файл метаданных '{}' существует, но бинарный индекс '{}' отсутствует. Создание нового индекса.", metadataDbPath, hnswIndexPath);
        fileIndex.clear();
        id_to_chunk_map.clear();
        embedding_dim = 0;
        current_max_elements = 0;
        return;
    }

    // Now load the HNSW index
    space = new hnswlib::L2Space(embedding_dim);
    index = new hnswlib::HierarchicalNSW<float>(space, current_max_elements, 16, 200, 100, true);
    SPDLOG_INFO("Загрузка бинарного индекса из '{}'...", hnswIndexPath);
    index->loadIndex(hnswIndexPath, space, current_max_elements);
    SPDLOG_INFO("Загружено {} векторов из индекса.", index->cur_element_count.load());
}

void ContextIndexer::saveIndex()
{
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
    meta["max_elements"] = current_max_elements;

    json file_index_json;
    for (const auto& [path, record] : fileIndex) {
        json chunks_json = json::array();
        for(const auto& chunk : record.chunks) {
            chunks_json.push_back({
                {"id", chunk.id},
                {"text", chunk.text}
            });
        }

        file_index_json[path] = {
            {"last_write_time", record.last_write_time.time_since_epoch().count()},
            {"chunks", chunks_json}
        };
    }
    meta["file_index"] = file_index_json;

    json id_map_json;
    for (const auto& [id, data] : id_to_chunk_map) {
        id_map_json[std::to_string(id)] = {
            {"path", data.first},
            {"text", data.second}
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

std::vector<std::string> ContextIndexer::fixedSizeChunkText(const std::string& text, size_t chunkSize, size_t overlap) {
    std::vector<std::string> chunks;
    if (text.empty()) return chunks;

    // Защита от бесконечного цикла при некорректном оверлапе
    if (overlap >= chunkSize) {
        overlap = chunkSize / 2;
    }

    size_t start = 0;
    while (start < text.length()) {
        if (start + chunkSize >= text.length()) {
            chunks.push_back(text.substr(start));
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
            if (space_pos != std::string::npos && space_pos > start + (chunkSize - overlap)) {
                actual_end = space_pos;
            }
        }

        chunks.push_back(text.substr(start, actual_end - start));
        
        // Сдвигаемся вперед с учетом overlap
        size_t step = actual_end - start;
        if (step <= overlap) {
            start = actual_end; // Предотвращаем зависание, если строка слишком длинная
        } else {
            start = actual_end - overlap;
        }
    }
    return chunks;
}

std::vector<SearchResult> ContextIndexer::findTopK(const std::string& queryText, int k)
{
    if (!index || getEmbeddingsCount() == 0) {
        SPDLOG_WARN("Индекс пуст. Поиск невозможен.");
        return {};
    }

    std::vector<float> queryEmbedding = embeddingClient.getEmbedding(queryText, "query");
    if (queryEmbedding.empty()) {
        SPDLOG_WARN("Не удалось сгенерировать эмбеддинг для запроса. Поиск невозможен.");
        return {};
    }
    if (queryEmbedding.size() != embedding_dim) {
        SPDLOG_ERROR("Размерность эмбеддинга запроса ({}) не совпадает с размерностью индекса ({}).", queryEmbedding.size(), embedding_dim);
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
            const auto& chunk_data = it->second;
            // To calculate cosine similarity, we need the original vector.
            std::vector<float> data_vec = index->getDataByLabel<float>(id);
            double score = cosineSimilarity(queryEmbedding, data_vec);
            topResults.push_back({chunk_data.first, chunk_data.second, score});
        }
    }

    // Sort by score descending
    std::sort(topResults.begin(), topResults.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    // --- Intelligent Keyword Boost ---
    // If the query mentions a filename or class name, this extracts it
    // and finds source files with a matching stem, forcing them into the context.
    std::unordered_set<std::string> seenChunks;
    for(const auto& res : topResults) {
        seenChunks.insert(res.chunkText);
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
            for (const auto& chunk_meta : record.chunks) {
                // Если этот чанк модель уже и так нашла через эмбеддинги — пропускаем
                if (seenChunks.find(chunk_meta.text) != seenChunks.end()) continue;

                std::string lower_chunk_text = chunk_meta.text;
                std::transform(lower_chunk_text.begin(), lower_chunk_text.end(), lower_chunk_text.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                // Проверяем вхождение ключевого слова (класса, функции) прямо в тело кода чанка
                if (lower_chunk_text.find(lower_keyword) != std::string::npos) {
                    SPDLOG_DEBUG("Keyword boost: Найдено совпадение для '{}' внутри чанка файла '{}'", keyword, path);
                    topResults.push_back({path, chunk_meta.text, 1.15}); // Даем мощный буст (1.15)
                    seenChunks.insert(chunk_meta.text);
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
    ignoredDirectories.clear();
    for (const auto& dir : ignoredDirs)
    {
        ignoredDirectories.insert(dir);
    }
}

void ContextIndexer::setIgnoredExtensions(const std::vector<std::string>& ignoredExts)
{
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

void ContextIndexer::addChunk(const std::string& path, const std::string& text, const std::vector<float>& embedding) {
    if (embedding.empty()) return;

    // Initialize index if this is the first element
    if (!index) {
        embedding_dim = embedding.size();
        space = new hnswlib::L2Space(embedding_dim);
        // Start with a reasonable max size, allow resizing and replacing deleted elements.
        index = new hnswlib::HierarchicalNSW<float>(space, 10000, 16, 200, 100, true);
        SPDLOG_INFO("HNSW index initialized with embedding dimension: {}", embedding_dim);
    }

    if (embedding.size() != embedding_dim) {
        SPDLOG_ERROR("Inconsistent embedding dimension! Expected {}, got {}. Skipping chunk.", embedding_dim, embedding.size());
        return;
    }

    size_t new_id = current_max_elements;
    // Check if we need to resize the index
    if (new_id >= index->max_elements_) {
        SPDLOG_INFO("Resizing HNSW index from {} to {}", index->max_elements_, index->max_elements_ * 2);
        index->resizeIndex(index->max_elements_ * 2);
    }

    index->addPoint(embedding.data(), new_id);
    id_to_chunk_map[new_id] = {path, text};
    fileIndex[path].chunks.push_back({text, new_id});
    current_max_elements++;
}

void ContextIndexer::indexDirectory(const fs::path& directoryPath)
{
    SPDLOG_INFO("\nStarting filtered indexing of directory: {}", directoryPath.string());
    auto iterator = fs::recursive_directory_iterator(directoryPath);

    std::unordered_set<std::string> files_on_disk;

    int updatedFiles = 0;
    int newFiles = 0;

    for (const auto& entry : iterator)
    {
        if (entry.is_directory() && ignoredDirectories.count(entry.path().filename().string()))
        {
            iterator.disable_recursion_pending();
            continue;
        }

        if (entry.is_regular_file())
        {
            const auto& path = entry.path();
            if (ignoredExtensions.count(path.extension().string()) ||
                ignoredFiles.count(path.filename().string()) ||
                path.filename() == metadataDbPath ||
                path.filename() == hnswIndexPath)
            {
                continue;
            }
            
            auto canonicalPath = fs::weakly_canonical(path).string();
            std::replace(canonicalPath.begin(), canonicalPath.end(), '\\', '/');
            files_on_disk.insert(canonicalPath);

            auto lastWriteTime = fs::last_write_time(path);

            auto it = fileIndex.find(canonicalPath);
            bool isNew = (it == fileIndex.end());
            bool isModified = !isNew && (it->second.last_write_time != lastWriteTime);

            if (isNew || isModified) {
                if(isNew) {
                    newFiles++;
                    SPDLOG_INFO("Обнаружен новый файл: {}", canonicalPath);
                } else {
                    updatedFiles++;
                    SPDLOG_INFO("Обнаружен изменённый файл: {}", canonicalPath);
                    // Mark old chunks for deletion
                    for (const auto& chunk_data : it->second.chunks) {
                        if (index) {
                           index->markDelete(chunk_data.id);
                        }
                        id_to_chunk_map.erase(chunk_data.id);
                    }
                    // Clear old chunks from file record
                    it->second.chunks.clear();
                }

                std::string content = readFileContent(path);
                if (content.empty()) {
                    SPDLOG_DEBUG("  Skipping empty file: {}", canonicalPath);
                    if (!isNew) {
                        fileIndex.erase(it); // Remove from index if it became empty
                    }
                    continue;
                }
                
                std::vector<std::string> textChunks;
                if ((config.chunking_strategy == "tree-sitter" || config.chunking_strategy == "tree-sitter-hybrid") && codeParser) {
                    textChunks = codeParser->parse(content, path.extension().string());
                    // Fallback to fixed size if tree-sitter fails or returns no chunks for a non-empty file
                    if (textChunks.empty() && !content.empty()) {
                        SPDLOG_WARN("Tree-sitter не вернул чанков для файла '{}'. Используется разбиение по умолчанию.", canonicalPath);
                        textChunks = fixedSizeChunkText(content, config.embedding_max_text_length, config.embedding_chunk_overlap);
                    }
                } else {
                    // Fallback to fixed-size chunking if strategy is "fixed" or something else.
                    textChunks = fixedSizeChunkText(content, config.embedding_max_text_length, config.embedding_chunk_overlap);
                }
                
                // Update file record timestamp and prepare for new chunks
                fileIndex[canonicalPath].last_write_time = lastWriteTime;
                fileIndex[canonicalPath].chunks.clear(); // Ensure it's empty before adding new ones

                for (size_t i = 0; i < textChunks.size(); ++i)
                {
                    const std::string& chunk = textChunks[i];
                    std::string chunkName = canonicalPath + " [#chunk " + std::to_string(i) + "]";
                    std::string text_for_embedding = chunk;

                    if (config.chunking_strategy == "tree-sitter-hybrid" && summarizerAssistant) {
                        std::string summary = summarizerAssistant->generateChunkSummary(chunk, chunkName);
                        text_for_embedding = "[SUMMARY]: " + summary + "\n[CODE]:\n" + chunk;
                    }
                    auto emb = embeddingClient.getEmbedding(text_for_embedding, chunkName);

                    if (!emb.empty()) {
                        addChunk(canonicalPath, chunk, emb);
                    }
                }

                // If no embeddings were generated for a file, it should not be in the index.
                if (fileIndex.count(canonicalPath) && fileIndex.at(canonicalPath).chunks.empty()) {
                    SPDLOG_WARN("Не удалось сгенерировать эмбеддинги для файла: {}. Он будет удален из индекса.", canonicalPath);
                    fileIndex.erase(canonicalPath);
                }
            }
        }
    }

    // Find and process deleted files
    int deletedFiles = 0;
    std::vector<std::string> files_to_erase;
    for (const auto& [path, record] : fileIndex) {
        if (files_on_disk.find(path) == files_on_disk.end()) {
            files_to_erase.push_back(path);
        }
    }

    for (const auto& path_to_erase : files_to_erase) {
        auto it = fileIndex.find(path_to_erase);
        if (it != fileIndex.end()) {
            SPDLOG_INFO("Удален из индекса отсутствующий файл: {}", path_to_erase);
            // Mark all chunks of this file for deletion
            for (const auto& chunk_data : it->second.chunks) {
                if (index) {
                    index->markDelete(chunk_data.id);
                }
                id_to_chunk_map.erase(chunk_data.id);
            }
            fileIndex.erase(it);
            deletedFiles++;
        }
    }

    SPDLOG_INFO("Завершено индексирование. Новых файлов: {}, Изменённых файлов: {}, Удалено файлов: {}",
                newFiles, updatedFiles, deletedFiles);
}
