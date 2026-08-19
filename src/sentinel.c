#include "sentinel.h"
#include "file_watch.h"
#include "log.h"
#include "net_scan.h"
#include "proc_scan.h"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) != 0) log_warn("cannot install SIGINT handler: %s", strerror(errno));
    if (sigaction(SIGTERM, &sa, NULL) != 0) log_warn("cannot install SIGTERM handler: %s", strerror(errno));
}

static void sleep_interval(unsigned seconds) {
    struct timespec remaining = { .tv_sec = (time_t)seconds, .tv_nsec = 0 };
    while (!stop_requested && nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) break;
    }
}

int sentinel_run(const struct sentinel_config *cfg) {
    stop_requested = 0;
    install_signal_handlers();

    log_info("SENTINEL %s starting", SENTINEL_VERSION);
    if (!cfg->monitor_processes && !cfg->monitor_network && !cfg->monitor_files) {
        log_warn("all telemetry classes are disabled");
    }

    if (cfg->monitor_files) file_watch_init(cfg);

    while (!stop_requested) {
        if (cfg->monitor_processes) scan_processes(cfg);
        if (cfg->monitor_network) scan_network_listeners(cfg);
        if (cfg->monitor_files) file_watch_poll();

        if (cfg->run_once) break;
        sleep_interval(cfg->interval_seconds);
    }

    file_watch_close();
    proc_scan_close();
    network_scan_close();
    log_info("SENTINEL stopping");
    return 0;
}
