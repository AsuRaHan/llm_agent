#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>

// Forward declarations
class ContextIndexer;

class FileWatcher {
public:
    FileWatcher(ContextIndexer& indexer);
    ~FileWatcher();

    // Starts watching in a background thread.
    void start(const std::string& directory);

    // Stops the watcher.
    void stop();

private:
    void run(); // The main loop for the watcher thread.

    ContextIndexer& indexer;
    std::string directory_to_watch;
    std::thread watcher_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> index_is_dirty{false};
    std::chrono::steady_clock::time_point last_change_time;
};