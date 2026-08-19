#ifndef SENTINEL_H
#define SENTINEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SENTINEL_VERSION "0.2.0"
#define SENTINEL_MAX_PATH 4096
#define SENTINEL_MAX_CMD 8192
#define SENTINEL_MAX_WATCH_PATHS 64

struct sentinel_config {
    const char *proc_root;
    const char *etc_root;
    const char *varlog_root;
    unsigned interval_seconds;
    bool monitor_processes;
    bool monitor_network;
    bool monitor_files;
    bool emit_baseline;
    bool default_watch_paths;
    bool run_once;
    size_t watch_path_count;
    char *watch_paths[SENTINEL_MAX_WATCH_PATHS];
};

struct proc_info {
    pid_t pid;
    pid_t ppid;
    uid_t uid;
    uint64_t start_ticks;
    char user[128];
    char comm[256];
    char cmdline[SENTINEL_MAX_CMD];
};

void sentinel_default_config(struct sentinel_config *cfg);
void sentinel_config_destroy(struct sentinel_config *cfg);
int sentinel_config_add_watch(struct sentinel_config *cfg, const char *path);
int sentinel_load_config_file(struct sentinel_config *cfg, const char *path);
int sentinel_run(const struct sentinel_config *cfg);

#endif
