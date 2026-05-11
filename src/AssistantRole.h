#pragma once

#include <string>
#include <httplib.h>

struct Config; // Forward declaration

class AssistantRole {
public:
    explicit AssistantRole(const Config& config);
    std::string analyzeCode(const std::string& filePath, const std::string& fileContent, const std::string& userQuery);

private:
    const Config& config;
    httplib::Client cli;
};
