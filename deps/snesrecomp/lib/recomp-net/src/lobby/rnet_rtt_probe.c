#include "recomp_net/rtt_probe.h"

#include "platform/rnet_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RNET_RTT_MAGIC "RNETDJ1"
#define RNET_RTT_MAX_PKT 256

struct RNetRttProbe {
    rnet_socket sock;
    struct sockaddr_in peer;
    int peer_known;
};

static void trim_crlf(char *s)
{
    size_t n;
    if (!s)
        return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static int append_line(char *buf, size_t cap, size_t *o, const char *line)
{
    size_t n;
    if (!buf || !o || !line)
        return 0;
    n = strlen(line);
    if (*o + n + 2 >= cap)
        return 0;
    memcpy(buf + *o, line, n);
    *o += n;
    buf[(*o)++] = '\n';
    buf[*o] = '\0';
    return 1;
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

static int send_text(rnet_socket sock, const struct sockaddr_in *dst, const char *text)
{
    size_t n;
    if (!rnet_os_socket_valid(sock) || !dst || !text)
        return -1;
    n = strlen(text);
    if (n == 0 || n > RNET_RTT_MAX_PKT)
        return -1;
    /* rnet_os_sendto returns bytes sent (>=0) or -1. */
    return rnet_os_sendto(sock, text, n, dst) < 0 ? -1 : 0;
}

static int build_ping(char *buf, size_t cap, rnet_u64 t_ms)
{
    char ts[32];
    size_t o = 0;
    snprintf(ts, sizeof(ts), "%llu", (unsigned long long)t_ms);
    if (!append_line(buf, cap, &o, RNET_RTT_MAGIC) ||
        !append_line(buf, cap, &o, "PING") || !append_line(buf, cap, &o, ts))
        return -1;
    return 0;
}

static int build_pong(char *buf, size_t cap, const char *ts)
{
    size_t o = 0;
    if (!append_line(buf, cap, &o, RNET_RTT_MAGIC) ||
        !append_line(buf, cap, &o, "PONG") ||
        !append_line(buf, cap, &o, ts ? ts : "0"))
        return -1;
    return 0;
}

static int rtt_from_pong_ts(const char *ts, int *out_rtt_ms)
{
    unsigned long long sent = 0;
    rnet_u64 now;
    if (!ts || !out_rtt_ms || sscanf(ts, "%llu", &sent) != 1)
        return 0;
    now = rnet_os_monotonic_ms();
    if ((rnet_u64)sent > now)
        return 0;
    *out_rtt_ms = (int)(now - (rnet_u64)sent);
    if (*out_rtt_ms < 0)
        *out_rtt_ms = 0;
    if (*out_rtt_ms > 60000)
        *out_rtt_ms = 60000;
    return 1;
}

int rnet_rtt_probe_open(RNetRttProbe **out, const char *bind_hostport)
{
    RNetRttProbe *p;
    char host[128];
    rnet_u16 port = 0;
    struct sockaddr_in addr;

    if (!out)
        return -1;
    *out = NULL;
    rnet_os_startup();
    p = (RNetRttProbe *)calloc(1, sizeof(*p));
    if (!p)
        return -1;
    p->sock = RNET_SOCKET_INVALID;

    p->sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(p->sock)) {
        free(p);
        return -1;
    }
    (void)rnet_os_setsockopt_reuseaddr(p->sock, 1);
    (void)rnet_os_set_nonblocking(p->sock);

    if (bind_hostport && bind_hostport[0]) {
        if (rnet_os_parse_hostport(bind_hostport, host, sizeof(host), &port) != 0 ||
            port == 0) {
            rnet_rtt_probe_close(&p);
            return -1;
        }
        if (rnet_os_resolve_sockaddr(host[0] ? host : "0.0.0.0", port, &addr) != 0) {
            rnet_rtt_probe_close(&p);
            return -1;
        }
        if (rnet_os_bind(p->sock, &addr) != 0) {
            rnet_rtt_probe_close(&p);
            return -1;
        }
    } else {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = 0;
        if (rnet_os_bind(p->sock, &addr) != 0) {
            rnet_rtt_probe_close(&p);
            return -1;
        }
    }

    *out = p;
    return 0;
}

void rnet_rtt_probe_close(RNetRttProbe **probe)
{
    if (!probe || !*probe)
        return;
    rnet_os_socket_destroy(&(*probe)->sock);
    free(*probe);
    *probe = NULL;
}

int rnet_rtt_probe_set_peer(RNetRttProbe *probe, const char *peer_hostport)
{
    char host[128];
    rnet_u16 port = 0;
    if (!probe)
        return -1;
    probe->peer_known = 0;
    memset(&probe->peer, 0, sizeof(probe->peer));
    if (!peer_hostport || !peer_hostport[0])
        return 0;
    if (rnet_os_parse_hostport(peer_hostport, host, sizeof(host), &port) != 0 ||
        port == 0 || !host[0])
        return -1;
    if (rnet_os_resolve_sockaddr(host, port, &probe->peer) != 0)
        return -1;
    probe->peer_known = 1;
    return 0;
}

int rnet_rtt_probe_peer_known(const RNetRttProbe *probe)
{
    return (probe && probe->peer_known) ? 1 : 0;
}

int rnet_rtt_probe_ping_ts(RNetRttProbe *probe, unsigned long long *out_sent_ms)
{
    char buf[RNET_RTT_MAX_PKT];
    rnet_u64 sent;
    if (!probe || !probe->peer_known || !rnet_os_socket_valid(probe->sock))
        return -1;
    sent = rnet_os_monotonic_ms();
    if (build_ping(buf, sizeof(buf), sent) != 0)
        return -1;
    if (send_text(probe->sock, &probe->peer, buf) != 0)
        return -1;
    if (out_sent_ms)
        *out_sent_ms = (unsigned long long)sent;
    return 0;
}

int rnet_rtt_probe_ping(RNetRttProbe *probe)
{
    return rnet_rtt_probe_ping_ts(probe, NULL);
}

int rnet_rtt_probe_pump_ex(RNetRttProbe *probe, int *out_rtt_ms,
                           unsigned long long *out_echo_ms)
{
    char buf[RNET_RTT_MAX_PKT + 1];
    struct sockaddr_in src;
    int would_block = 0;
    int n;

    if (!probe || !rnet_os_socket_valid(probe->sock))
        return -1;

    for (;;) {
        char *cursor;
        const char *magic;
        const char *op;

        n = rnet_os_recvfrom(probe->sock, buf, sizeof(buf) - 1, &src, &would_block);
        if (n < 0) {
            if (would_block)
                break;
            return -1;
        }
        if (n == 0)
            continue;
        buf[n] = '\0';
        cursor = buf;
        magic = next_line(&cursor);
        op = next_line(&cursor);
        if (!magic || strcmp(magic, RNET_RTT_MAGIC) != 0 || !op)
            continue;

        if (strcmp(op, "PING") == 0) {
            const char *ts = next_line(&cursor);
            char reply[RNET_RTT_MAX_PKT];
            if (build_pong(reply, sizeof(reply), ts) == 0)
                (void)send_text(probe->sock, &src, reply);
            /* Learn peer from first ping if unset (host answering guest). */
            if (!probe->peer_known) {
                probe->peer = src;
                probe->peer_known = 1;
            }
            continue;
        }
        if (strcmp(op, "PONG") == 0) {
            const char *ts = next_line(&cursor);
            int ms = 0;
            unsigned long long echo = 0;
            /* One PONG per call so burst list probes can match echo stamps. */
            if (ts && sscanf(ts, "%llu", &echo) == 1) {
                if (out_echo_ms)
                    *out_echo_ms = echo;
                if (out_rtt_ms && rtt_from_pong_ts(ts, &ms)) {
                    *out_rtt_ms = ms;
                    return 1;
                }
            }
            continue;
        }
    }
    return 0;
}

int rnet_rtt_probe_pump(RNetRttProbe *probe, int *out_rtt_ms)
{
    return rnet_rtt_probe_pump_ex(probe, out_rtt_ms, NULL);
}

int rnet_rtt_probe_once(const char *peer_hostport, int timeout_ms, int *out_rtt_ms)
{
    RNetRttProbe *p = NULL;
    rnet_u64 deadline;
    int ms = 0;

    if (!peer_hostport || !peer_hostport[0] || !out_rtt_ms)
        return 0;
    if (timeout_ms < 1)
        timeout_ms = 1;
    if (rnet_rtt_probe_open(&p, NULL) != 0)
        return 0;
    if (rnet_rtt_probe_set_peer(p, peer_hostport) != 0) {
        rnet_rtt_probe_close(&p);
        return 0;
    }
    if (rnet_rtt_probe_ping(p) != 0) {
        rnet_rtt_probe_close(&p);
        return 0;
    }
    deadline = rnet_os_monotonic_ms() + (rnet_u64)timeout_ms;
    while (rnet_os_monotonic_ms() < deadline) {
        if (rnet_rtt_probe_pump(p, &ms) == 1) {
            *out_rtt_ms = ms;
            rnet_rtt_probe_close(&p);
            return 1;
        }
        (void)rnet_os_poll_recv(p->sock, 5);
    }
    rnet_rtt_probe_close(&p);
    return 0;
}
