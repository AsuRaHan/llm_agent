#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include "IndexManager.h"
#include "ChunkerStrategy.h"
#include "CodeParser.h"
#include "ChunkerStrategy.h"

namespace fs = std::filesystem;

class EmbeddingClient;
class LLMProvider;
struct Config;

/**
 * "Воспоминание" о конкретном чанке
 */
struct ChunkInfo {
    size_t id;
    ChunkLocation location;
};

/**
 * "Воспоминание" о целом файле
 */
struct FileRecord {
    fs::file_time_type last_write_time;
    std::vector<ChunkInfo> chunks;
};

/**
 * Структура для передачи информации о чанке при добавлении
 */
struct ChunkToAdd
{
    std::string text;
    std::vector<float> embedding;
    ChunkLocation location;
};

/**
 * FileIndexer: Управляет сканированием директории и реиндексированием файлов
 */
class FileIndexer
{
public:
    explicit FileIndexer(
        const Config& config,
        IndexManager& indexManager,
        EmbeddingClient& embeddingClient, 
        std::shared_ptr<LLMProvider> provider,
        const std::string& metadata_path);
    ~FileIndexer();

    /**
     * Индексирует всю директорию, определяет изменения, реиндексирует измененные файлы
     */
    void indexDirectory(const fs::path& directoryPath);

    /**
     * Переиндексирует один файл
     */
    void reindexFile(const std::string& path);

    /**
     * Удаляет файл из индекса
     */
    void removeFileFromIndex(const std::string& path);

    /**
     * Устанавливает директории для исключения из индексирования
     */
    void setIgnoredDirectories(const std::vector<std::string>& dirs);

    /**
     * Устанавливает расширения файлов для исключения из индексирования
     */
    void setIgnoredExtensions(const std::vector<std::string>& exts);

    /**
     * Загружает метаданные файлов из файла.
     */
    void load();
    /**
     * Сохраняет метаданные файлов в файл.
     */
    void save();

    /**
     * Возвращает количество файлов в индексе
     */
    int getFileCount() const { return fileIndex.size(); }

    // Мьютекс для синхронизации доступа к fileIndex
    mutable std::recursive_mutex mtx;

    // Доступ к fileIndex для поиска в keyword boost
    const std::unordered_map<std::string, FileRecord>& getFileIndex() const
    {
        return fileIndex;
    }

private:
    const Config& config;
    IndexManager& indexManager;
    EmbeddingClient& embeddingClient;
    std::unique_ptr<CodeParser> codeParser;
    std::shared_ptr<LLMProvider> llmProvider;

    std::unordered_set<std::string> ignoredDirectories;
    std::unordered_set<std::string> ignoredExtensions;
    std::unordered_set<std::string> ignoredFiles;

    // fileIndex: path -> запись о файле (время изменения и список чанков с их ID)
    std::unordered_map<std::string, FileRecord> fileIndex;
    std::string fileIndexerMetadataPath; // Путь к файлу метаданных FileIndexer

    /**
     * Сканирует диск и определяет какие файлы нужно переиндексировать
     */
    void scanDiskForChanges(
        const fs::path& directoryPath,
        std::vector<std::string>& files_to_reindex,
        std::vector<std::string>& files_to_remove,
        int& updatedFilesCount
    );

    /**
     * Обрабатывает файл, разбивает на чанки и получает эмбеддинги
     */
    std::vector<ChunkToAdd> processFileChunks(const std::string& path);

    /**
     * Обновляет индекс новыми чанками для файла (удаляет старые, добавляет новые)
     */
    void updateIndexWithNewChunks(
        const std::string& path,
        const std::vector<ChunkToAdd>& chunks
    );
};
