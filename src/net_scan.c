#include "net_scan.h"
#include "json_emit.h"
#include "log.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct listener {
    char proto[5];
    char addr[INET6_ADDRSTRLEN];
    unsigned port;
    uint64_t inode;
};

struct listener_vec {
    struct listener *items;
    size_t count;
    size_t capacity;
};

static struct listener *previous;
static size_t previous_count;
static bool initialized;

static int listener_cmp(const void *a, const void *b) {
    const struct listener *la = a;
    const struct listener *lb = b;
    int c = strcmp(la->proto, lb->proto);
    if (c != 0) return c;
    c = strcmp(la->addr, lb->addr);
    if (c != 0) return c;
    if (la->port < lb->port) return -1;
    if (la->port > lb->port) return 1;
    if (la->inode < lb->inode) return -1;
    if (la->inode > lb->inode) return 1;
    return 0;
}

static int append_listener(struct listener_vec *vec, const struct listener *item) {
    if (vec->count == vec->capacity) {
        size_t new_capacity = vec->capacity ? vec->capacity * 2 : 64;
        struct listener *grown = realloc(vec->items, new_capacity * sizeof(*grown));
        if (!grown) return -1;
        vec->items = grown;
        vec->capacity = new_capacity;
    }
    vec->items[vec->count++] = *item;
    return 0;
}

static int hex_to_ipv4(const char *hex, char *out, size_t len) {
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(hex, &end, 16);
    if (errno != 0 || end == hex || *end != '\0' || value > UINT32_MAX) return -1;
    uint32_t word = (uint32_t)value;
    struct in_addr addr;
    memcpy(&addr, &word, sizeof(addr));
    return inet_ntop(AF_INET, &addr, out, len) ? 0 : -1;
}

static int hex_to_ipv6(const char *hex, char *out, size_t len) {
    if (strlen(hex) != 32) return -1;
    struct in6_addr addr = {0};

    for (size_t i = 0; i < 4; i++) {
        char chunk[9];
        memcpy(chunk, hex + i * 8, 8);
        chunk[8] = '\0';
        errno = 0;
        char *end = NULL;
        unsigned long value = strtoul(chunk, &end, 16);
        if (errno != 0 || end == chunk || *end != '\0' || value > UINT32_MAX) return -1;
        uint32_t word = (uint32_t)value;
        memcpy(addr.s6_addr + i * 4, &word, sizeof(word));
    }

    return inet_ntop(AF_INET6, &addr, out, len) ? 0 : -1;
}

static int scan_tcp_file(const char *path, const char *proto, bool ipv6, struct listener_vec *current) {
    FILE *f = fopen(path, "r");
    if (!f) return -errno;

    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        int err = ferror(f) ? errno : EIO;
        fclose(f);
        return -err;
    }

    while (fgets(line, sizeof(line), f)) {
        char local_hex[65];
        char remote_hex[65];
        char state[3];
        unsigned local_port;
        unsigned remote_port;
        unsigned long long inode;

        int matched = sscanf(line,
            " %*d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %2s %*s %*s %*s %*u %*u %llu",
            local_hex, &local_port, remote_hex, &remote_port, state, &inode);
        (void)remote_hex;
        (void)remote_port;
        if (matched != 6 || strcmp(state, "0A") != 0) continue;

        struct listener item = {0};
        snprintf(item.proto, sizeof(item.proto), "%s", proto);
        item.port = local_port;
        item.inode = (uint64_t)inode;
        int addr_rc = ipv6 ? hex_to_ipv6(local_hex, item.addr, sizeof(item.addr))
                           : hex_to_ipv4(local_hex, item.addr, sizeof(item.addr));
        if (addr_rc != 0) continue;
        if (append_listener(current, &item) != 0) {
            fclose(f);
            return -ENOMEM;
        }
    }

    if (ferror(f)) {
        int err = errno ? errno : EIO;
        fclose(f);
        return -err;
    }
    fclose(f);
    return 0;
}

static int retain_previous_proto(struct listener_vec *current, const char *proto) {
    for (size_t i = 0; i < previous_count; i++) {
        if (strcmp(previous[i].proto, proto) == 0 && append_listener(current, &previous[i]) != 0) return -1;
    }
    return 0;
}

static bool contains_listener(const struct listener *items, size_t count, const struct listener *needle) {
    return bsearch(needle, items, count, sizeof(*items), listener_cmp) != NULL;
}

static size_t dedupe_sorted(struct listener *items, size_t count) {
    if (count < 2) return count;
    size_t out = 1;
    for (size_t i = 1; i < count; i++) {
        if (listener_cmp(&items[i], &items[out - 1]) != 0) items[out++] = items[i];
    }
    return out;
}

int scan_network_listeners(const struct sentinel_config *cfg) {
    struct listener_vec current = {0};
    char path[SENTINEL_MAX_PATH];

    int written = snprintf(path, sizeof(path), "%s/net/tcp", cfg->proc_root);
    if (written < 0 || (size_t)written >= sizeof(path)) return -1;
    int rc4 = scan_tcp_file(path, "tcp4", false, &current);
    if (rc4 != 0) {
        log_warn("cannot scan %s: %s", path, strerror(-rc4));
        if (retain_previous_proto(&current, "tcp4") != 0) goto oom;
    }

    written = snprintf(path, sizeof(path), "%s/net/tcp6", cfg->proc_root);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        free(current.items);
        return -1;
    }
    int rc6 = scan_tcp_file(path, "tcp6", true, &current);
    if (rc6 != 0) {
        if (rc6 != -ENOENT) log_warn("cannot scan %s: %s", path, strerror(-rc6));
        if (retain_previous_proto(&current, "tcp6") != 0) goto oom;
    }

    qsort(current.items, current.count, sizeof(*current.items), listener_cmp);
    current.count = dedupe_sorted(current.items, current.count);

    if (initialized || cfg->emit_baseline) {
        for (size_t i = 0; i < current.count; i++) {
            if (!contains_listener(previous, previous_count, &current.items[i])) {
                emit_network_listen(current.items[i].proto, current.items[i].addr,
                                    current.items[i].port, current.items[i].inode);
            }
        }
    }

    if (initialized) {
        for (size_t i = 0; i < previous_count; i++) {
            if (!contains_listener(current.items, current.count, &previous[i])) {
                emit_network_close(previous[i].proto, previous[i].addr,
                                   previous[i].port, previous[i].inode);
            }
        }
    }

    free(previous);
    previous = current.items;
    previous_count = current.count;
    initialized = true;
    return 0;

oom:
    log_error("cannot allocate network listener state");
    free(current.items);
    return -1;
}

void network_scan_close(void) {
    free(previous);
    previous = NULL;
    previous_count = 0;
    initialized = false;
}
