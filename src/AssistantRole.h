#pragma once

#include <string>
#include "httplib.h"
#include "nlohmann/json.hpp"

class AssistantRole {
public:
    AssistantRole();
    std::string analyzeCode(const std::string& filePath, const std::string& fileContent, const std::string& userQuery);

private:
    httplib::Client cli;
};
