#include "net_scan.h"
#include "json_emit.h"
#include "log.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static void hex_to_ipv4(const char *hex, char *out, size_t len) {
    unsigned ip;
    sscanf(hex, "%X", &ip);
    struct in_addr a = { .s_addr = ip };
    inet_ntop(AF_INET, &a, out, len);
}

static int scan_tcp_file(const char *path, const char *proto) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        char local_hex[64], rem_hex[64], state[8];
        unsigned local_port, rem_port, inode;
        int matched = sscanf(line, " %*d: %63[0-9A-Fa-f]:%X %63[0-9A-Fa-f]:%X %7s %*s %*s %*s %*s %*s %u",
                             local_hex, &local_port, rem_hex, &rem_port, state, &inode);
        (void)rem_hex; (void)rem_port;
        if (matched >= 6 && strcmp(state, "0A") == 0) {
            char addr[INET_ADDRSTRLEN];
            hex_to_ipv4(local_hex, addr, sizeof(addr));
            emit_network_listen(proto, addr, local_port, inode);
        }
    }
    fclose(f);
    return 0;
}

int scan_network_listeners(const struct sentinel_config *cfg) {
    char path[SENTINEL_MAX_PATH];
    snprintf(path, sizeof(path), "%s/net/tcp", cfg->proc_root);
    if (scan_tcp_file(path, "tcp4") != 0) log_warn("cannot scan %s: %s", path, strerror(errno));
    snprintf(path, sizeof(path), "%s/net/tcp6", cfg->proc_root);
    scan_tcp_file(path, "tcp6");
    return 0;
}
