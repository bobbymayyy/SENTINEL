#include "sentinel.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENTINEL_MAX_INTERVAL 86400U

static void usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s [options]\n"
        "\n"
        "  --config FILE          load SENTINEL's supported YAML subset\n"
        "  --check-config         validate configuration and exit\n"
        "  --host-roots           use /host/proc, /host/etc, /host/var/log\n"
        "  --interval SEC         polling interval (1-86400, default 2)\n"
        "  --watch PATH           add an absolute inotify watch path\n"
        "  --no-default-watches   disable built-in /etc watch paths\n"
        "  --[no-]processes       enable/disable process telemetry\n"
        "  --[no-]network         enable/disable listener telemetry\n"
        "  --[no-]files           enable/disable file telemetry\n"
        "  --[no-]baseline        emit/suppress initial process/listener snapshot\n"
        "  --once                 run one polling cycle and exit\n"
        "  --version              print version and exit\n"
        "  -h, --help             show this help\n",
        argv0);
}

static int parse_interval(const char *s, unsigned *out) {
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || value == 0 || value > SENTINEL_MAX_INTERVAL) return -1;
    *out = (unsigned)value;
    return 0;
}

static void print_config_summary(const struct sentinel_config *cfg) {
    printf("SENTINEL %s configuration OK\n", SENTINEL_VERSION);
    printf("interval_seconds=%u processes=%s network=%s files=%s baseline=%s default_watches=%s custom_watches=%zu\n",
           cfg->interval_seconds,
           cfg->monitor_processes ? "on" : "off",
           cfg->monitor_network ? "on" : "off",
           cfg->monitor_files ? "on" : "off",
           cfg->emit_baseline ? "on" : "off",
           cfg->default_watch_paths ? "on" : "off",
           cfg->watch_path_count);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("SENTINEL %s\n", SENTINEL_VERSION);
            return 0;
        }
    }

    struct sentinel_config cfg;
    sentinel_default_config(&cfg);

    const char *config_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc || config_path) {
                fprintf(stderr, "--config requires exactly one FILE\n");
                sentinel_config_destroy(&cfg);
                return 2;
            }
            config_path = argv[++i];
        }
    }

    if (config_path && sentinel_load_config_file(&cfg, config_path) != 0) {
        sentinel_config_destroy(&cfg);
        return 2;
    }

    bool check_config = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0) {
            i++;
        } else if (strcmp(argv[i], "--check-config") == 0) {
            check_config = true;
        } else if (strcmp(argv[i], "--host-roots") == 0) {
            cfg.proc_root = "/host/proc";
            cfg.etc_root = "/host/etc";
            cfg.varlog_root = "/host/var/log";
        } else if (strcmp(argv[i], "--interval") == 0) {
            if (i + 1 >= argc || parse_interval(argv[++i], &cfg.interval_seconds) != 0) {
                fprintf(stderr, "--interval must be an integer from 1 to %u\n", SENTINEL_MAX_INTERVAL);
                sentinel_config_destroy(&cfg);
                return 2;
            }
        } else if (strcmp(argv[i], "--watch") == 0) {
            if (i + 1 >= argc || sentinel_config_add_watch(&cfg, argv[++i]) != 0) {
                sentinel_config_destroy(&cfg);
                return 2;
            }
        } else if (strcmp(argv[i], "--no-default-watches") == 0) {
            cfg.default_watch_paths = false;
        } else if (strcmp(argv[i], "--processes") == 0) {
            cfg.monitor_processes = true;
        } else if (strcmp(argv[i], "--no-processes") == 0) {
            cfg.monitor_processes = false;
        } else if (strcmp(argv[i], "--network") == 0) {
            cfg.monitor_network = true;
        } else if (strcmp(argv[i], "--no-network") == 0) {
            cfg.monitor_network = false;
        } else if (strcmp(argv[i], "--files") == 0) {
            cfg.monitor_files = true;
        } else if (strcmp(argv[i], "--no-files") == 0) {
            cfg.monitor_files = false;
        } else if (strcmp(argv[i], "--baseline") == 0) {
            cfg.emit_baseline = true;
        } else if (strcmp(argv[i], "--no-baseline") == 0) {
            cfg.emit_baseline = false;
        } else if (strcmp(argv[i], "--once") == 0) {
            cfg.run_once = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--version") == 0) {
            /* handled before config parsing */
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            sentinel_config_destroy(&cfg);
            return 2;
        }
    }

    if (check_config) {
        print_config_summary(&cfg);
        sentinel_config_destroy(&cfg);
        return 0;
    }

    int rc = sentinel_run(&cfg);
    sentinel_config_destroy(&cfg);
    return rc;
}
