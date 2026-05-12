#pragma once

#include <string>
#include <httplib.h>

// Forward declarations to avoid including full headers
struct SearchResult;
struct Config; // Forward declaration

class AssistantRole {
public:
    explicit AssistantRole(const Config& config);
    std::string answerWithContext(const std::string& userQuery, const std::vector<SearchResult>& searchResults);
    std::string generateProjectSummaryGreeting(int file_count, int embedding_count);
    std::string generateChunkSummary(const std::string& codeChunk, const std::string& chunkName);

private:
    const Config& config;
    httplib::Client cli;
};
