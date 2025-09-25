#pragma once

typedef struct FileWatcher FileWatcher;

typedef enum {
    FW_CREATE,
    FW_MODIFY,
    FW_DELETE,
} FileWatcherEventType;

typedef struct {
    FileWatcherEventType type;
    const char* file_name;
    void* user_data;
} FileWatcherEvent;

typedef void FileWatcherEventCallback(FileWatcherEvent* event, void* user_data);

FileWatcher* create_file_watcher(const char* path, FileWatcherEventCallback* callback, void* user_data);
void file_watcher_update(FileWatcher* file_watcher);


