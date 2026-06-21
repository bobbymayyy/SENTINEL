#ifndef IR_SENTINEL_LOG_H
#define IR_SENTINEL_LOG_H

#include <stdarg.h>

void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
