#include "CodeParser.h"
#include "Config.h"
#include "Logger.h"
#include <spdlog/spdlog.h>

// TODO: Добавить tree-sitter парсеры
// Для начала используем базовую реализацию

CodeParser::CodeParser(const Config& config)
    : config(config)
{
    SPDLOG_INFO("CodeParser инициализирован.");
}

CodeParser::~CodeParser()
{
    SPDLOG_INFO("CodeParser уничтожен.");
}

std::string CodeParser::detectLanguage(const std::string& extension)
{
    // Простая маппинг расширений на языки tree-sitter
    if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".h" || extension == ".hpp") {
        return "cpp";
    }
    else if (extension == ".py") {
        return "python";
    }
    else if (extension == ".js" || extension == ".ts") {
        return "javascript";
    }
    else if (extension == ".java") {
        return "java";
    }
    else if (extension == ".go") {
        return "go";
    }
    else if (extension == ".rs") {
        return "rust";
    }
    else if (extension == ".rb") {
        return "ruby";
    }
    else if (extension == ".php") {
        return "php";
    }
    else if (extension == ".swift") {
        return "swift";
    }
    else if (extension == ".kt" || extension == ".kts") {
        return "kotlin";
    }
    else if (extension == ".scala") {
        return "scala";
    }
    else if (extension == ".hs") {
        return "haskell";
    }
    else if (extension == ".clj" || extension == ".cljs") {
        return "clojure";
    }
    else if (extension == ".erl" || extension == ".hrl") {
        return "erlang";
    }
    else if (extension == ".ex" || extension == ".exs") {
        return "elixir";
    }
    else if (extension == ".lua") {
        return "lua";
    }
    else if (extension == ".pl" || extension == ".pm") {
        return "perl";
    }
    else if (extension == ".r" || extension == ".R") {
        return "r";
    }
    else if (extension == ".sh" || extension == ".bash" || extension == ".zsh") {
        return "bash";
    }
    else if (extension == ".sql") {
        return "sql";
    }
    else if (extension == ".html" || extension == ".htm") {
        return "html";
    }
    else if (extension == ".css") {
        return "css";
    }
    else if (extension == ".scss" || extension == ".sass") {
        return "scss";
    }
    else if (extension == ".json") {
        return "json";
    }
    else if (extension == ".xml") {
        return "xml";
    }
    else if (extension == ".yaml" || extension == ".yml") {
        return "yaml";
    }
    else if (extension == ".toml") {
        return "toml";
    }
    else if (extension == ".ini") {
        return "ini";
    }
    else if (extension == ".md" || extension == ".markdown") {
        return "markdown";
    }
    else if (extension == ".tex") {
        return "latex";
    }
    else if (extension == ".c") {
        return "c";
    }
    else if (extension == ".hxx" || extension == ".hh") {
        return "cpp";
    }
    else {
        SPDLOG_WARN("Неизвестное расширение файла: {}. Используем fallback.", extension);
        return "text";
    }
}

std::vector<CodeChunk> CodeParser::parse(const std::string& content, const std::string& extension)
{
    std::vector<CodeChunk> chunks;
    
    // TODO: Реализовать парсинг с tree-sitter
    // Для начала используем базовое разбиение по строкам
    
    // Временная реализация: разбиваем на строки
    std::istringstream stream(content);
    std::string line;
    size_t current_pos = 0;
    
    while (std::getline(stream, line)) {
        CodeChunk chunk;
        chunk.text = line;
        chunk.start_byte = current_pos;
        chunk.length = line.length() + 1; // +1 для переноса строки
        chunk.language = detectLanguage(extension);
        chunks.push_back(chunk);
        current_pos += chunk.length;
    }
    
    SPDLOG_DEBUG("Код разбит на {} чанков.", chunks.size());
    return chunks;
}
