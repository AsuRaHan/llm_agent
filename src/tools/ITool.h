#pragma once

#include <string>
#include <nlohmann/json.hpp>

class ContextIndexer; // Forward declaration

class ITool {
public:
    virtual ~ITool() = default;

    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual nlohmann::json getParameters() const = 0;
    // Pass ContextIndexer as a pointer to allow tools to interact with the main index.
    virtual std::string execute(const nlohmann::json& args, ContextIndexer* indexer) = 0;
};