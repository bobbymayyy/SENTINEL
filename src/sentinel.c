#include "sentinel.h"
#include "file_watch.h"
#include "log.h"
#include "net_scan.h"
#include "proc_scan.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

void sentinel_default_config(struct sentinel_config *cfg) {
    cfg->proc_root = "/proc";
    cfg->etc_root = "/etc";
    cfg->varlog_root = "/var/log";
    cfg->interval_seconds = 2;
    cfg->monitor_processes = true;
    cfg->monitor_network = true;
    cfg->monitor_files = true;
}

int sentinel_run(const struct sentinel_config *cfg) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    log_info("ir-sentinel %s starting", SENTINEL_VERSION);
    if (cfg->monitor_files) file_watch_init(cfg);

    while (!stop_requested) {
        if (cfg->monitor_processes) scan_processes(cfg);
        if (cfg->monitor_network) scan_network_listeners(cfg);
        if (cfg->monitor_files) file_watch_poll();
        sleep(cfg->interval_seconds);
    }

    file_watch_close();
    log_info("ir-sentinel stopping");
    return 0;
}
