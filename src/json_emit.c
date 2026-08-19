#include "json_emit.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

static void utc_now(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm_utc;
    if (gmtime_r(&now, &tm_utc) != NULL) {
        strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    } else {
        snprintf(buf, len, "unknown-time");
    }
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

static void emit_prefix(const char *event) {
    printf("{\"event\":");
    json_string(stdout, event);
    printf(",\"schema_version\":1,\"sensor_version\":");
    json_string(stdout, SENTINEL_VERSION);
}

static void emit_time_and_close(void) {
    char ts[32];
    utc_now(ts, sizeof(ts));
    printf(",\"time\":");
    json_string(stdout, ts);
    puts("}");
    fflush(stdout);
}

void emit_process_seen(const struct proc_info *p) {
    emit_prefix("process_seen");
    printf(",\"pid\":%ld,\"ppid\":%ld,\"uid\":%lu,\"start_ticks\":%" PRIu64 ",\"user\":",
           (long)p->pid, (long)p->ppid, (unsigned long)p->uid, p->start_ticks);
    json_string(stdout, p->user);
    printf(",\"comm\":");
    json_string(stdout, p->comm);
    printf(",\"cmd\":");
    json_string(stdout, p->cmdline);
    emit_time_and_close();
}

void emit_file_change(const char *path, const char *action) {
    emit_prefix("file_change");
    printf(",\"path\":");
    json_string(stdout, path);
    printf(",\"action\":");
    json_string(stdout, action);
    emit_time_and_close();
}

static void emit_network_event(const char *event, const char *proto, const char *local_addr,
                               unsigned local_port, uint64_t inode) {
    emit_prefix(event);
    printf(",\"proto\":");
    json_string(stdout, proto);
    printf(",\"local_addr\":");
    json_string(stdout, local_addr);
    printf(",\"local_port\":%u,\"inode\":%" PRIu64, local_port, inode);
    emit_time_and_close();
}

void emit_network_listen(const char *proto, const char *local_addr, unsigned local_port, uint64_t inode) {
    emit_network_event("network_listen", proto, local_addr, local_port, inode);
}

void emit_network_close(const char *proto, const char *local_addr, unsigned local_port, uint64_t inode) {
    emit_network_event("network_close", proto, local_addr, local_port, inode);
}
