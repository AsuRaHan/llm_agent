#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <unordered_set>
#include "tree_sitter/api.h"

// Forward declare Tree-sitter types to avoid including C headers in a C++ header
struct Config; // Forward declare Config

struct CodeChunk {
    std::string text;
    std::string language;
    size_t start_byte;
    size_t length;
};

class CodeParser {
public:
    explicit CodeParser(const Config& config);
    ~CodeParser();

    // Запрещаем копирование и перемещение, т.к. управляем сырым указателем TSParser
    CodeParser(const CodeParser&) = delete;
    CodeParser& operator=(const CodeParser&) = delete;
    CodeParser(CodeParser&&) = delete;
    CodeParser& operator=(CodeParser&&) = delete;

    std::vector<CodeChunk> parse(const std::string& content, const std::string& extension);
    std::string detectLanguage(const std::string& extension);

private:
    const Config& config;
    TSParser* parser;
    std::unordered_map<std::string, TSLanguage*> languages;

    void initializeLanguages();
    void collectChunks(TSNode node, const std::string& content, std::vector<CodeChunk>& chunks, const std::string& language);
    void addChunk(const std::string& text, size_t start_byte, const std::string& language, std::vector<CodeChunk>& chunks);
    const std::unordered_set<std::string>& getChunkableNodeTypes(const std::string& language);
};