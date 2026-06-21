#ifndef IR_SENTINEL_JSON_EMIT_H
#define IR_SENTINEL_JSON_EMIT_H

#include "sentinel.h"

void emit_process_exec(const struct proc_info *p);
void emit_file_change(const char *path, const char *action);
void emit_network_listen(const char *proto, const char *local_addr, unsigned local_port, unsigned inode);

#endif
