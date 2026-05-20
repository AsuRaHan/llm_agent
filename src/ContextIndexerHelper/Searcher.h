#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include "FileIndexer.h" // Для FileRecord
#include "IndexManager.h"

class EmbeddingClient;
class FileReader;

/**
 * SearchResult: Результат поиска
 */
struct SearchResult
{
    std::string filePath;
    std::string chunkText;
    double score;
};

/**
 * Searcher: Поиск с эмбеддингами и keyword boost логикой
 */
class Searcher
{
public:
    explicit Searcher(IndexManager& indexManager, EmbeddingClient& embeddingClient);

    /**
     * Поиск топ K результатов по запросу
     * Использует HNSW поиск + keyword boost для релевантных результатов
     * 
     * @param queryText Текст запроса
     * @param k Количество результатов
     * @param fileIndex Карта файлов для keyword boost логики
     * @return Отсортированный вектор лучших результатов
     */
    std::vector<SearchResult> findTopK(
        const std::string& queryText,
        int k,
        const std::unordered_map<std::string, FileRecord>& fileIndex
    );

private:
    IndexManager& indexManager;
    EmbeddingClient& embeddingClient;

    /**
     * Извлекает ключевые слова из запроса (названия файлов, идентификаторы)
     */
    std::unordered_set<std::string> extractKeywords(const std::string& queryText);

    /**
     * Применяет keyword boost к результатам поиска
     */
    void boostByKeywords(
        std::vector<SearchResult>& results,
        const std::string& queryText,
        const std::unordered_map<std::string, FileRecord>& fileIndex,
        const std::vector<float>& queryEmbedding,
        std::unordered_set<size_t>& seenChunkIds
    );
};
