#ifndef IR_SENTINEL_FILE_WATCH_H
#define IR_SENTINEL_FILE_WATCH_H

#include "sentinel.h"

int file_watch_init(const struct sentinel_config *cfg);
int file_watch_poll(void);
void file_watch_close(void);

#endif
