#include "ChunkerStrategy.h"
#include <algorithm>
#include <cmath>

std::vector<ChunkLocation> ChunkerStrategy::splitFixedSize(
    const std::string& text,
    size_t chunkSize,
    size_t overlap)
{
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

double ChunkerStrategy::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b)
{
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
