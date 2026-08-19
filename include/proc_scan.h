#ifndef SENTINEL_PROC_SCAN_H
#define SENTINEL_PROC_SCAN_H

#include "sentinel.h"

int scan_processes(const struct sentinel_config *cfg);
void proc_scan_close(void);

#endif
