#include "file_watch.h"
#include "json_emit.h"
#include "log.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

static int ifd = -1;
static char watch_roots[16][SENTINEL_MAX_PATH];
static int watch_count = 0;

static void add_watch_path(const char *path) {
    if (watch_count >= 16) return;
    int wd = inotify_add_watch(ifd, path, IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB);
    if (wd < 0) {
        log_warn("cannot watch %s: %s", path, strerror(errno));
        return;
    }
    snprintf(watch_roots[watch_count++], SENTINEL_MAX_PATH, "%s", path);
    log_info("watching %s", path);
}

int file_watch_init(const struct sentinel_config *cfg) {
    ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ifd < 0) {
        log_warn("inotify unavailable: %s", strerror(errno));
        return -1;
    }

    char path[SENTINEL_MAX_PATH];
    snprintf(path, sizeof(path), "%s/passwd", cfg->etc_root); add_watch_path(path);
    snprintf(path, sizeof(path), "%s/shadow", cfg->etc_root); add_watch_path(path);
    snprintf(path, sizeof(path), "%s/group", cfg->etc_root); add_watch_path(path);
    snprintf(path, sizeof(path), "%s/sudoers", cfg->etc_root); add_watch_path(path);
    snprintf(path, sizeof(path), "%s/sudoers.d", cfg->etc_root); add_watch_path(path);
    snprintf(path, sizeof(path), "%s/ssh", cfg->etc_root); add_watch_path(path);
    return 0;
}

static const char *mask_action(uint32_t mask) {
    if (mask & IN_CREATE) return "create";
    if (mask & IN_MODIFY) return "modify";
    if (mask & IN_DELETE) return "delete";
    if (mask & IN_MOVED_FROM) return "move_from";
    if (mask & IN_MOVED_TO) return "move_to";
    if (mask & IN_ATTRIB) return "attrib";
    return "unknown";
}

int file_watch_poll(void) {
    if (ifd < 0) return -1;
    char buf[8192] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t n = read(ifd, buf, sizeof(buf));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        log_warn("inotify read failed: %s", strerror(errno));
        return -1;
    }

    for (char *p = buf; p < buf + n;) {
        struct inotify_event *ev = (struct inotify_event *)p;
        char full[SENTINEL_MAX_PATH];
        const char *root = "unknown";
        if (ev->wd > 0 && ev->wd <= watch_count) root = watch_roots[ev->wd - 1];
        if (ev->len && ev->name[0]) snprintf(full, sizeof(full), "%s/%s", root, ev->name);
        else snprintf(full, sizeof(full), "%s", root);
        emit_file_change(full, mask_action(ev->mask));
        p += sizeof(struct inotify_event) + ev->len;
    }
    return 0;
}

void file_watch_close(void) {
    if (ifd >= 0) close(ifd);
    ifd = -1;
}
