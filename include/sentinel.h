#ifndef IR_SENTINEL_H
#define IR_SENTINEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SENTINEL_VERSION "0.1.0"
#define SENTINEL_MAX_PATH 4096
#define SENTINEL_MAX_CMD 8192

struct sentinel_config {
    const char *proc_root;
    const char *etc_root;
    const char *varlog_root;
    unsigned interval_seconds;
    bool monitor_processes;
    bool monitor_network;
    bool monitor_files;
};

struct proc_info {
    pid_t pid;
    pid_t ppid;
    uid_t uid;
    char user[128];
    char comm[256];
    char cmdline[SENTINEL_MAX_CMD];
};

void sentinel_default_config(struct sentinel_config *cfg);
int sentinel_run(const struct sentinel_config *cfg);

#endif
