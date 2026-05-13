 #pragma once

#include <string>
#include <vector>
#include <httplib.h>
 
#include <nlohmann/json.hpp>
#include <memory>
 
class ContextIndexer; // Forward declare for processQuery
class ToolManager;    // Forward declare for the member
// Forward declarations to avoid including full headers
struct SearchResult;
struct Config; // Forward declaration

class AssistantRole {
public:
    explicit AssistantRole(const Config& config);
    ~AssistantRole(); // Required for pimpl with unique_ptr

    /**
     * @brief Processes a user query, potentially using tools and context, to generate a final answer.
     * This method orchestrates the conversation with the LLM, including RAG and tool calls.
     * @param userQuery The user's question.
     * @param initialContext The initial set of relevant chunks from the vector search.
     * @param indexer A reference to the main indexer, allowing tools to perform actions like `code_search`.
     * @return The final, user-facing answer from the assistant.
     */
    std::string processQuery(const std::string& userQuery, const std::vector<SearchResult>& initialContext, ContextIndexer& indexer);

    std::string generateProjectSummaryGreeting(int file_count, int embedding_count);
    std::string generateChunkSummary(const std::string& codeChunk, const std::string& chunkName);

private:
    const Config& config;
    httplib::Client cli;
    std::unique_ptr<ToolManager> toolManager;
};
