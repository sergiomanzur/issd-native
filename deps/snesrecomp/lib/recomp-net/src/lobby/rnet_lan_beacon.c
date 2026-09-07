#include "recomp_net/lan_beacon.h"

#include "platform/rnet_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RNET_BC_MAGIC "RNETBC1"
#define RNET_BC_MAX_PKT 512
#define RNET_BC_CACHE_MAX 32
#define RNET_BC_INTERVAL_MS 1000ull
#define RNET_BC_STALE_MS 5000ull

typedef struct RNetLanBeaconEntry {
    char lobby_id[RNET_LAN_BEACON_ID_MAX];
    char endpoint[RNET_LAN_BEACON_ENDPOINT_MAX];
    char game_name[RNET_LAN_BEACON_GAME_MAX];
    rnet_u64 last_seen_ms;
} RNetLanBeaconEntry;

struct RNetLanBeacon {
    rnet_socket sock;
    unsigned short discovery_port;
    int is_publisher;
    char lobby_id[RNET_LAN_BEACON_ID_MAX];
    char endpoint[RNET_LAN_BEACON_ENDPOINT_MAX];
    char game_name[RNET_LAN_BEACON_GAME_MAX];
    rnet_u64 next_send_ms;
    RNetLanBeaconEntry cache[RNET_BC_CACHE_MAX];
    int cache_count;
};

static void copy_trunc(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0)
        return;
    dst[0] = '\0';
    if (!src)
        return;
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static void trim_crlf(char *s)
{
    size_t n;
    if (!s)
        return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static const char *next_line(char **cursor)
{
    char *start;
    char *nl;
    if (!cursor || !*cursor || !**cursor)
        return NULL;
    start = *cursor;
    nl = strchr(start, '\n');
    if (nl) {
        *nl = '\0';
        *cursor = nl + 1;
    } else {
        *cursor = start + strlen(start);
    }
    trim_crlf(start);
    return start;
}

static int endpoint_looks_ok(const char *ep)
{
    const char *colon;
    unsigned a, b, c, d;
    char extra;
    int port;
    if (!ep || !ep[0])
        return 0;
    colon = strrchr(ep, ':');
    if (!colon || colon == ep || !colon[1])
        return 0;
    if (sscanf(ep, "%u.%u.%u.%u:%d%c", &a, &b, &c, &d, &port, &extra) != 5)
        return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255 || port <= 0 || port > 65535)
        return 0;
    /* RFC1918 only — never broadcast loopback / WAN via discovery. */
    if (a == 10)
        return 1;
    if (a == 172 && b >= 16 && b <= 31)
        return 1;
    if (a == 192 && b == 168)
        return 1;
    return 0;
}

static int build_announce(char *buf, size_t cap, const char *lobby_id,
                          const char *endpoint, const char *game_name)
{
    int n;
    if (!buf || cap < 32 || !lobby_id || !lobby_id[0] || !endpoint || !endpoint[0])
        return -1;
    n = snprintf(buf, cap, "%s\nANNOUNCE\n%s\n%s\n%s\n", RNET_BC_MAGIC, lobby_id,
                 endpoint, game_name ? game_name : "");
    if (n < 0 || (size_t)n >= cap)
        return -1;
    return 0;
}

static int parse_announce(char *pkt, char *lobby_id, size_t id_cap, char *endpoint,
                          size_t ep_cap, char *game_name, size_t game_cap)
{
    char *cursor = pkt;
    const char *magic = next_line(&cursor);
    const char *op = next_line(&cursor);
    const char *id = next_line(&cursor);
    const char *ep = next_line(&cursor);
    const char *game = next_line(&cursor);
    if (!magic || strcmp(magic, RNET_BC_MAGIC) != 0)
        return 0;
    if (!op || strcmp(op, "ANNOUNCE") != 0)
        return 0;
    if (!id || !id[0] || !ep || !endpoint_looks_ok(ep))
        return 0;
    copy_trunc(lobby_id, id_cap, id);
    copy_trunc(endpoint, ep_cap, ep);
    copy_trunc(game_name, game_cap, game ? game : "");
    return 1;
}

static void cache_upsert(RNetLanBeacon *b, const char *lobby_id, const char *endpoint,
                         const char *game_name, rnet_u64 now)
{
    int i;
    int free_i = -1;
    int oldest_i = 0;
    rnet_u64 oldest = 0;
    if (!b || !lobby_id || !endpoint)
        return;
    for (i = 0; i < RNET_BC_CACHE_MAX; ++i) {
        if (!b->cache[i].lobby_id[0]) {
            if (free_i < 0)
                free_i = i;
            continue;
        }
        if (strcmp(b->cache[i].lobby_id, lobby_id) == 0) {
            copy_trunc(b->cache[i].endpoint, sizeof(b->cache[i].endpoint), endpoint);
            copy_trunc(b->cache[i].game_name, sizeof(b->cache[i].game_name),
                       game_name);
            b->cache[i].last_seen_ms = now;
            return;
        }
        if (free_i < 0 &&
            (oldest == 0 || b->cache[i].last_seen_ms < oldest)) {
            oldest = b->cache[i].last_seen_ms;
            oldest_i = i;
        }
    }
    i = free_i >= 0 ? free_i : oldest_i;
    copy_trunc(b->cache[i].lobby_id, sizeof(b->cache[i].lobby_id), lobby_id);
    copy_trunc(b->cache[i].endpoint, sizeof(b->cache[i].endpoint), endpoint);
    copy_trunc(b->cache[i].game_name, sizeof(b->cache[i].game_name), game_name);
    b->cache[i].last_seen_ms = now;
    if (b->cache_count < RNET_BC_CACHE_MAX)
        ++b->cache_count;
}

static RNetLanBeacon *beacon_alloc(unsigned short discovery_port, int is_publisher)
{
    RNetLanBeacon *b = (RNetLanBeacon *)calloc(1, sizeof(*b));
    if (!b)
        return NULL;
    b->sock = RNET_SOCKET_INVALID;
    b->discovery_port =
        discovery_port ? discovery_port : (unsigned short)RNET_LAN_BEACON_DEFAULT_PORT;
    b->is_publisher = is_publisher ? 1 : 0;
    return b;
}

int rnet_lan_beacon_publish_open(RNetLanBeacon **out, unsigned short discovery_port)
{
    RNetLanBeacon *b;
    struct sockaddr_in addr;

    if (!out)
        return -1;
    *out = NULL;
    rnet_os_startup();
    b = beacon_alloc(discovery_port, 1);
    if (!b)
        return -1;
    b->sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(b->sock)) {
        free(b);
        return -1;
    }
    (void)rnet_os_setsockopt_broadcast(b->sock, 1);
    (void)rnet_os_set_nonblocking(b->sock);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;
    if (rnet_os_bind(b->sock, &addr) != 0) {
        rnet_lan_beacon_close(&b);
        return -1;
    }
    *out = b;
    return 0;
}

int rnet_lan_beacon_publish_set(RNetLanBeacon *beacon, const char *lobby_id,
                                const char *game_endpoint, const char *game_name)
{
    if (!beacon || !beacon->is_publisher)
        return -1;
    if (!lobby_id || !lobby_id[0] || !endpoint_looks_ok(game_endpoint)) {
        beacon->lobby_id[0] = '\0';
        beacon->endpoint[0] = '\0';
        return -1;
    }
    copy_trunc(beacon->lobby_id, sizeof(beacon->lobby_id), lobby_id);
    copy_trunc(beacon->endpoint, sizeof(beacon->endpoint), game_endpoint);
    copy_trunc(beacon->game_name, sizeof(beacon->game_name),
               game_name ? game_name : "");
    beacon->next_send_ms = 0;
    return 0;
}

int rnet_lan_beacon_publish_tick(RNetLanBeacon *beacon)
{
    char buf[RNET_BC_MAX_PKT];
    struct sockaddr_in dst;
    rnet_u64 now;
    if (!beacon || !beacon->is_publisher || !rnet_os_socket_valid(beacon->sock))
        return -1;
    if (!beacon->lobby_id[0] || !beacon->endpoint[0])
        return 0;
    now = rnet_os_monotonic_ms();
    if (beacon->next_send_ms && now < beacon->next_send_ms)
        return 0;
    if (build_announce(buf, sizeof(buf), beacon->lobby_id, beacon->endpoint,
                       beacon->game_name) != 0)
        return -1;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    dst.sin_port = htons(beacon->discovery_port);
    /* rnet_os_sendto returns bytes sent (>=0) or -1. */
    if (rnet_os_sendto(beacon->sock, buf, strlen(buf), &dst) < 0)
        return -1;
    beacon->next_send_ms = now + RNET_BC_INTERVAL_MS;
    return 0;
}

int rnet_lan_beacon_listen_open(RNetLanBeacon **out, unsigned short discovery_port)
{
    RNetLanBeacon *b;
    struct sockaddr_in addr;

    if (!out)
        return -1;
    *out = NULL;
    rnet_os_startup();
    b = beacon_alloc(discovery_port, 0);
    if (!b)
        return -1;
    b->sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(b->sock)) {
        free(b);
        return -1;
    }
    (void)rnet_os_setsockopt_reuseaddr(b->sock, 1);
    (void)rnet_os_setsockopt_broadcast(b->sock, 1);
    (void)rnet_os_set_nonblocking(b->sock);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(b->discovery_port);
    if (rnet_os_bind(b->sock, &addr) != 0) {
        rnet_lan_beacon_close(&b);
        return -1;
    }
    *out = b;
    return 0;
}

int rnet_lan_beacon_listen_pump(RNetLanBeacon *beacon)
{
    int updated = 0;
    if (!beacon || beacon->is_publisher || !rnet_os_socket_valid(beacon->sock))
        return 0;
    for (;;) {
        char buf[RNET_BC_MAX_PKT];
        struct sockaddr_in src;
        int would_block = 0;
        int n;
        char lobby_id[RNET_LAN_BEACON_ID_MAX];
        char endpoint[RNET_LAN_BEACON_ENDPOINT_MAX];
        char game_name[RNET_LAN_BEACON_GAME_MAX];
        memset(&src, 0, sizeof(src));
        n = rnet_os_recvfrom(beacon->sock, buf, sizeof(buf) - 1, &src, &would_block);
        if (n < 0) {
            if (would_block)
                break;
            break;
        }
        if (n == 0)
            break;
        buf[n] = '\0';
        if (!parse_announce(buf, lobby_id, sizeof(lobby_id), endpoint, sizeof(endpoint),
                            game_name, sizeof(game_name)))
            continue;
        cache_upsert(beacon, lobby_id, endpoint, game_name, rnet_os_monotonic_ms());
        ++updated;
    }
    return updated;
}

int rnet_lan_beacon_lookup(const RNetLanBeacon *beacon, const char *lobby_id,
                           char *endpoint_out, size_t endpoint_cap)
{
    rnet_u64 now;
    int i;
    if (!beacon || !lobby_id || !lobby_id[0] || !endpoint_out || endpoint_cap == 0)
        return 0;
    endpoint_out[0] = '\0';
    now = rnet_os_monotonic_ms();
    for (i = 0; i < RNET_BC_CACHE_MAX; ++i) {
        if (!beacon->cache[i].lobby_id[0])
            continue;
        if (strcmp(beacon->cache[i].lobby_id, lobby_id) != 0)
            continue;
        if (now - beacon->cache[i].last_seen_ms > RNET_BC_STALE_MS)
            return 0;
        copy_trunc(endpoint_out, endpoint_cap, beacon->cache[i].endpoint);
        return endpoint_out[0] ? 1 : 0;
    }
    return 0;
}

void rnet_lan_beacon_close(RNetLanBeacon **beacon)
{
    if (!beacon || !*beacon)
        return;
    rnet_os_socket_destroy(&(*beacon)->sock);
    free(*beacon);
    *beacon = NULL;
}
