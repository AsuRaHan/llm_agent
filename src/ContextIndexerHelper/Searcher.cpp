#include "Searcher.h"
#include "IndexManager.h"
#include "EmbeddingClient.h"
#include "FileReader.h"
#include "ChunkerStrategy.h"
#include "Logger.h"
#include "FileIndexer.h" // Для FileRecord
#include <algorithm>
#include <filesystem>
#include <regex>
#include <cctype>

Searcher::Searcher(IndexManager& indexManager, EmbeddingClient& embeddingClient)
    : indexManager(indexManager), embeddingClient(embeddingClient)
{
    SPDLOG_INFO("Searcher инициализирован.");
}

std::unordered_set<std::string> Searcher::extractKeywords(const std::string& queryText)
{
    std::unordered_set<std::string> keywords;

    // 1. Ищем названия файлов (e.g., "config.json") и берём stem ("config")
    std::regex re_filename(R"(([\w\.-]+)\.[\w]+)");
    auto fn_begin = std::sregex_iterator(queryText.begin(), queryText.end(), re_filename);
    auto fn_end = std::sregex_iterator();
    for (std::sregex_iterator i = fn_begin; i != fn_end; ++i) {
        std::filesystem::path query_path((*i).str(0));
        keywords.insert(query_path.stem().string());
    }

    // 2. Ищем PascalCase или UPPERCASE идентификаторы (e.g., "ContextIndexer", "URL")
    std::regex re_identifier(R"(\b([A-Z][a-z0-9_]*[A-Z][a-zA-Z0-9_]*|[A-Z]{2,})\b)");
    auto id_begin = std::sregex_iterator(queryText.begin(), queryText.end(), re_identifier);
    auto id_end = std::sregex_iterator();
    for (std::sregex_iterator i = id_begin; i != id_end; ++i) {
        keywords.insert((*i).str(0));
    }

    return keywords;
}

void Searcher::boostByKeywords(
    std::vector<SearchResult>& results,
    const std::string& queryText,
    const std::unordered_map<std::string, FileRecord>& fileIndex,
    const std::vector<float>& queryEmbedding,
    std::unordered_set<size_t>& seenChunkIds)
{
    auto keywords = extractKeywords(queryText);
    
     // Умный поиск ключевых слов внутри текста чанков
    for (const std::string& keyword : keywords) {
        if (keyword.length() < 3) continue; // Игнорируем слишком короткие слова

        std::string lower_keyword = keyword;
        std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(),
            [](unsigned char c) { return std::tolower(c); });

        for (const auto& [path, record] : fileIndex) {
            for (const auto& chunk_info : record.chunks) {
                // Если этот чанк модель уже и так нашла через эмбеддинги — пропускаем
                if (seenChunkIds.count(chunk_info.id)) continue;

                std::string chunk_text = FileReader::readChunk(path, chunk_info.location.start_byte, chunk_info.location.length);
                if (chunk_text.empty()) continue;

                std::string lower_chunk_text = chunk_text;
                std::transform(lower_chunk_text.begin(), lower_chunk_text.end(), lower_chunk_text.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                // Проверяем вхождение ключевого слова в текст чанка
                if (lower_chunk_text.find(lower_keyword) != std::string::npos) {
                    SPDLOG_DEBUG("Keyword boost: Найдено совпадение для '{}' внутри чанка файла '{}'", 
                                keyword, path);

                    // Извлекаем оригинальный вектор чанка из HNSW по его ID
                    std::vector<float> data_vec = indexManager.getDataByLabel(chunk_info.id); // getDataByLabel имеет свою блокировку
                    if (data_vec.empty()) continue;

                    double base_score = ChunkerStrategy::cosineSimilarity(queryEmbedding, data_vec);
                    // Мягко бустим score, но не позволяем ему улететь в стратосферу выше 1.0
                    double boosted_score = std::min(1.0, base_score + 0.15);

                    results.push_back({path, chunk_text, boosted_score});
                    seenChunkIds.insert(chunk_info.id);
                }
            }
        }
    }
}

std::vector<SearchResult> Searcher::findTopK(
    const std::string& queryText,
    int k,
    const std::unordered_map<std::string, FileRecord>& fileIndex)
{
    // Получаем эмбеддинг для запроса
    std::vector<float> queryEmbedding = embeddingClient.getEmbedding(queryText, "query");
    if (queryEmbedding.empty()) {
        SPDLOG_WARN("Не удалось сгенерировать эмбеддинг для запроса. Поиск невозможен.");
        return {};
    }

    // Выполняем k-NN поиск
    auto result_pairs = indexManager.searchKnn(queryEmbedding, k);

    std::vector<SearchResult> topResults;
    std::unordered_set<size_t> seenChunkIds;
    for (const auto& [id, distance] : result_pairs) {
        seenChunkIds.insert(id);
        auto chunk_info = indexManager.getChunkById(id);
        const auto& [path, location] = chunk_info;

        if (path.empty()) continue;

        std::string chunk_text = FileReader::readChunk(path, location.start_byte, location.length);
        if (chunk_text.empty()) continue;

        // Вычисляем косинус-подобие как меру релевантности (вместо L2 расстояния)
        std::vector<float> data_vec = indexManager.getDataByLabel(id); // getDataByLabel имеет свою блокировку
        double score = ChunkerStrategy::cosineSimilarity(queryEmbedding, data_vec);
        topResults.push_back({path, chunk_text, score});
    }

    // Сортируем по score
    std::sort(topResults.begin(), topResults.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    // Применяем keyword boost для дополнительных релевантных результатов
    boostByKeywords(topResults, queryText, fileIndex, queryEmbedding, seenChunkIds);

    // Финальная сортировка и обрезка
    std::sort(topResults.begin(), topResults.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    if (topResults.size() > (size_t)k) {
        topResults.resize(k);
    }

    return topResults;
}
