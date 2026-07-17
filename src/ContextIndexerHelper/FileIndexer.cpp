#include "FileIndexer.h"
#include "FileReader.h"
#include "ChunkerStrategy.h"
#include "IndexManager.h"
#include "EmbeddingClient.h"
#include "CodeParser.h"
#include "AssistantRole.h"
#include "Config.h"
#include "Logger.h"
#include <algorithm>
#include "nlohmann/json.hpp" // Для JSON сериализации

using json = nlohmann::json;

FileIndexer::FileIndexer(
    const Config& config,
    IndexManager& indexManager,
    EmbeddingClient& embeddingClient,
    std::shared_ptr<LLMProvider> provider)
    : config(config), indexManager(indexManager), embeddingClient(embeddingClient),
      llmProvider(provider),
      fileIndexerMetadataPath(".shdata/file_indexer_meta.json") // Инициализация пути
{
    ignoredDirectories.insert(config.ignored_directories.begin(), config.ignored_directories.end());
    // ignoredDirectories.insert(".shdata");
    ignoredExtensions.insert(config.ignored_extensions.begin(), config.ignored_extensions.end());
    ignoredFiles.insert(config.ignored_files.begin(), config.ignored_files.end());

    if (config.chunking_strategy == "tree-sitter" || config.chunking_strategy == "tree-sitter-hybrid") {
        codeParser = std::make_unique<CodeParser>(config);
    }

    SPDLOG_INFO("FileIndexer инициализирован.");
}

FileIndexer::~FileIndexer()
{
    SPDLOG_INFO("FileIndexer уничтожен.");
}

void FileIndexer::setIgnoredDirectories(const std::vector<std::string>& dirs)
{
    std::lock_guard lock(mtx);
    ignoredDirectories.clear();
    for (const auto& dir : dirs) {
        ignoredDirectories.insert(dir);
    }
}

void FileIndexer::setIgnoredExtensions(const std::vector<std::string>& exts)
{
    std::lock_guard lock(mtx);
    ignoredExtensions.clear();
    for (const auto& ext : exts) {
        ignoredExtensions.insert(ext);
    }
}

void FileIndexer::load() {
    std::lock_guard lock(mtx);
    std::ifstream metaFile(fileIndexerMetadataPath);
    if (!metaFile.is_open()) {
        SPDLOG_WARN("Файл метаданных FileIndexer '{}' не найден. Будет создан новый fileIndex.", fileIndexerMetadataPath);
        fileIndex.clear(); // Убеждаемся, что он пуст, если нет файла метаданных
        return;
    }

    json meta;
    try {
        metaFile >> meta;

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
        SPDLOG_INFO("Загружено {} записей в fileIndex из '{}'.", fileIndex.size(), fileIndexerMetadataPath);
    }
    catch (json::parse_error& e) {
        SPDLOG_ERROR("Error: Could not parse FileIndexer metadata file '{}'. Starting fresh. Error: {}", fileIndexerMetadataPath, e.what());
        fileIndex.clear(); // Начинаем с чистого листа, если JSON поврежден
        return;
    }
}

void FileIndexer::save() {
    std::lock_guard lock(mtx);
    if (fileIndex.empty()) {
        SPDLOG_INFO("FileIndexer: fileIndex пуст, сохранение не требуется. Удаляю старый файл метаданных.");
        fs::remove(fileIndexerMetadataPath);
        return;
    }

    SPDLOG_INFO("Сохранение метаданных FileIndexer в: {}", fileIndexerMetadataPath);
    std::ofstream metaFile(fileIndexerMetadataPath);
    if (!metaFile.is_open()) {
        SPDLOG_ERROR("Error: Could not open FileIndexer metadata file '{}' for writing.", fileIndexerMetadataPath);
        return;
    }

    json meta;
    json file_index_json;
    for (const auto& [path, record] : fileIndex) {
        json chunks_json = json::array();
        for(const auto& chunk_info : record.chunks) {
            chunks_json.push_back({{"id", chunk_info.id}, {"start_byte", chunk_info.location.start_byte}, {"length", chunk_info.location.length}});
        }
        file_index_json[path] = {{"last_write_time", record.last_write_time.time_since_epoch().count()}, {"chunks", chunks_json}};
    }
    meta["file_index"] = file_index_json;
    metaFile << meta.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
    SPDLOG_INFO("Сохранено {} записей в метаданные FileIndexer.", fileIndex.size());
}

void FileIndexer::scanDiskForChanges(
    const fs::path& directoryPath,
    std::vector<std::string>& files_to_reindex,
    std::vector<std::string>& files_to_remove,
    int& updatedFilesCount)
{
    std::unordered_set<std::string> files_on_disk;

    // Сканируем диск (без блокировки)
    try {
        for (auto it = fs::recursive_directory_iterator(directoryPath); 
             it != fs::recursive_directory_iterator(); ++it) {
            const auto& entry = *it;
            if (entry.is_directory() && ignoredDirectories.count(entry.path().filename().string())) {
                it.disable_recursion_pending();
                continue;
            }
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                if (ignoredExtensions.count(path.extension().string()) || 
                    ignoredFiles.count(path.filename().string())) {
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

    // Захватываем блокировку для сравнения состояния индекса
    {
        std::lock_guard lock(mtx);
        for (const auto& path_str : files_on_disk) {
            auto lastWriteTime = fs::last_write_time(fs::path(path_str));
            auto it = fileIndex.find(path_str);
            bool isNew = (it == fileIndex.end());
            bool isModified = !isNew && (it->second.last_write_time != lastWriteTime);
            if (isNew || isModified) {
                files_to_reindex.push_back(path_str);
                if (isModified) {
                    updatedFilesCount++;
                }
            }
        }
        for (const auto& [path, record] : fileIndex) {
            if (files_on_disk.find(path) == files_on_disk.end()) {
                // Удалённый файл
                files_to_remove.push_back(path);
            }
        }
    }
}

std::vector<ChunkToAdd> FileIndexer::processFileChunks(const std::string& path)
{
    // Эта функция больше не используется напрямую. Логика перенесена в reindexFile.
    // Оставляем ее пустой, чтобы избежать ошибок компиляции в других местах, если они есть.
    SPDLOG_WARN("Вызвана устаревшая функция processFileChunks. Логика должна быть в reindexFile.");
    return {};
}

void FileIndexer::updateIndexWithNewChunks(
    const std::string& path,
    const std::vector<ChunkToAdd>& chunks)
{
    std::lock_guard lock(mtx);
    // Эта функция больше не используется напрямую. Логика перенесена в reindexFile.
    // Оставляем ее пустой, чтобы избежать ошибок компиляции.
    SPDLOG_WARN("Вызвана устаревшая функция updateIndexWithNewChunks. Логика должна быть в reindexFile.");
}


void FileIndexer::indexDirectory(const fs::path& directoryPath)
{
    SPDLOG_INFO("Начало сканирования каталога: {}", directoryPath.string());

    // Фаза 1: Сканирование диска
    std::vector<std::string> files_to_reindex;
    std::vector<std::string> files_to_remove;
    int updatedFiles = 0;
    scanDiskForChanges(directoryPath, files_to_reindex, files_to_remove, updatedFiles);

    int removedFiles = files_to_remove.size();
    int newFiles = files_to_reindex.size() - updatedFiles;

    // Фаза 2: Обработка изменений (вне блокировки для сетевых операций)
    for (const auto& path : files_to_reindex) {
        reindexFile(path);
    }
    for (const auto& path : files_to_remove) {
        removeFileFromIndex(path);
    }

    SPDLOG_INFO("Завершено индексирование. Новых файлов: {}, Измененных: {}, Удалено: {}", 
                newFiles, updatedFiles, removedFiles);
}

void FileIndexer::reindexFile(const std::string& path)
{
    fs::path fs_path(path);
    if (!fs::exists(fs_path) || !fs::is_regular_file(fs_path)) {
        removeFileFromIndex(path);
        return;
    }

    if (ignoredExtensions.count(fs_path.extension().string()) ||
        ignoredFiles.count(fs_path.filename().string())) {
        return;
    }

    // --- Начало новой логики с сохранением после каждого чанка ---

    // 1. Сначала удаляем все старые чанки, связанные с этим файлом.
    // Это делает операцию реиндексации атомарной на уровне файла.
    // Если процесс упадет, при следующем запуске файл будет снова помечен для реиндексации.
    {
        std::lock_guard lock(mtx);
        auto it = fileIndex.find(path);
        if (it != fileIndex.end()) {
            SPDLOG_INFO("Удаление {} старых чанков для файла: {}", it->second.chunks.size(), path);
            for (const auto& chunk_info : it->second.chunks) {
                indexManager.removePoint(chunk_info.id);
            }
            // Очищаем запись, но не удаляем ее полностью, чтобы сохранить last_write_time
            it->second.chunks.clear();
        }
    }

    // 2. Получаем контент и разбиваем его на "сырые" чанки (только текст и позиция)
    std::string content = FileReader::readFile(fs_path);
    if (content.empty()) {
        removeFileFromIndex(path); // Если файл пуст, просто удаляем его из индекса
        return;
    }

    std::vector<CodeChunk> code_chunks;
    bool use_semantic = false;
    if ((config.chunking_strategy == "tree-sitter" || config.chunking_strategy == "tree-sitter-hybrid") && codeParser) {
        code_chunks = codeParser->parse(content, fs_path.extension().string());
        use_semantic = !code_chunks.empty();
    }

    if (!use_semantic) {
        auto fixedChunks = ChunkerStrategy::splitFixedSize(content, config.embedding_max_text_length, config.embedding_chunk_overlap);
        for (const auto& location : fixedChunks) {
            code_chunks.push_back({content.substr(location.start_byte, location.length), "text", location.start_byte, location.length});
        }
    }

    // 3. Последовательно обрабатываем каждый чанк и сохраняем прогресс
    for (size_t i = 0; i < code_chunks.size(); ++i) {
        const auto& chunk = code_chunks[i];
        std::string chunkName = path + " [#chunk " + std::to_string(i) + "]";
        std::string text_for_embedding = chunk.text;

        // Генерация саммари (долгая операция)
        if (use_semantic && config.chunking_strategy == "tree-sitter-hybrid" && llmProvider) {
            std::string summary = llmProvider->generateChunkSummary(chunk.text, chunkName);
            if (!summary.empty()) {
                text_for_embedding = "[SUMMARY]: " + summary + "\n[CODE]:\n" + chunk.text;
            }
        }

        // Генерация эмбеддинга
        auto emb = embeddingClient.getEmbedding(text_for_embedding, chunkName);

        // Добавление в индекс и сохранение
        if (!emb.empty()) {
            std::lock_guard lock(mtx);
            size_t id = indexManager.addPoint(path, emb, {chunk.start_byte, chunk.length});
            if (id != (size_t)-1) {
                fileIndex[path].chunks.push_back({id, {chunk.start_byte, chunk.length}});
                SPDLOG_INFO("Проиндексирован чанк {}/{} для файла '{}'. Сохранение прогресса...", i + 1, code_chunks.size(), path);
                indexManager.save();
                save();
            }
        }
    }

    // 4. После обработки всех чанков, обновляем время модификации файла в нашем индексе.
    // Это "зафиксирует" успешную индексацию файла.
    {
        std::lock_guard lock(mtx);
        fileIndex[path].last_write_time = fs::last_write_time(fs_path);
        save(); // Сохраняем финальное время
    }
}

void FileIndexer::removeFileFromIndex(const std::string& path)
{
    std::lock_guard lock(mtx);
    auto it = fileIndex.find(path);
    if (it != fileIndex.end()) {
        SPDLOG_INFO("Удаление файла из индекса: {}", path);
        // Корректно удаляем все чанки файла из IndexManager
        for (const auto& chunk_info : it->second.chunks) {
            indexManager.removePoint(chunk_info.id);
        }
        fileIndex.erase(it);
        indexManager.save();
        save();
    }
}
