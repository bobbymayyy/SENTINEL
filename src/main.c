#include "sentinel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s [--host-roots] [--interval seconds]\n"
        "\n"
        "  --host-roots       use /host/proc, /host/etc, /host/var/log for container sidecar mode\n"
        "  --interval SEC     polling interval, default 2\n",
        argv0);
}

int main(int argc, char **argv) {
    struct sentinel_config cfg;
    sentinel_default_config(&cfg);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host-roots") == 0) {
            cfg.proc_root = "/host/proc";
            cfg.etc_root = "/host/etc";
            cfg.varlog_root = "/host/var/log";
        } else if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            cfg.interval_seconds = (unsigned)strtoul(argv[++i], NULL, 10);
            if (cfg.interval_seconds == 0) cfg.interval_seconds = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    return sentinel_run(&cfg);
}
