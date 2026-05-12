#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declare Tree-sitter types to avoid including C headers in a C++ header
struct TSLanguage;
struct TSParser;
struct Config; // Forward declare Config

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
    std::vector<std::string> parse(const std::string& sourceCode, const std::string& fileExtension);

private:
    const Config& config;
    // A map from file extension (e.g., ".cpp") to the corresponding Tree-sitter language parser.
    std::unordered_map<std::string, TSLanguage*> languageMap;
    
    // The main Tree-sitter parser object. It can be reused by setting a new language.
    TSParser* parser;

    void initializeLanguages();
    void registerLanguage(const std::vector<std::string>& extensions, TSLanguage* language);
    
    // Helper to recursively extract nodes
    void extractChunks(const std::string& sourceCode, void* rootNode, std::vector<std::string>& chunks);
    std::vector<std::string> fixedSizeChunkText(const std::string& text, size_t chunkSize, size_t overlap);
};