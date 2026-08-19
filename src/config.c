#include "sentinel.h"
#include "log.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SENTINEL_MAX_INTERVAL 86400U

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

static char *unquote(char *s) {
    size_t n = strlen(s);
    if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') || (s[0] == '\'' && s[n - 1] == '\''))) {
        s[n - 1] = '\0';
        return s + 1;
    }
    return s;
}

static int parse_bool(const char *s, bool *out) {
    if (strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 ||
        strcasecmp(s, "on") == 0 || strcmp(s, "1") == 0) {
        *out = true;
        return 0;
    }
    if (strcasecmp(s, "false") == 0 || strcasecmp(s, "no") == 0 ||
        strcasecmp(s, "off") == 0 || strcmp(s, "0") == 0) {
        *out = false;
        return 0;
    }
    return -1;
}

static int parse_interval(const char *s, unsigned *out) {
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *trim(end) != '\0' || value == 0 || value > SENTINEL_MAX_INTERVAL) {
        return -1;
    }
    *out = (unsigned)value;
    return 0;
}

void sentinel_default_config(struct sentinel_config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->proc_root = "/proc";
    cfg->etc_root = "/etc";
    cfg->varlog_root = "/var/log";
    cfg->interval_seconds = 2;
    cfg->monitor_processes = true;
    cfg->monitor_network = true;
    cfg->monitor_files = true;
    cfg->emit_baseline = true;
    cfg->default_watch_paths = true;
    cfg->run_once = false;
}

void sentinel_config_destroy(struct sentinel_config *cfg) {
    if (!cfg) return;
    for (size_t i = 0; i < cfg->watch_path_count; i++) {
        free(cfg->watch_paths[i]);
        cfg->watch_paths[i] = NULL;
    }
    cfg->watch_path_count = 0;
}

int sentinel_config_add_watch(struct sentinel_config *cfg, const char *path) {
    if (!cfg || !path || path[0] != '/') {
        log_error("watch paths must be absolute: %s", path ? path : "(null)");
        return -1;
    }
    if (strlen(path) >= SENTINEL_MAX_PATH) {
        log_error("watch path exceeds %d bytes: %s", SENTINEL_MAX_PATH - 1, path);
        return -1;
    }
    for (size_t i = 0; i < cfg->watch_path_count; i++) {
        if (strcmp(cfg->watch_paths[i], path) == 0) return 0;
    }
    if (cfg->watch_path_count >= SENTINEL_MAX_WATCH_PATHS) {
        log_error("too many custom watch paths (max %d)", SENTINEL_MAX_WATCH_PATHS);
        return -1;
    }
    char *copy = strdup(path);
    if (!copy) {
        log_error("cannot allocate watch path");
        return -1;
    }
    cfg->watch_paths[cfg->watch_path_count++] = copy;
    return 0;
}

int sentinel_load_config_file(struct sentinel_config *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        log_error("cannot open config %s: %s", path, strerror(errno));
        return -1;
    }

    char line[8192];
    unsigned line_no = 0;
    bool in_watch_paths = false;
    int rc = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        if (!strchr(line, '\n') && !feof(f)) {
            log_error("%s:%u line too long", path, line_no);
            rc = -1;
            break;
        }

        char *s = trim(line);
        if (*s == '\0' || *s == '#') continue;

        if (*s == '-') {
            if (!in_watch_paths) {
                log_error("%s:%u list item outside watch_paths", path, line_no);
                rc = -1;
                break;
            }
            char *value = trim(s + 1);
            value = unquote(value);
            if (*value == '\0' || sentinel_config_add_watch(cfg, value) != 0) {
                log_error("%s:%u invalid watch path", path, line_no);
                rc = -1;
                break;
            }
            continue;
        }

        char *colon = strchr(s, ':');
        if (!colon) {
            log_error("%s:%u expected key: value", path, line_no);
            rc = -1;
            break;
        }
        *colon = '\0';
        char *key = trim(s);
        char *value = trim(colon + 1);
        value = unquote(value);
        in_watch_paths = false;

        if (strcmp(key, "watch_paths") == 0) {
            if (*value != '\0' && strcmp(value, "[]") != 0) {
                log_error("%s:%u watch_paths must be a YAML sequence", path, line_no);
                rc = -1;
                break;
            }
            in_watch_paths = (*value == '\0');
        } else if (strcmp(key, "interval_seconds") == 0) {
            if (parse_interval(value, &cfg->interval_seconds) != 0) {
                log_error("%s:%u invalid interval_seconds (1-%u)", path, line_no, SENTINEL_MAX_INTERVAL);
                rc = -1;
                break;
            }
        } else if (strcmp(key, "monitor_processes") == 0) {
            if (parse_bool(value, &cfg->monitor_processes) != 0) goto invalid_bool;
        } else if (strcmp(key, "monitor_network") == 0) {
            if (parse_bool(value, &cfg->monitor_network) != 0) goto invalid_bool;
        } else if (strcmp(key, "monitor_files") == 0) {
            if (parse_bool(value, &cfg->monitor_files) != 0) goto invalid_bool;
        } else if (strcmp(key, "emit_baseline") == 0) {
            if (parse_bool(value, &cfg->emit_baseline) != 0) goto invalid_bool;
        } else if (strcmp(key, "default_watch_paths") == 0) {
            if (parse_bool(value, &cfg->default_watch_paths) != 0) goto invalid_bool;
        } else {
            log_error("%s:%u unknown config key: %s", path, line_no, key);
            rc = -1;
            break;
        }
        continue;

invalid_bool:
        log_error("%s:%u invalid boolean for %s", path, line_no, key);
        rc = -1;
        break;
    }

    if (ferror(f)) {
        log_error("error reading config %s: %s", path, strerror(errno));
        rc = -1;
    }
    fclose(f);
    return rc;
}
