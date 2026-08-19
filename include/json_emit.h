#ifndef SENTINEL_JSON_EMIT_H
#define SENTINEL_JSON_EMIT_H

#include "sentinel.h"
#include <stdint.h>

void emit_process_seen(const struct proc_info *p);
void emit_file_change(const char *path, const char *action);
void emit_network_listen(const char *proto, const char *local_addr, unsigned local_port, uint64_t inode);
void emit_network_close(const char *proto, const char *local_addr, unsigned local_port, uint64_t inode);

#endif
