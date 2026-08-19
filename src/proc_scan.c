#include "proc_scan.h"
#include "json_emit.h"
#include "log.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct proc_key {
    pid_t pid;
    uint64_t start_ticks;
};

static struct proc_key *previous;
static size_t previous_count;
static bool initialized;

static int is_pid_dir(const char *name) {
    for (const char *p = name; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return *name != '\0';
}

static void read_cmdline(const char *path, char *buf, size_t len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        buf[0] = '\0';
        return;
    }
    size_t n = fread(buf, 1, len - 1, f);
    fclose(f);
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\0') buf[i] = ' ';
    }
    while (n > 0 && buf[n - 1] == ' ') n--;
    buf[n] = '\0';
}

static int read_stat_info(const char *path, pid_t *ppid, uint64_t *start_ticks,
                          char *comm, size_t comm_len) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[4096];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    char *l = strchr(line, '(');
    char *r = strrchr(line, ')');
    if (!l || !r || r <= l) return -1;

    size_t n = (size_t)(r - l - 1);
    if (n >= comm_len) n = comm_len - 1;
    memcpy(comm, l + 1, n);
    comm[n] = '\0';

    char *cursor = r + 2;
    char *save = NULL;
    char *tok = strtok_r(cursor, " ", &save);
    unsigned field = 3;
    bool got_ppid = false;
    bool got_start = false;

    while (tok) {
        if (field == 4) {
            errno = 0;
            char *end = NULL;
            long value = strtol(tok, &end, 10);
            if (errno != 0 || end == tok || *end != '\0') return -1;
            *ppid = (pid_t)value;
            got_ppid = true;
        } else if (field == 22) {
            errno = 0;
            char *end = NULL;
            unsigned long long value = strtoull(tok, &end, 10);
            if (errno != 0 || end == tok || *end != '\0') return -1;
            *start_ticks = (uint64_t)value;
            got_start = true;
            break;
        }
        field++;
        tok = strtok_r(NULL, " ", &save);
    }

    return (got_ppid && got_start) ? 0 : -1;
}

static void uid_to_user(uid_t uid, char *buf, size_t len) {
    struct passwd pw;
    struct passwd *out = NULL;
    char scratch[16384];
    if (getpwuid_r(uid, &pw, scratch, sizeof(scratch), &out) == 0 && out) {
        snprintf(buf, len, "%s", out->pw_name);
    } else {
        snprintf(buf, len, "%lu", (unsigned long)uid);
    }
}

static int proc_key_cmp(const void *a, const void *b) {
    const struct proc_key *pa = a;
    const struct proc_key *pb = b;
    if (pa->pid < pb->pid) return -1;
    if (pa->pid > pb->pid) return 1;
    return 0;
}

static const struct proc_key *find_previous(pid_t pid) {
    size_t lo = 0;
    size_t hi = previous_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (previous[mid].pid == pid) return &previous[mid];
        if (previous[mid].pid < pid) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}

static int append_key(struct proc_key **items, size_t *count, size_t *capacity,
                      pid_t pid, uint64_t start_ticks) {
    if (*count == *capacity) {
        size_t new_capacity = *capacity ? *capacity * 2 : 256;
        struct proc_key *grown = realloc(*items, new_capacity * sizeof(**items));
        if (!grown) return -1;
        *items = grown;
        *capacity = new_capacity;
    }
    (*items)[*count].pid = pid;
    (*items)[*count].start_ticks = start_ticks;
    (*count)++;
    return 0;
}

int scan_processes(const struct sentinel_config *cfg) {
    DIR *d = opendir(cfg->proc_root);
    if (!d) {
        log_warn("cannot open proc root %s: %s", cfg->proc_root, strerror(errno));
        return -1;
    }

    struct proc_key *current = NULL;
    size_t current_count = 0;
    size_t current_capacity = 0;
    int rc = 0;

    struct dirent *de;
    while ((de = readdir(d))) {
        if (!is_pid_dir(de->d_name)) continue;

        errno = 0;
        char *end = NULL;
        long pid_l = strtol(de->d_name, &end, 10);
        if (errno != 0 || end == de->d_name || *end != '\0' || pid_l <= 0) continue;

        struct proc_info p = {0};
        p.pid = (pid_t)pid_l;

        char path[SENTINEL_MAX_PATH];
        int written = snprintf(path, sizeof(path), "%s/%ld", cfg->proc_root, pid_l);
        if (written < 0 || (size_t)written >= sizeof(path)) continue;

        struct stat st;
        if (stat(path, &st) != 0) continue;
        p.uid = st.st_uid;
        uid_to_user(p.uid, p.user, sizeof(p.user));

        written = snprintf(path, sizeof(path), "%s/%ld/stat", cfg->proc_root, pid_l);
        if (written < 0 || (size_t)written >= sizeof(path)) continue;
        if (read_stat_info(path, &p.ppid, &p.start_ticks, p.comm, sizeof(p.comm)) != 0) continue;

        if (append_key(&current, &current_count, &current_capacity, p.pid, p.start_ticks) != 0) {
            log_error("cannot allocate process state");
            rc = -1;
            break;
        }

        const struct proc_key *old = find_previous(p.pid);
        bool is_new = !old || old->start_ticks != p.start_ticks;
        if (is_new && (initialized || cfg->emit_baseline)) {
            written = snprintf(path, sizeof(path), "%s/%ld/cmdline", cfg->proc_root, pid_l);
            if (written >= 0 && (size_t)written < sizeof(path)) read_cmdline(path, p.cmdline, sizeof(p.cmdline));
            if (p.cmdline[0] == '\0') snprintf(p.cmdline, sizeof(p.cmdline), "[%s]", p.comm);
            emit_process_seen(&p);
        }
    }

    closedir(d);
    if (rc != 0) {
        free(current);
        return rc;
    }

    qsort(current, current_count, sizeof(*current), proc_key_cmp);
    free(previous);
    previous = current;
    previous_count = current_count;
    initialized = true;
    return 0;
}

void proc_scan_close(void) {
    free(previous);
    previous = NULL;
    previous_count = 0;
    initialized = false;
}
