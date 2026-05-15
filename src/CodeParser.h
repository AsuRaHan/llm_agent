#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declare Tree-sitter types to avoid including C headers in a C++ header
struct TSLanguage;
struct TSParser;
struct TSNode;
struct Config; // Forward declare Config

struct CodeChunk {
    std::string text;
    size_t start_byte;
    size_t length;
};

class CodeParser {
public:
    explicit CodeParser(const Config& config);
    ~CodeParser();

    /**
     * @brief Parses the given source code and extracts logical chunks (functions, classes, etc.).
     * 
     * @param sourceCode The source code to parse.
     * @param fileExtension The extension of the file (e.g., ".cpp", ".h").
     * @return A vector of strings, where each string is a logical chunk of code.
     */
    std::vector<CodeChunk> parse(const std::string& sourceCode, const std::string& fileExtension); // Обновлен тип возвращаемого значения

private:
    const Config& config;
    // A map from file extension (e.g., ".cpp") to the corresponding Tree-sitter language parser.
    std::unordered_map<std::string, TSLanguage*> languageMap;
    
    // The main Tree-sitter parser object. It can be reused by setting a new language.
    TSParser* parser;

    void initializeLanguages();
    void registerLanguage(const std::vector<std::string>& extensions, TSLanguage* language);
    
// Helper to recursively extract nodes, now passes TSLanguage* for language-specific chunking
    void extractChunks(const std::string& sourceCode, TSNode tsNode, std::vector<CodeChunk>& chunks, TSLanguage* language);
};
