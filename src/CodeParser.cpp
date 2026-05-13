#include "CodeParser.h"
#include "Logger.h"
#include <algorithm>
#include "Config.h" // Include Config header

// Include the C headers from tree-sitter
extern "C" {
#include <tree_sitter/api.h>
}

// Forward declare the function to get the C++ language grammar
extern "C" TSLanguage* tree_sitter_cpp();
// Forward declare functions for new grammars
extern "C" TSLanguage* tree_sitter_markdown();
extern "C" TSLanguage* tree_sitter_cmake();


CodeParser::CodeParser(const Config& config) 
    : config(config) {
    // Create a new parser
    parser = ts_parser_new();
    initializeLanguages();
}

CodeParser::~CodeParser() {
    // Free the parser
    ts_parser_delete(parser);
}

void CodeParser::initializeLanguages() {
    SPDLOG_INFO("Инициализация языковых парсеров Tree-sitter...");
    // Register C++
    registerLanguage({".cpp", ".hpp", ".h", ".cxx", ".hxx", ".cc", ".hh"}, tree_sitter_cpp());
    registerLanguage({".md"}, tree_sitter_markdown());
    registerLanguage({".cmake", "CMakeLists.txt"}, tree_sitter_cmake());
    // Future languages will be registered here, e.g.:
    // registerLanguage({".py"}, tree_sitter_python());
}

void CodeParser::registerLanguage(const std::vector<std::string>& extensions, TSLanguage* language) {
    if (language == nullptr) {
        SPDLOG_ERROR("Не удалось загрузить грамматику языка.");
        return;
    }
    for (const auto& ext : extensions) {
        languageMap[ext] = language;
        SPDLOG_DEBUG("Зарегистрирован парсер для расширения: {}", ext);
    }
}

std::vector<std::string> CodeParser::parse(const std::string& sourceCode, const std::string& fileExtension) {
    auto it = languageMap.find(fileExtension);
    if (it == languageMap.end()) {
        SPDLOG_WARN("Для расширения '{}' не найден парсер Tree-sitter. Будет использовано разбиение по умолчанию.", fileExtension);
        return {}; // Return empty vector to signal fallback
    }

    TSLanguage* language = it->second;

    // Set the parser's language
    if (!ts_parser_set_language(parser, language)) {
        SPDLOG_ERROR("Ошибка при установке языка для парсера Tree-sitter.");
        return {};
    }

    // Build a syntax tree
    TSTree* tree = ts_parser_parse_string(parser, NULL, sourceCode.c_str(), sourceCode.length());
    if (tree == nullptr) {
        SPDLOG_ERROR("Не удалось построить синтаксическое дерево для файла.");
        return {};
    }

    // Get the root node of the syntax tree
    TSNode root_node = ts_tree_root_node(tree);

    std::vector<std::string> chunks;
    extractChunks(sourceCode, &root_node, chunks);

    // Free the syntax tree
    ts_tree_delete(tree);

    return chunks;
}

void CodeParser::extractChunks(const std::string& sourceCode, void* node, std::vector<std::string>& chunks) {
    TSNode tsNode = *(static_cast<TSNode*>(node));
    const char* type = ts_node_type(tsNode);

    // We are interested in top-level declarations
    std::string nodeType(type);
    if (nodeType == "function_definition" || 
        nodeType == "class_specifier" || 
        nodeType == "struct_specifier" ||
        nodeType == "template_declaration") {

        uint32_t start_byte = ts_node_start_byte(tsNode);
        uint32_t end_byte = ts_node_end_byte(tsNode);
        size_t chunk_len = end_byte - start_byte;

        if (chunk_len > 0) {
            std::string chunk_text = sourceCode.substr(start_byte, chunk_len);

            // If the semantically extracted chunk is larger than the embedding limit, split it.
            if (chunk_len > config.embedding_max_text_length) {
                SPDLOG_WARN("Семантический чанк типа '{}' слишком большой ({} символов). Применяется разбиение по размеру embedding_max_text_length.", nodeType, chunk_len);
                
                std::vector<std::string> sub_chunks;
                size_t start = 0;
                while (start < chunk_text.length()) {
                    if (start + config.embedding_max_text_length >= chunk_text.length()) {
                        sub_chunks.push_back(chunk_text.substr(start));
                        break;
                    }

                    size_t target_end = start + config.embedding_max_text_length;
                    // Пытаемся найти перевод строки в зоне перекрытия (overlap), чтобы разбить красиво
                    size_t newline_pos = chunk_text.rfind('\n', target_end);
                    
                    size_t actual_end = target_end;
                    if (newline_pos != std::string::npos && newline_pos > start + (config.embedding_max_text_length - config.embedding_chunk_overlap)) {
                        actual_end = newline_pos + 1; // Режем прямо по концу строки
                    }

                    sub_chunks.push_back(chunk_text.substr(start, actual_end - start));
                    
                    // Сдвигаемся назад на размер overlap для непрерывности контекста
                    size_t step = actual_end - start;
                    if (step <= (size_t)config.embedding_chunk_overlap) {
                        start = actual_end; // Защита от зависания
                    } else {
                        start = actual_end - config.embedding_chunk_overlap;
                    }
                }

                chunks.insert(chunks.end(), sub_chunks.begin(), sub_chunks.end());
            } else {
                chunks.push_back(std::move(chunk_text));
            }
        }
        // We've captured this whole block, no need to go deeper into its children
        return;
    }

    // If the node is not a chunk type, recurse into its children
    uint32_t child_count = ts_node_child_count(tsNode);
    for (uint32_t i = 0; i < child_count; ++i) {
        TSNode child_node = ts_node_child(tsNode, i);
        // We only want to recurse on named children to avoid syntax tokens like '{', '}', etc.
        if (ts_node_is_named(child_node)) {
            extractChunks(sourceCode, &child_node, chunks);
        }
    }
}