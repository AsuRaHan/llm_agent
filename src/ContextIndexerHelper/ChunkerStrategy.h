#pragma once

#include <string>
#include <vector>

/**
 * ChunkLocation: Описывает расположение чанка внутри файла
 */
struct ChunkLocation
{
    size_t start_byte;
    size_t length;
};

/**
 * ChunkerStrategy: Ответственен за разбиение текста на чанки (фиксированный размер + смысловое разбиение)
 */
class ChunkerStrategy
{
public:
    /**
     * Разбивает текст на чанки фиксированного размера с перекрытием
     * Старается разрывать по границам строк и слов для сохранения семантики
     * 
     * @param text Исходный текст
     * @param chunkSize Желаемый размер одного чанка в символах
     * @param overlap Размер перекрытия между смежными чанками в символах
     * @return Вектор с позициями и размерами чанков
     */
    static std::vector<ChunkLocation> splitFixedSize(
        const std::string& text,
        size_t chunkSize,
        size_t overlap
    );

    /**
     * Вспомогательный метод: вычисляет косинус-подобие двух векторов
     * @param a Первый вектор
     * @param b Второй вектор
     * @return Значение от 0.0 до 1.0
     */
    static double cosineSimilarity(
        const std::vector<float>& a,
        const std::vector<float>& b
    );
};
