#include "json_emit.h"
#include <stdio.h>
#include <time.h>

static void utc_now(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static void json_string(FILE *out, const char *s) {
    fputc('"', out);
    for (; s && *s; s++) {
        switch (*s) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\b': fputs("\\b", out); break;
            case '\f': fputs("\\f", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if ((unsigned char)*s < 0x20) fprintf(out, "\\u%04x", (unsigned char)*s);
                else fputc(*s, out);
        }
    }
    fputc('"', out);
}

void emit_process_exec(const struct proc_info *p) {
    char ts[32];
    utc_now(ts, sizeof(ts));
    printf("{\"event\":\"process_seen\",\"pid\":%d,\"ppid\":%d,\"uid\":%u,\"user\":",
           p->pid, p->ppid, p->uid);
    json_string(stdout, p->user);
    printf(",\"comm\":");
    json_string(stdout, p->comm);
    printf(",\"cmd\":");
    json_string(stdout, p->cmdline);
    printf(",\"time\":");
    json_string(stdout, ts);
    puts("}");
    fflush(stdout);
}

void emit_file_change(const char *path, const char *action) {
    char ts[32];
    utc_now(ts, sizeof(ts));
    printf("{\"event\":\"file_change\",\"path\":");
    json_string(stdout, path);
    printf(",\"action\":");
    json_string(stdout, action);
    printf(",\"time\":");
    json_string(stdout, ts);
    puts("}");
    fflush(stdout);
}

void emit_network_listen(const char *proto, const char *local_addr, unsigned local_port, unsigned inode) {
    char ts[32];
    utc_now(ts, sizeof(ts));
    printf("{\"event\":\"network_listen\",\"proto\":");
    json_string(stdout, proto);
    printf(",\"local_addr\":");
    json_string(stdout, local_addr);
    printf(",\"local_port\":%u,\"inode\":%u,\"time\":", local_port, inode);
    json_string(stdout, ts);
    puts("}");
    fflush(stdout);
}
