#include "file_watch.h"
#include "json_emit.h"
#include "log.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define SENTINEL_DEFAULT_WATCH_COUNT 6
#define SENTINEL_MAX_ACTIVE_WATCHES (SENTINEL_MAX_WATCH_PATHS + SENTINEL_DEFAULT_WATCH_COUNT)

struct watch_entry {
    int wd;
    bool warned_missing;
    bool ever_armed;
    char path[SENTINEL_MAX_PATH];
};

static int ifd = -1;
static struct watch_entry watches[SENTINEL_MAX_ACTIVE_WATCHES];
static size_t watch_count;

static const char *default_watch_suffixes[SENTINEL_DEFAULT_WATCH_COUNT] = {
    "passwd",
    "shadow",
    "group",
    "sudoers",
    "sudoers.d",
    "ssh",
};

static int map_host_path(const struct sentinel_config *cfg, const char *requested,
                         char *out, size_t out_len) {
    const char *root = NULL;
    const char *suffix = NULL;

    if (strcmp(requested, "/etc") == 0 || strncmp(requested, "/etc/", 5) == 0) {
        root = cfg->etc_root;
        suffix = requested + 4;
    } else if (strcmp(requested, "/var/log") == 0 || strncmp(requested, "/var/log/", 9) == 0) {
        root = cfg->varlog_root;
        suffix = requested + 8;
    } else if (strcmp(requested, "/proc") == 0 || strncmp(requested, "/proc/", 6) == 0) {
        root = cfg->proc_root;
        suffix = requested + 5;
    }

    int written = root ? snprintf(out, out_len, "%s%s", root, suffix)
                       : snprintf(out, out_len, "%s", requested);
    return (written >= 0 && (size_t)written < out_len) ? 0 : -1;
}

static int arm_watch(struct watch_entry *entry) {
    uint32_t mask = IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE |
                    IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF;
    int wd = inotify_add_watch(ifd, entry->path, mask);
    if (wd < 0) {
        if (!entry->warned_missing) {
            log_warn("cannot watch %s: %s", entry->path, strerror(errno));
            entry->warned_missing = true;
        }
        entry->wd = -1;
        return -1;
    }

    bool rearming = entry->ever_armed;
    entry->wd = wd;
    entry->warned_missing = false;
    entry->ever_armed = true;
    log_info("%swatching %s", rearming ? "re-" : "", entry->path);
    return 0;
}

static int add_watch_path(const char *path) {
    for (size_t i = 0; i < watch_count; i++) {
        if (strcmp(watches[i].path, path) == 0) return 0;
    }
    if (watch_count >= SENTINEL_MAX_ACTIVE_WATCHES) {
        log_warn("watch limit reached; skipping %s", path);
        return -1;
    }

    struct watch_entry *entry = &watches[watch_count++];
    memset(entry, 0, sizeof(*entry));
    entry->wd = -1;
    int written = snprintf(entry->path, sizeof(entry->path), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(entry->path)) {
        watch_count--;
        return -1;
    }
    arm_watch(entry);
    return 0;
}

static struct watch_entry *find_watch(int wd) {
    for (size_t i = 0; i < watch_count; i++) {
        if (watches[i].wd == wd) return &watches[i];
    }
    return NULL;
}

static void rearm_missing(void) {
    for (size_t i = 0; i < watch_count; i++) {
        if (watches[i].wd < 0) arm_watch(&watches[i]);
    }
}

int file_watch_init(const struct sentinel_config *cfg) {
    file_watch_close();
    memset(watches, 0, sizeof(watches));
    watch_count = 0;

    ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ifd < 0) {
        log_warn("inotify unavailable: %s", strerror(errno));
        return -1;
    }

    if (cfg->default_watch_paths) {
        for (size_t i = 0; i < SENTINEL_DEFAULT_WATCH_COUNT; i++) {
            char path[SENTINEL_MAX_PATH];
            int written = snprintf(path, sizeof(path), "%s/%s", cfg->etc_root, default_watch_suffixes[i]);
            if (written >= 0 && (size_t)written < sizeof(path)) add_watch_path(path);
        }
    }

    for (size_t i = 0; i < cfg->watch_path_count; i++) {
        char path[SENTINEL_MAX_PATH];
        if (map_host_path(cfg, cfg->watch_paths[i], path, sizeof(path)) != 0) {
            log_warn("resolved watch path too long: %s", cfg->watch_paths[i]);
            continue;
        }
        add_watch_path(path);
    }
    return 0;
}

static const char *mask_action(uint32_t mask) {
    if (mask & IN_CREATE) return "create";
    if (mask & IN_CLOSE_WRITE) return "close_write";
    if (mask & IN_MODIFY) return "modify";
    if (mask & IN_DELETE) return "delete";
    if (mask & IN_MOVED_FROM) return "move_from";
    if (mask & IN_MOVED_TO) return "move_to";
    if (mask & IN_ATTRIB) return "attrib";
    if (mask & IN_DELETE_SELF) return "delete_self";
    if (mask & IN_MOVE_SELF) return "move_self";
    return "unknown";
}

int file_watch_poll(void) {
    if (ifd < 0) return -1;
    rearm_missing();

    char buf[8192] __attribute__((aligned(__alignof__(struct inotify_event))));
    for (;;) {
        ssize_t n = read(ifd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            if (errno == EINTR) continue;
            log_warn("inotify read failed: %s", strerror(errno));
            return -1;
        }
        if (n == 0) return 0;

        for (char *p = buf; p < buf + n;) {
            struct inotify_event *ev = (struct inotify_event *)p;
            struct watch_entry *entry = find_watch(ev->wd);
            if (entry) {
                char full[SENTINEL_MAX_PATH];
                int written;
                if (ev->len && ev->name[0]) written = snprintf(full, sizeof(full), "%s/%s", entry->path, ev->name);
                else written = snprintf(full, sizeof(full), "%s", entry->path);

                if ((ev->mask & IN_IGNORED) != 0) entry->wd = -1;
                if ((ev->mask & ~IN_IGNORED) != 0 && written >= 0 && (size_t)written < sizeof(full)) {
                    emit_file_change(full, mask_action(ev->mask));
                }
            }
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
}

void file_watch_close(void) {
    if (ifd >= 0) close(ifd);
    ifd = -1;
    watch_count = 0;
}
