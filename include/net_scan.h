#ifndef SENTINEL_NET_SCAN_H
#define SENTINEL_NET_SCAN_H

#include "sentinel.h"

int scan_network_listeners(const struct sentinel_config *cfg);
void network_scan_close(void);

#endif
