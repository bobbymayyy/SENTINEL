#include "log.h"
#include <stdio.h>
#include <time.h>

static void vlog_line(const char *level, const char *fmt, va_list ap) {
    char ts[32];
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    fprintf(stderr, "%s [%s] ", ts, level);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog_line("INFO", fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog_line("WARN", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog_line("ERROR", fmt, ap);
    va_end(ap);
}
