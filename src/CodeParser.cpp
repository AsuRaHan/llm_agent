#include "CodeParser.h"
#include "Config.h"
#include "Logger.h"
#include "ContextIndexerHelper/ChunkerStrategy.h" // Для fallback-разбиения
#include <sstream>
#include <unordered_set>
#include <algorithm> // for std::all_of

// Объявляем функции грамматик tree-sitter, которые скомпилированы в бинарник
extern "C" {
    TSLanguage* tree_sitter_cpp();
    TSLanguage* tree_sitter_javascript();
    TSLanguage* tree_sitter_html();
    TSLanguage* tree_sitter_css();
    TSLanguage* tree_sitter_markdown();
    // TSLanguage* tree_sitter_php();
}

CodeParser::CodeParser(const Config& config)
    : config(config), parser(ts_parser_new())
{
    initializeLanguages();
    SPDLOG_INFO("CodeParser инициализирован с поддержкой tree-sitter.");
}

CodeParser::~CodeParser() {
    ts_parser_delete(parser);
    SPDLOG_INFO("CodeParser уничтожен, ресурсы tree-sitter освобождены.");
}

void CodeParser::initializeLanguages() {
    languages["cpp"] = tree_sitter_cpp();
    languages["javascript"] = tree_sitter_javascript();
    languages["html"] = tree_sitter_html();
    languages["css"] = tree_sitter_css();
    languages["markdown"] = tree_sitter_markdown();
    // languages["php"] = tree_sitter_php();
    // 'c' может использовать парсер 'cpp'
    languages["c"] = tree_sitter_cpp();
    
    SPDLOG_INFO("Загружено {} tree-sitter грамматик.", languages.size());
}

std::string CodeParser::detectLanguage(const std::string& extension)
{
    // This is a simplified mapping. A more robust solution might use a map.
    if (extension == ".cpp" || extension == ".hpp" || extension == ".cxx" || extension == ".hxx" || extension == ".cc" || extension == ".hh") return "cpp";
    if (extension == ".c" || extension == ".h") return "c";
    if (extension == ".js" || extension == ".jsx" || extension == ".mjs" || extension == ".cjs") return "javascript";
    // if (extension == ".html" || extension == ".htm") return "html"; // Временно отключено для использования fallback-разбиения
    // if (extension == ".css") return "css"; // Временно отключено для использования fallback-разбиения
    if (extension == ".md" || extension == ".markdown") return "markdown";
    // if (extension == ".php") return "php";
    // Add other languages here
    return "text"; // Default fallback
}

std::vector<CodeChunk> CodeParser::parse(const std::string& content, const std::string& extension)
{
    std::string lang = detectLanguage(extension);
    auto it = languages.find(lang);

    // Fallback для неподдерживаемых языков или пустого контента
    if (it == languages.end() || content.empty()) {
        if (it == languages.end()) {
            SPDLOG_DEBUG("Tree-sitter грамматика для '{}' (расширение '{}') не найдена. Используется fallback-разбиение.", lang, extension);
        }
        std::vector<CodeChunk> chunks;
        auto locations = ChunkerStrategy::splitFixedSize(content, config.embedding_max_text_length, config.embedding_chunk_overlap);
        for (const auto& loc : locations) {
            chunks.push_back({
                content.substr(loc.start_byte, loc.length),
                "text", // язык "text"
                loc.start_byte,
                loc.length
            });
        }
        return chunks;
    }

    // Настройка парсера tree-sitter
    TSLanguage* language = it->second;
    ts_parser_set_language(parser, language);
    TSTree* tree = ts_parser_parse_string(parser, NULL, content.c_str(), content.length());
    TSNode root_node = ts_tree_root_node(tree);

    std::vector<CodeChunk> chunks;
    // Рекурсивная функция выполняет основную работу
    collectChunks(root_node, content, chunks, lang);

    ts_tree_delete(tree);

    // Если tree-sitter ничего не вернул, снова используем fallback
    if (chunks.empty() && !content.empty()) {
        SPDLOG_WARN("Tree-sitter не смог разбить код на семантические чанки для '{}'. Используется fallback-разбиение.", extension);
        return parse(content, ".txt"); // Повторный запуск с fallback
    }

    SPDLOG_DEBUG("Код разбит на {} чанков с помощью tree-sitter для языка '{}'.", chunks.size(), lang);
    return chunks;
}

const std::unordered_set<std::string>& CodeParser::getChunkableNodeTypes(const std::string& language) {
    // Определяем, какие типы узлов считать самостоятельными чанками
    static const std::unordered_set<std::string> cpp_nodes = {
        "function_definition", "class_specifier", "struct_specifier", "enum_specifier", 
        "template_declaration", "concept_definition", "declaration"
    };
    static const std::unordered_set<std::string> js_nodes = {
        "function_declaration", "class_declaration", "lexical_declaration", // const/let
        "variable_declaration", // var
        "export_statement"
    };
    // Для HTML и CSS стратегия другая - лучше брать блоки верхнего уровня
    static const std::unordered_set<std::string> html_nodes = { "script_element", "style_element" };
    static const std::unordered_set<std::string> css_nodes = { "rule_set", "at_rule" };
    static const std::unordered_set<std::string> md_nodes = { "section", "fenced_code_block" };
    static const std::unordered_set<std::string> php_nodes = { "function_declaration", "class_declaration", "property_declaration", "variable_declaration", "use_declaration", "namespace_declaration" };
    static const std::unordered_set<std::string> empty_set;

    if (language == "cpp" || language == "c") return cpp_nodes;
    if (language == "javascript") return js_nodes;
    if (language == "html") return html_nodes;
    if (language == "css") return css_nodes;
    if (language == "markdown") return md_nodes;
    if (language == "php") return php_nodes;
    
    return empty_set;
}

void CodeParser::collectChunks(TSNode node, const std::string& content, std::vector<CodeChunk>& chunks, const std::string& language) {
    const auto& chunkable_types = getChunkableNodeTypes(language);
    
    size_t last_chunk_end = ts_node_start_byte(node);
    uint32_t child_count = ts_node_named_child_count(node);

    for (uint32_t i = 0; i < child_count; ++i) {
        TSNode child = ts_node_named_child(node, i);
        size_t child_start = ts_node_start_byte(child);
        std::string child_type = ts_node_type(child);

        // 1. Захватываем текст "между" чанками (комментарии, пустые строки и т.д.)
        if (child_start > last_chunk_end) {
            addChunk(content.substr(last_chunk_end, child_start - last_chunk_end), last_chunk_end, language, chunks);
        }

        // 2. Обрабатываем сам дочерний узел
        if (chunkable_types.count(child_type)) {
            // Этот узел - самостоятельный чанк. Добавляем его.
            addChunk(content.substr(child_start, ts_node_end_byte(child) - child_start), child_start, language, chunks);
            last_chunk_end = ts_node_end_byte(child);
        } else {
            // Этот узел - контейнер (например, namespace) или что-то еще. Рекурсивно спускаемся в него.
            collectChunks(child, content, chunks, language);
            last_chunk_end = ts_node_end_byte(child);
        }
    }
    
    // 3. Захватываем "хвост" - текст после последнего дочернего узла до конца текущего узла
    size_t current_node_end = ts_node_end_byte(node);
    if (current_node_end > last_chunk_end) {
        addChunk(content.substr(last_chunk_end, current_node_end - last_chunk_end), last_chunk_end, language, chunks);
    }
}

void CodeParser::addChunk(const std::string& text, size_t start_byte, const std::string& language, std::vector<CodeChunk>& chunks) {
    // Игнорируем пустые или состоящие только из пробелов чанки
    if (text.empty() || std::all_of(text.begin(), text.end(), [](unsigned char c){ return std::isspace(c); })) {
        return;
    }

    // "Гибридная" стратегия: если семантический чанк слишком большой, разбиваем его дальше
    if (text.length() > config.embedding_max_text_length) {
        SPDLOG_DEBUG("Семантический чанк ({} байт) слишком большой. Разбиваем его дальше.", text.length());
        auto sub_locations = ChunkerStrategy::splitFixedSize(text, config.embedding_max_text_length, config.embedding_chunk_overlap);
        for (const auto& loc : sub_locations) {
            chunks.push_back({
                text.substr(loc.start_byte, loc.length),
                language,
                start_byte + loc.start_byte,
                loc.length
            });
        }
    } else {
        chunks.push_back({
            text,
            language,
            start_byte,
            text.length()
        });
    }
}