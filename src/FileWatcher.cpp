#include "FileWatcher.h"
#include "ContextIndexer.h"
#include "Logger.h"
#include <filesystem>
#include <chrono>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/inotify.h>
#include <unistd.h>
#include <map>
#endif

namespace fs = std::filesystem;

FileWatcher::FileWatcher(ContextIndexer& indexer) : indexer(indexer) {}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start(const std::string& directory) {
    if (running) {
        return;
    }
    directory_to_watch = directory;
    running = true;
    watcher_thread = std::thread(&FileWatcher::run, this);
    SPDLOG_INFO("[FileWatcher] Запущен мониторинг директории: {}", directory);
}

void FileWatcher::stop() {
    if (running) {
        running = false;
        if (watcher_thread.joinable()) {
            watcher_thread.join();
        }
        SPDLOG_INFO("[FileWatcher] Мониторинг остановлен.");
    }
}

void FileWatcher::freeze() {
    is_frozen.store(true);
    SPDLOG_INFO("[FileWatcher] Заморозка активирована. Обработка изменений приостановлена.");
}

void FileWatcher::unfreeze() {
    is_frozen.store(false);
    SPDLOG_INFO("[FileWatcher] Заморозка снята. Обработка изменений возобновлена.");
}

bool FileWatcher::isFrozen() const {
    return is_frozen.load();
}

void FileWatcher::run() {
#ifdef _WIN32
    HANDLE hDir = CreateFile(
        directory_to_watch.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        SPDLOG_ERROR("[FileWatcher] Не удалось получить handle для директории: {}. Код ошибки: {}", directory_to_watch, GetLastError());
        return;
    }

    const int buffer_size = 4096;
    std::vector<BYTE> buffer(buffer_size);
    DWORD bytesReturned;

    OVERLAPPED overlapped;
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (running) {
        if (!ReadDirectoryChangesW(
            hDir,
            buffer.data(),
            buffer_size,
            TRUE, // watch subtree
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            &overlapped,
            NULL
        )) {
            // This can happen if the handle is closed.
            break;
        }

        DWORD waitStatus = WaitForSingleObject(overlapped.hEvent, 1000); // 1 sec timeout

        if (!running) break;

        if (waitStatus == WAIT_OBJECT_0) {
            if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE)) {
                FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)buffer.data();
                while (pNotify) {
                    wchar_t filename_w[MAX_PATH];
                    wcsncpy_s(filename_w, pNotify->FileName, pNotify->FileNameLength / sizeof(wchar_t));
                    
                    char filename_utf8[MAX_PATH * 4];
                    WideCharToMultiByte(CP_UTF8, 0, filename_w, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);

                    fs::path full_path = fs::path(directory_to_watch) / filename_utf8;
                    auto canonical_path_str = fs::weakly_canonical(full_path).string();
                    std::replace(canonical_path_str.begin(), canonical_path_str.end(), '\\', '/');

                    switch (pNotify->Action) {
                        case FILE_ACTION_ADDED:
                        case FILE_ACTION_MODIFIED:
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            SPDLOG_INFO("[FileWatcher] Обнаружено изменение/создание файла: {}", canonical_path_str);
                            last_change_time = std::chrono::steady_clock::now();
                            indexer.reindexFile(canonical_path_str);
                            index_is_dirty = true;
                            break;
                        case FILE_ACTION_REMOVED:
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            SPDLOG_INFO("[FileWatcher] Обнаружено удаление/переименование файла: {}", canonical_path_str);
                            indexer.removeFileFromIndex(canonical_path_str);
                            index_is_dirty = true;
                            break;
                    }

                    if (pNotify->NextEntryOffset == 0) break;
                    pNotify = (FILE_NOTIFY_INFORMATION*)((BYTE*)pNotify + pNotify->NextEntryOffset);
                }
            }
        }

        // Debounced save: if there are changes and some time has passed, save the index.
        if (index_is_dirty && std::chrono::steady_clock::now() - last_change_time > std::chrono::seconds(5)) {
            SPDLOG_INFO("[FileWatcher] Сохранение накопленных изменений индекса...");
            indexer.saveIndex();
            index_is_dirty = false;
        }
    }

    // Final save before the thread exits, if there are any pending changes.
    if (index_is_dirty) {
        SPDLOG_INFO("[FileWatcher] Сохранение финальных изменений индекса перед завершением потока...");
        indexer.saveIndex();
        index_is_dirty = false;
    }

    CloseHandle(hDir);
    CloseHandle(overlapped.hEvent);
#else // Linux inotify implementation
    SPDLOG_WARN("[FileWatcher] Мониторинг файлов в реальном времени на Linux пока не поддерживается.");
    // Keep the thread alive but idle until stop() is called.
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
#endif
}