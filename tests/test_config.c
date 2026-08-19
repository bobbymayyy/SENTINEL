#include "sentinel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    struct sentinel_config cfg;
    sentinel_default_config(&cfg);

    assert(cfg.interval_seconds == 2);
    assert(cfg.monitor_processes);
    assert(cfg.monitor_network);
    assert(cfg.monitor_files);
    assert(cfg.emit_baseline);
    assert(cfg.default_watch_paths);

    assert(sentinel_load_config_file(&cfg, "tests/fixtures/valid.yaml") == 0);
    assert(cfg.interval_seconds == 7);
    assert(!cfg.monitor_processes);
    assert(cfg.monitor_network);
    assert(cfg.monitor_files);
    assert(!cfg.emit_baseline);
    assert(!cfg.default_watch_paths);
    assert(cfg.watch_path_count == 2);
    assert(strcmp(cfg.watch_paths[0], "/etc/passwd") == 0);
    assert(strcmp(cfg.watch_paths[1], "/etc/ssh") == 0);

    sentinel_config_destroy(&cfg);
    puts("config tests passed");
    return 0;
}
