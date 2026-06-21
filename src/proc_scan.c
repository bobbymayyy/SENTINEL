#include "proc_scan.h"
#include "json_emit.h"
#include "log.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PID_MAX_TRACK 4194304
static unsigned char *seen_pids;

static int is_pid_dir(const char *name) {
    for (const char *p = name; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    return *name != '\0';
}

static void read_cmdline(const char *path, char *buf, size_t len) {
    FILE *f = fopen(path, "rb");
    if (!f) { buf[0] = '\0'; return; }
    size_t n = fread(buf, 1, len - 1, f);
    fclose(f);
    for (size_t i = 0; i < n; i++) if (buf[i] == '\0') buf[i] = ' ';
    buf[n] = '\0';
}

static int read_stat_ppid_comm(const char *path, pid_t *ppid, char *comm, size_t comm_len) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[4096];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    char *l = strchr(line, '(');
    char *r = strrchr(line, ')');
    if (!l || !r || r <= l) return -1;
    size_t n = (size_t)(r - l - 1);
    if (n >= comm_len) n = comm_len - 1;
    memcpy(comm, l + 1, n);
    comm[n] = '\0';

    char state;
    long ppid_l;
    if (sscanf(r + 2, "%c %ld", &state, &ppid_l) != 2) return -1;
    (void)state;
    *ppid = (pid_t)ppid_l;
    return 0;
}

static void uid_to_user(uid_t uid, char *buf, size_t len) {
    struct passwd pw, *out = NULL;
    char scratch[16384];
    if (getpwuid_r(uid, &pw, scratch, sizeof(scratch), &out) == 0 && out) {
        snprintf(buf, len, "%s", out->pw_name);
    } else {
        snprintf(buf, len, "%u", uid);
    }
}

int scan_processes(const struct sentinel_config *cfg) {
    if (!seen_pids) {
        seen_pids = calloc(PID_MAX_TRACK, 1);
        if (!seen_pids) return -1;
    }

    DIR *d = opendir(cfg->proc_root);
    if (!d) {
        log_warn("cannot open proc root %s: %s", cfg->proc_root, strerror(errno));
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d))) {
        if (!is_pid_dir(de->d_name)) continue;
        long pid_l = strtol(de->d_name, NULL, 10);
        if (pid_l <= 0 || pid_l >= PID_MAX_TRACK) continue;
        if (seen_pids[pid_l]) continue;

        struct proc_info p = {0};
        p.pid = (pid_t)pid_l;

        char path[SENTINEL_MAX_PATH];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%ld", cfg->proc_root, pid_l);
        if (stat(path, &st) != 0) continue;
        p.uid = st.st_uid;
        uid_to_user(p.uid, p.user, sizeof(p.user));

        snprintf(path, sizeof(path), "%s/%ld/stat", cfg->proc_root, pid_l);
        if (read_stat_ppid_comm(path, &p.ppid, p.comm, sizeof(p.comm)) != 0) continue;

        snprintf(path, sizeof(path), "%s/%ld/cmdline", cfg->proc_root, pid_l);
        read_cmdline(path, p.cmdline, sizeof(p.cmdline));
        if (p.cmdline[0] == '\0') snprintf(p.cmdline, sizeof(p.cmdline), "[%s]", p.comm);

        seen_pids[pid_l] = 1;
        emit_process_exec(&p);
    }

    closedir(d);
    return 0;
}
