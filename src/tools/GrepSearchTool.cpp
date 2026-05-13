#include "GrepSearchTool.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <string>
#include <vector>
#include <unordered_set>

namespace fs = std::filesystem;

// Helper to convert glob-style wildcards to a regex string.
static std::string wildcard_to_regex(const std::string& wildcard) {
    std::string regex_str;
    regex_str.reserve(wildcard.size() * 2);
    for (char c : wildcard) {
        switch (c) {
            case '*':
                regex_str += ".*";
                break;
            case '?':
                regex_str += '.';
                break;
            // Escape special regex characters
            case '.': case '+': case '(': case ')': case '{': case '}':
            case '[': case ']': case '\\': case '|': case '^': case '$':
                regex_str += '\\';
                regex_str += c;
                break;
            default:
                regex_str += c;
                break;
        }
    }
    return regex_str;
}

std::string GrepSearchTool::getName() const {
    return "grep_search";
}

std::string GrepSearchTool::getDescription() const {
    return "Searches for a specific regex pattern within files in the project, similar to the 'grep' command. Can be filtered by a file path glob pattern.";
}

nlohmann::json GrepSearchTool::getParameters() const {
    return nlohmann::json::parse(R"({
        "type": "object",
        "properties": {
            "pattern": {
                "type": "string",
                "description": "The ECMA-262 (JavaScript) regular expression to search for."
            },
            "path_pattern": {
                "type": "string",
                "description": "Optional. A glob pattern to filter file paths (e.g., 'src/**/*.cpp', '*.h'). Defaults to all files ('*')."
            }
        },
        "required": ["pattern"]
    })");
}

std::string GrepSearchTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    (void)indexer; // This tool doesn't use the indexer.

    if (!args.contains("pattern")) {
        return "Error: 'pattern' argument is missing for grep_search.";
    }

    std::string pattern_str = args["pattern"].get<std::string>();
    std::string path_pattern_str = "*";
    if (args.contains("path_pattern") && args["path_pattern"].is_string()) {
        path_pattern_str = args["path_pattern"].get<std::string>();
    }

    SPDLOG_INFO("[Tool:grep_search] Searching for regex '{}' in files matching '{}'", pattern_str, path_pattern_str);

    std::regex pattern_regex;
    try {
        pattern_regex = std::regex(pattern_str, std::regex::ECMAScript);
    } catch (const std::regex_error& e) {
        SPDLOG_ERROR("[Tool:grep_search] Invalid regex pattern '{}': {}", pattern_str, e.what());
        return "Error: Invalid regular expression pattern provided: " + std::string(e.what());
    }
    
    std::regex path_regex(wildcard_to_regex(path_pattern_str), std::regex::ECMAScript);

    std::stringstream results_ss;
    int matches_found = 0;
    const int max_matches = 100; // To avoid flooding the context

    const std::unordered_set<std::string> ignored_dirs = {".git", "build", ".shdata", "CMakeFiles"};

    try {
        auto it = fs::recursive_directory_iterator(".", fs::directory_options::skip_permission_denied);
        for (const auto& entry : it) {
            if (entry.is_directory() && ignored_dirs.count(entry.path().filename().string())) {
                it.disable_recursion_pending();
                continue;
            }

            if (entry.is_regular_file()) {
                std::string file_path_str = fs::relative(entry.path()).string();
                std::replace(file_path_str.begin(), file_path_str.end(), '\\', '/');

                if (std::regex_match(file_path_str, path_regex)) {
                    std::ifstream file(entry.path());
                    if (!file.is_open()) continue;

                    std::string line;
                    int line_num = 1;
                    while (std::getline(file, line)) {
                        if (std::regex_search(line, pattern_regex)) {
                            if (matches_found >= max_matches) {
                                results_ss << "\n... (more matches found, but output is truncated to " << max_matches << " lines)\n";
                                goto end_loop;
                            }
                            results_ss << file_path_str << ":" << line_num << ":" << line << "\n";
                            matches_found++;
                        }
                        line_num++;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Tool:grep_search] Exception during file iteration: {}", e.what());
        return "Error: An exception occurred while searching files: " + std::string(e.what());
    }

end_loop:;

    if (matches_found == 0) {
        return "No matches found for the given pattern.";
    }

    return "Found " + std::to_string(matches_found) + " match(es):\n" + results_ss.str();
}