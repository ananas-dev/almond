#include "file_watcher.h"

#include "logger.h"

#include <SDL3/SDL_stdinc.h>
#include <libgen.h>
#include <sys/inotify.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

struct FileWatcher {
    FileWatcherEventCallback* callback;
    void *user_data;
    int fd;
    int wd;
    char buffer[BUF_LEN];
};

FileWatcher* create_file_watcher(const char* dir_path, FileWatcherEventCallback* callback, void* user_data)
{
    FileWatcher* watcher = SDL_malloc(sizeof(FileWatcher));

    watcher->callback = callback;
    watcher->user_data = user_data;

    watcher->fd = inotify_init1(IN_NONBLOCK);
    if (watcher->fd < 0) {
        SDL_free(watcher);
        return NULL;
    }

    watcher->wd = inotify_add_watch(watcher->fd, dir_path, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (watcher->wd < 0) {
        close(watcher->fd);
        SDL_free(watcher);
        return NULL;
    }

    return watcher;
}

void file_watcher_update(FileWatcher* file_watcher)
{
    ssize_t length = read(file_watcher->fd, file_watcher->buffer, BUF_LEN);
    if (length <= 0) {
        return;
    }

    size_t i = 0;
    while (i < (size_t)length) {
        struct inotify_event* event = (struct inotify_event*)&file_watcher->buffer[i];

        FileWatcherEvent file_watcher_event;
        file_watcher_event.file_name = event->name;
        file_watcher_event.user_data = file_watcher->user_data;

        if (event->mask & IN_CREATE) {
            file_watcher_event.type = FW_CREATE;
        } else if (event->mask & IN_MODIFY) {
            file_watcher_event.type = FW_MODIFY;
        } else if (event->mask & IN_DELETE) {
            file_watcher_event.type = FW_DELETE;
        } else {
            return;
        }

        file_watcher->callback(&file_watcher_event, file_watcher->user_data);

        i += EVENT_SIZE + event->len;
    }
}