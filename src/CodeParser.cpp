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
extern "C" TSLanguage* tree_sitter_markdown();
extern "C" TSLanguage* tree_sitter_css();
extern "C" TSLanguage* tree_sitter_html();
extern "C" TSLanguage* tree_sitter_javascript();

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
    // Register CSS
    registerLanguage({".css", ".scss", ".sass"}, tree_sitter_css());
    // Register Markdown
    registerLanguage({".md"}, tree_sitter_markdown());
    // Register HTML
    registerLanguage({".html", ".htm", ".xhtml"}, tree_sitter_html());
    // Register JavaScript
    registerLanguage({".js", ".jsx", ".mjs", ".cjs"}, tree_sitter_javascript());
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

std::vector<CodeChunk> CodeParser::parse(const std::string& sourceCode, const std::string& fileExtension) {
    SPDLOG_DEBUG("Попытка парсинга файла с расширением '{}' с помощью Tree-sitter...", fileExtension);
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

    SPDLOG_DEBUG("Язык установлен. Начинаю построение синтаксического дерева...");
    // Build a syntax tree
    TSTree* tree = ts_parser_parse_string(parser, NULL, sourceCode.c_str(), sourceCode.length());
    SPDLOG_DEBUG("Построение синтаксического дерева завершено.");
    if (tree == nullptr) {
        SPDLOG_ERROR("Не удалось построить синтаксическое дерево для файла.");
        ts_parser_reset(parser); // Сбрасываем парсер после ошибки
        return {};
    }

    SPDLOG_DEBUG("Указатель на дерево (tree) валиден. Получаю корневой узел...");
    // Get the root node of the syntax tree
    TSNode root_node = ts_tree_root_node(tree);
    SPDLOG_DEBUG("Корневой узел успешно получен. Начинаю обход дерева...");

    std::vector<CodeChunk> chunks;    
    // Начальный вызов рекурсивной функции с глубиной 0
    extractChunks(sourceCode, root_node, chunks, language, 0);

    // Free the syntax tree
    ts_tree_delete(tree);

    return chunks;
}

void CodeParser::extractChunks(const std::string& sourceCode, TSNode tsNode, std::vector<CodeChunk>& chunks, TSLanguage* language, int depth) {
    const int MAX_RECURSION_DEPTH = 256; // Защита от переполнения стека
    if (depth > MAX_RECURSION_DEPTH) {
        SPDLOG_WARN("Достигнута максимальная глубина рекурсии ({}) при парсинге узла '{}'. Обход этой ветки дерева прекращен.", MAX_RECURSION_DEPTH, ts_node_type(tsNode));
        return;
    }

    // --- ЗАЩИТА ОТ NULLPTR (по рекомендации Copilot) ---
    // Проверяем, не является ли узел "нулевым", что может привести к nullptr
    if (ts_node_is_null(tsNode)) {
        return;
    }

    // Безопасно получаем тип узла и имя языка, проверяя на nullptr перед созданием std::string
    const char* type_cstr = ts_node_type(tsNode);
    std::string nodeType = (type_cstr != nullptr) ? std::string(type_cstr) : "";

    const char* lang_cstr = ts_language_name(language);
    std::string lang_name = (lang_cstr != nullptr) ? std::string(lang_cstr) : "";
    // --- КОНЕЦ ЗАЩИТЫ ---

    bool is_chunk_candidate = false;

    if (lang_name == "cpp") {
        // For C++, we are interested in top-level declarations
        if (nodeType == "function_definition" || 
            nodeType == "class_specifier" || 
            nodeType == "struct_specifier" ||
            nodeType == "template_declaration") {
            is_chunk_candidate = true;
        }
    } else if (lang_name == "markdown") {
        // For markdown, consider headings, paragraphs, and code blocks as logical chunks
        if (nodeType == "atx_heading" || nodeType == "setext_heading" || 
            nodeType == "fenced_code_block" || nodeType == "paragraph" ||
            nodeType == "list_item") { // Also consider list items as chunks
            is_chunk_candidate = true;
        }
    } else if (lang_name == "css") {
        // Для CSS в качестве чанков используем наборы правил и at-правила.
        if (nodeType == "rule_set" || nodeType == "at_rule") {
            is_chunk_candidate = true;
        }
    } else if (lang_name == "html") {
        // Для HTML в качестве чанков используем основные элементы, скрипты, стили и комментарии.
        if (nodeType == "element" || nodeType == "script_element" || 
            nodeType == "style_element" || nodeType == "comment") {
            is_chunk_candidate = true;
        }
    } else if (lang_name == "javascript") {
        // Для JavaScript в качестве чанков используем объявления верхнего уровня и определения методов.
        if (nodeType == "function_declaration" || 
            nodeType == "class_declaration" ||
            nodeType == "lexical_declaration" || // let, const
            nodeType == "variable_declaration" || // var
            nodeType == "method_definition" ||
            nodeType == "import_statement" ||
            nodeType == "export_statement") {
            is_chunk_candidate = true;
        }
    }

    if (is_chunk_candidate) {

        uint32_t start_byte = ts_node_start_byte(tsNode);
        uint32_t end_byte = ts_node_end_byte(tsNode);
        size_t chunk_len = end_byte - start_byte;

        if (chunk_len > 0) {
            std::string chunk_text = sourceCode.substr(start_byte, chunk_len);

            // If the semantically extracted chunk is larger than the embedding limit, split it.
            if (chunk_len > config.embedding_max_text_length) {
                SPDLOG_WARN("Семантический чанк типа '{}' слишком большой ({} символов). Применяется разбиение по размеру embedding_max_text_length.", nodeType, chunk_len);
                
                size_t start = 0;
                while (start < chunk_text.length()) {
                    size_t sub_chunk_start_abs = start_byte + start;
                    if (start + config.embedding_max_text_length >= chunk_text.length()) {
                        chunks.push_back({chunk_text.substr(start), sub_chunk_start_abs, chunk_text.length() - start});
                        break;
                    }

                    size_t target_end = start + config.embedding_max_text_length;
                    // Пытаемся найти перевод строки в зоне перекрытия (overlap), чтобы разбить красиво
                    size_t newline_pos = chunk_text.rfind('\n', target_end);
                    
                    size_t actual_end = target_end;
                    if (newline_pos != std::string::npos && newline_pos > start + (config.embedding_max_text_length - config.embedding_chunk_overlap)) {
                        actual_end = newline_pos + 1; // Режем прямо по концу строки
                    }

                    size_t sub_chunk_len = actual_end - start;
                    chunks.push_back({chunk_text.substr(start, sub_chunk_len), sub_chunk_start_abs, sub_chunk_len});
                    
                    // Сдвигаемся назад на размер overlap для непрерывности контекста
                    size_t step = sub_chunk_len;
                    if (step <= (size_t)config.embedding_chunk_overlap) {
                        start = actual_end; // Защита от зависания
                    } else {
                        start = actual_end - config.embedding_chunk_overlap;
                    }
                }
            } else {
                chunks.push_back({std::move(chunk_text), start_byte, chunk_len});
            }
        }
        // Once a chunk candidate is processed, we treat it as an atomic unit and don't recurse deeper into it for more chunks.
        return; 
    }

    // If the node is not a chunk type (or not a top-level C++ declaration), recurse into its children to find potential chunks.
    uint32_t child_count = ts_node_child_count(tsNode);
    for (uint32_t i = 0; i < child_count; ++i) {
        TSNode child_node = ts_node_child(tsNode, i);
        if (ts_node_is_named(child_node)) { // We only want to recurse on named children
            extractChunks(sourceCode, child_node, chunks, language, depth + 1); // Рекурсивный вызов с увеличением глубины
        }
    }
}
