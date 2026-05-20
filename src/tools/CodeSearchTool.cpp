#include "CodeSearchTool.h"
#include "ContextIndexer.h" // We need the full definition here
#include "Logger.h"
#include <sstream>
#include <iomanip> // for std::setprecision

std::string CodeSearchTool::getName() const {
    return "code_search";
}

std::string CodeSearchTool::getDescription() const {
    return "Performs a semantic search over the indexed codebase for a given query string. Returns the most relevant code chunks.";
}

nlohmann::json CodeSearchTool::getParameters() const {
    return nlohmann::json::parse(R"({
        "type": "object",
        "properties": {
            "query": {
                "type": "string",
                "description": "The natural language query to search for in the code."
            },
            "k": {
                "type": "integer",
                "description": "Optional. The number of top results to return. Defaults to 5."
            }
        },
        "required": ["query"]
    })");
}

std::string CodeSearchTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    if (!indexer) {
        return "Error: ContextIndexer is not available to perform search.";
    }
    if (!args.contains("query")) {
        return "Error: 'query' argument is missing for code_search.";
    }

    std::string query = args["query"].get<std::string>();
    int k = 5; // Default value
    if (args.contains("k") && args["k"].is_number_integer()) {
        k = args["k"].get<int>();
    }

    SPDLOG_INFO("[Tool:code_search] Searching for: '{}' with k={}", query, k);

    auto& searcher = indexer->getSearcher();
    auto& fileIndex = indexer->getFileIndexer().getFileIndex();
    std::vector<SearchResult> results = searcher.findTopK(query, k, fileIndex);

    if (results.empty()) {
        return "No relevant code chunks found for the query: '" + query + "'";
    }

    std::stringstream result_ss;
    result_ss << "Found " << results.size() << " relevant code chunks for query '" << query << "':\n\n";
    for (const auto& res : results) {
        result_ss << "--- FROM FILE: " << res.filePath 
                  << " (relevance score: " << std::fixed << std::setprecision(3) << res.score << ") ---\n"
                  << "```\n"
                  << res.chunkText << "\n"
                  << "```\n\n";
    }

    return result_ss.str();
}