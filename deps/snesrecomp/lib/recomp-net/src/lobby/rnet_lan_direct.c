#include "recomp_net/lan_direct.h"

#include "platform/rnet_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RNET_DJ_MAGIC "RNETDJ1"
#define RNET_DJ_MAX_PKT 1024

struct RNetLanDirectHost {
    rnet_socket sock;
    struct sockaddr_in guest;
    int guest_known;
    char bind_hostport[64];
};

struct RNetLanDirectGuest {
    rnet_socket sock;
    struct sockaddr_in host;
    char host_hostport[64];
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

static int send_text(rnet_socket sock, const struct sockaddr_in *dst,
                     const char *text)
{
    size_t n;
    if (!rnet_os_socket_valid(sock) || !dst || !text)
        return RNET_LAN_DIRECT_ERR_IO;
    n = strlen(text);
    if (n == 0 || n > RNET_DJ_MAX_PKT)
        return RNET_LAN_DIRECT_ERR_IO;
    if (rnet_os_sendto(sock, text, n, dst) < 0)
        return RNET_LAN_DIRECT_ERR_IO;
    return RNET_LAN_DIRECT_OK;
}

static int build_join_req(char *buf, size_t cap, const char *game,
                          const char *version, const char *password,
                          const char *player)
{
    size_t o = 0;
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "JOIN_REQ") ||
        !append_line(buf, cap, &o, game ? game : "") ||
        !append_line(buf, cap, &o, version ? version : "") ||
        !append_line(buf, cap, &o, password ? password : "") ||
        !append_line(buf, cap, &o, player && player[0] ? player : "Player"))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int clamp_direct_input_delay(int delay)
{
    if (delay < 2)
        return 2;
    if (delay > 20)
        return 20;
    return delay;
}

static int parse_direct_input_delay_line(const char *line, int def)
{
    long v;
    char *end;
    if (!line || !line[0])
        return def;
    v = strtol(line, &end, 10);
    if (end == line || *end != '\0')
        return def;
    return clamp_direct_input_delay((int)v);
}

static int clamp_direct_prediction(int p)
{
    if (p < 2)
        return 2;
    if (p > 16)
        return 16;
    return p;
}

static int parse_direct_prediction_line(const char *line, int def)
{
    long v;
    char *end;
    if (!line || !line[0])
        return clamp_direct_prediction(def);
    v = strtol(line, &end, 10);
    if (end == line || *end != '\0')
        return clamp_direct_prediction(def);
    return clamp_direct_prediction((int)v);
}

static int parse_direct_bool_line(const char *line, int def)
{
    long v;
    char *end;
    if (!line || !line[0])
        return def ? 1 : 0;
    v = strtol(line, &end, 10);
    if (end == line || *end != '\0')
        return def ? 1 : 0;
    return v != 0 ? 1 : 0;
}

static int build_join_ok(char *buf, size_t cap, const RNetLanLobby *room)
{
    char slot[8];
    char delay_line[16];
    char rollback_line[8];
    char pred_line[16];
    size_t o = 0;
    if (!room)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    snprintf(slot, sizeof(slot), "%d", room->host_slot == 1 ? 1 : 0);
    snprintf(delay_line, sizeof(delay_line), "%d",
             clamp_direct_input_delay(room->input_delay >= 2 ? room->input_delay
                                                             : 2));
    snprintf(rollback_line, sizeof(rollback_line), "%d",
             room->rollback ? 1 : 0);
    snprintf(pred_line, sizeof(pred_line), "%d",
             clamp_direct_prediction(room->input_prediction >= 2
                                         ? room->input_prediction
                                         : 4));
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "JOIN_OK") ||
        !append_line(buf, cap, &o, room->endpoint) ||
        !append_line(buf, cap, &o, room->host_name) ||
        !append_line(buf, cap, &o, room->joiner_name) ||
        !append_line(buf, cap, &o, slot) ||
        !append_line(buf, cap, &o, room->name) ||
        !append_line(buf, cap, &o, room->game) ||
        !append_line(buf, cap, &o, room->game_version) ||
        !append_line(buf, cap, &o, delay_line) ||
        !append_line(buf, cap, &o, rollback_line) ||
        !append_line(buf, cap, &o, pred_line))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int build_join_nak(char *buf, size_t cap, const char *code)
{
    size_t o = 0;
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "JOIN_NAK") ||
        !append_line(buf, cap, &o, code ? code : "reject"))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int build_simple(char *buf, size_t cap, const char *op)
{
    size_t o = 0;
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, op ? op : ""))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int build_start(char *buf, size_t cap, const RNetLanLobby *room)
{
    char delay_line[16];
    char rollback_line[8];
    char pred_line[16];
    size_t o = 0;
    int delay = 2;
    int rollback = 0;
    int pred = 4;
    if (room) {
        delay = clamp_direct_input_delay(room->input_delay >= 2 ? room->input_delay
                                                                : 2);
        rollback = room->rollback ? 1 : 0;
        pred = clamp_direct_prediction(room->input_prediction >= 2
                                           ? room->input_prediction
                                           : 4);
    }
    snprintf(delay_line, sizeof(delay_line), "%d", delay);
    snprintf(rollback_line, sizeof(rollback_line), "%d", rollback);
    snprintf(pred_line, sizeof(pred_line), "%d", pred);
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "START") ||
        !append_line(buf, cap, &o, delay_line) ||
        !append_line(buf, cap, &o, rollback_line) ||
        !append_line(buf, cap, &o, pred_line))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int build_caps(char *buf, size_t cap, const RNetLanLobby *room)
{
    char delay_line[16];
    char rollback_line[8];
    char pred_line[16];
    size_t o = 0;
    int delay = 2;
    int rollback = 0;
    int pred = 4;
    if (room) {
        delay = clamp_direct_input_delay(room->input_delay >= 2 ? room->input_delay
                                                                : 2);
        rollback = room->rollback ? 1 : 0;
        pred = clamp_direct_prediction(room->input_prediction >= 2
                                           ? room->input_prediction
                                           : 4);
    }
    snprintf(delay_line, sizeof(delay_line), "%d", delay);
    snprintf(rollback_line, sizeof(rollback_line), "%d", rollback);
    snprintf(pred_line, sizeof(pred_line), "%d", pred);
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "CAPS") ||
        !append_line(buf, cap, &o, delay_line) ||
        !append_line(buf, cap, &o, rollback_line) ||
        !append_line(buf, cap, &o, pred_line))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int nak_code_to_rc(const char *code)
{
    if (!code)
        return RNET_LAN_DIRECT_ERR_IO;
    if (strcmp(code, "password") == 0)
        return RNET_LAN_DIRECT_ERR_PASSWORD;
    if (strcmp(code, "full") == 0)
        return RNET_LAN_DIRECT_ERR_FULL;
    if (strcmp(code, "identity") == 0)
        return RNET_LAN_DIRECT_ERR_IDENTITY;
    if (strcmp(code, "started") == 0)
        return RNET_LAN_DIRECT_ERR_STARTED;
    return RNET_LAN_DIRECT_ERR_IO;
}

int rnet_lan_direct_host_open(RNetLanDirectHost **out, const char *bind_hostport,
                              const RNetLanLobby *room)
{
    RNetLanDirectHost *h;
    char host[128];
    rnet_u16 port = 0;
    struct sockaddr_in addr;
    (void)room;
    if (!out || !bind_hostport || !bind_hostport[0])
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    *out = NULL;
    rnet_os_startup();
    if (rnet_os_parse_hostport(bind_hostport, host, sizeof(host), &port) != 0 ||
        port == 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    if (rnet_os_resolve_sockaddr(host[0] ? host : "0.0.0.0", port, &addr) != 0)
        return RNET_LAN_DIRECT_ERR_IO;
    h = (RNetLanDirectHost *)calloc(1, sizeof(*h));
    if (!h)
        return RNET_LAN_DIRECT_ERR_IO;
    h->sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(h->sock)) {
        free(h);
        return RNET_LAN_DIRECT_ERR_IO;
    }
    (void)rnet_os_setsockopt_reuseaddr(h->sock, 1);
    if (rnet_os_bind(h->sock, &addr) != 0 ||
        rnet_os_set_nonblocking(h->sock) != 0) {
        rnet_os_socket_destroy(&h->sock);
        free(h);
        return RNET_LAN_DIRECT_ERR_IO;
    }
    snprintf(h->bind_hostport, sizeof(h->bind_hostport), "%s", bind_hostport);
    *out = h;
    return RNET_LAN_DIRECT_OK;
}

void rnet_lan_direct_host_close(RNetLanDirectHost **host)
{
    if (!host || !*host)
        return;
    rnet_os_socket_destroy(&(*host)->sock);
    free(*host);
    *host = NULL;
}

static int build_ping(char *buf, size_t cap, rnet_u64 t_ms)
{
    char ts[32];
    size_t o = 0;
    snprintf(ts, sizeof(ts), "%llu", (unsigned long long)t_ms);
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "PING") || !append_line(buf, cap, &o, ts))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
}

static int build_pong(char *buf, size_t cap, const char *ts)
{
    size_t o = 0;
    if (!append_line(buf, cap, &o, RNET_DJ_MAGIC) ||
        !append_line(buf, cap, &o, "PONG") ||
        !append_line(buf, cap, &o, ts ? ts : "0"))
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return RNET_LAN_DIRECT_OK;
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

int rnet_lan_direct_host_ping(RNetLanDirectHost *host)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!host || !host->guest_known)
        return RNET_LAN_DIRECT_ERR_IO;
    if (build_ping(buf, sizeof(buf), rnet_os_monotonic_ms()) != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return send_text(host->sock, &host->guest, buf);
}

int rnet_lan_direct_host_pump(RNetLanDirectHost *host, RNetLanLobby *room,
                              int *out_rtt_ms)
{
    char buf[RNET_DJ_MAX_PKT + 1];
    struct sockaddr_in src;
    int would_block = 0;
    int n;
    int changed = 0;
    char *cursor;
    const char *magic;
    const char *op;

    if (!host || !room || !rnet_os_socket_valid(host->sock))
        return 0;

    for (;;) {
        n = rnet_os_recvfrom(host->sock, buf, sizeof(buf) - 1, &src,
                             &would_block);
        if (n < 0) {
            if (would_block)
                break;
            break;
        }
        if (n == 0)
            continue;
        buf[n] = '\0';
        cursor = buf;
        magic = next_line(&cursor);
        op = next_line(&cursor);
        if (!magic || strcmp(magic, RNET_DJ_MAGIC) != 0 || !op)
            continue;

        if (strcmp(op, "PING") == 0) {
            const char *ts = next_line(&cursor);
            char reply[RNET_DJ_MAX_PKT];
            if (build_pong(reply, sizeof(reply), ts) == 0)
                (void)send_text(host->sock, &src, reply);
            continue;
        }
        if (strcmp(op, "PONG") == 0) {
            const char *ts = next_line(&cursor);
            if (out_rtt_ms)
                (void)rtt_from_pong_ts(ts, out_rtt_ms);
            continue;
        }

        if (strcmp(op, "JOIN_REQ") == 0) {
            const char *game = next_line(&cursor);
            const char *version = next_line(&cursor);
            const char *password = next_line(&cursor);
            const char *player = next_line(&cursor);
            char reply[RNET_DJ_MAX_PKT];
            const char *nak = NULL;

            if (!game)
                game = "";
            if (!version)
                version = "";
            if (!password)
                password = "";
            if (!player || !player[0])
                player = "Player";

            if (strcmp(game, room->game) != 0 ||
                strcmp(version, room->game_version) != 0)
                nak = "identity";
            else if (strcmp(password, room->password) != 0)
                nak = "password";
            else if (room->started)
                nak = "started";
            else if (room->joiner_name[0] != '\0')
                nak = "full";

            if (nak) {
                if (build_join_nak(reply, sizeof(reply), nak) == 0)
                    (void)send_text(host->sock, &src, reply);
                continue;
            }

            snprintf(room->joiner_name, sizeof(room->joiner_name), "%s",
                     player);
            room->started = 0;
            host->guest = src;
            host->guest_known = 1;
            changed = 1;
            if (build_join_ok(reply, sizeof(reply), room) == 0)
                (void)send_text(host->sock, &src, reply);
        } else if (strcmp(op, "LEAVE") == 0) {
            if (host->guest_known && room->joiner_name[0]) {
                room->joiner_name[0] = '\0';
                room->started = 0;
                host->guest_known = 0;
                changed = 1;
            }
        }
    }
    return changed;
}

int rnet_lan_direct_host_notify_start(RNetLanDirectHost *host,
                                      const RNetLanLobby *room)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!host || !host->guest_known)
        return RNET_LAN_DIRECT_ERR_IO;
    if (build_start(buf, sizeof(buf), room) != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return send_text(host->sock, &host->guest, buf);
}

int rnet_lan_direct_host_notify_caps(RNetLanDirectHost *host,
                                     const RNetLanLobby *room)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!host || !host->guest_known)
        return RNET_LAN_DIRECT_ERR_IO;
    if (build_caps(buf, sizeof(buf), room) != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return send_text(host->sock, &host->guest, buf);
}

int rnet_lan_direct_host_notify_kick(RNetLanDirectHost *host)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!host || !host->guest_known)
        return RNET_LAN_DIRECT_ERR_IO;
    if (build_simple(buf, sizeof(buf), "KICK") != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    (void)send_text(host->sock, &host->guest, buf);
    host->guest_known = 0;
    return RNET_LAN_DIRECT_OK;
}

int rnet_lan_direct_host_notify_close(RNetLanDirectHost *host)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!host)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    if (!host->guest_known)
        return RNET_LAN_DIRECT_OK;
    if (build_simple(buf, sizeof(buf), "CLOSE") != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    (void)send_text(host->sock, &host->guest, buf);
    host->guest_known = 0;
    return RNET_LAN_DIRECT_OK;
}

int rnet_lan_direct_guest_join(const char *host_hostport,
                               const char *expected_game,
                               const char *expected_version,
                               const char *password, const char *player_name,
                               const char *guest_bind_hostport, int timeout_ms,
                               RNetLanLobby *out_room,
                               RNetLanDirectGuest **out_guest)
{
    RNetLanDirectGuest *g;
    char host[128];
    char bind_host[128];
    rnet_u16 port = 0;
    rnet_u16 bind_port = 0;
    struct sockaddr_in bind_addr;
    char req[RNET_DJ_MAX_PKT];
    char buf[RNET_DJ_MAX_PKT + 1];
    rnet_u64 deadline;
    int rc;

    if (!out_guest || !out_room || !host_hostport || !host_hostport[0] ||
        !expected_game || !expected_game[0])
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    *out_guest = NULL;
    memset(out_room, 0, sizeof(*out_room));
    if (timeout_ms <= 0)
        timeout_ms = 2000;

    rnet_os_startup();
    if (rnet_os_parse_hostport(host_hostport, host, sizeof(host), &port) != 0 ||
        !host[0] || port == 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;

    g = (RNetLanDirectGuest *)calloc(1, sizeof(*g));
    if (!g)
        return RNET_LAN_DIRECT_ERR_IO;
    if (rnet_os_resolve_sockaddr(host, port, &g->host) != 0) {
        free(g);
        return RNET_LAN_DIRECT_ERR_IO;
    }
    snprintf(g->host_hostport, sizeof(g->host_hostport), "%s", host_hostport);

    g->sock = rnet_os_socket_create_dgram();
    if (!rnet_os_socket_valid(g->sock)) {
        free(g);
        return RNET_LAN_DIRECT_ERR_IO;
    }
    (void)rnet_os_setsockopt_reuseaddr(g->sock, 1);

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    if (guest_bind_hostport && guest_bind_hostport[0] &&
        rnet_os_parse_hostport(guest_bind_hostport, bind_host,
                               sizeof(bind_host), &bind_port) == 0 &&
        bind_port != 0) {
        if (rnet_os_resolve_sockaddr(bind_host[0] ? bind_host : "0.0.0.0",
                                     bind_port, &bind_addr) != 0) {
            rnet_os_socket_destroy(&g->sock);
            free(g);
            return RNET_LAN_DIRECT_ERR_IO;
        }
    } else {
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        bind_addr.sin_port = 0;
    }
    if (rnet_os_bind(g->sock, &bind_addr) != 0 ||
        rnet_os_set_nonblocking(g->sock) != 0) {
        rnet_os_socket_destroy(&g->sock);
        free(g);
        return RNET_LAN_DIRECT_ERR_IO;
    }

    rc = build_join_req(req, sizeof(req), expected_game, expected_version,
                        password, player_name);
    if (rc != RNET_LAN_DIRECT_OK) {
        rnet_lan_direct_guest_close(&g);
        return rc;
    }

    deadline = rnet_os_monotonic_ms() + (rnet_u64)timeout_ms;
    /* Retransmit a few times — UDP can drop the first probe. */
    for (;;) {
        rnet_u64 now = rnet_os_monotonic_ms();
        int wait_ms;
        int n;
        int would_block = 0;
        struct sockaddr_in src;
        char *cursor;
        const char *magic;
        const char *op;

        if (now >= deadline) {
            rnet_lan_direct_guest_close(&g);
            return RNET_LAN_DIRECT_ERR_TIMEOUT;
        }
        (void)send_text(g->sock, &g->host, req);
        wait_ms = (int)(deadline - now);
        if (wait_ms > 400)
            wait_ms = 400;
        if (rnet_os_poll_recv(g->sock, wait_ms) <= 0)
            continue;

        n = rnet_os_recvfrom(g->sock, buf, sizeof(buf) - 1, &src, &would_block);
        if (n <= 0)
            continue;
        buf[n] = '\0';
        cursor = buf;
        magic = next_line(&cursor);
        op = next_line(&cursor);
        if (!magic || strcmp(magic, RNET_DJ_MAGIC) != 0 || !op)
            continue;
        if (strcmp(op, "JOIN_NAK") == 0) {
            const char *code = next_line(&cursor);
            rc = nak_code_to_rc(code);
            rnet_lan_direct_guest_close(&g);
            return rc;
        }
        if (strcmp(op, "JOIN_OK") != 0)
            continue;

        {
            const char *endpoint = next_line(&cursor);
            const char *host_name = next_line(&cursor);
            const char *joiner = next_line(&cursor);
            const char *slot = next_line(&cursor);
            const char *name = next_line(&cursor);
            const char *game = next_line(&cursor);
            const char *version = next_line(&cursor);
            const char *delay_line = next_line(&cursor);
            const char *rollback_line = next_line(&cursor);
            const char *pred_line = next_line(&cursor);
            snprintf(out_room->endpoint, sizeof(out_room->endpoint), "%s",
                     endpoint && endpoint[0] ? endpoint : host_hostport);
            snprintf(out_room->host_name, sizeof(out_room->host_name), "%s",
                     host_name ? host_name : "Host");
            snprintf(out_room->joiner_name, sizeof(out_room->joiner_name), "%s",
                     joiner && joiner[0] ? joiner
                                         : (player_name && player_name[0]
                                                ? player_name
                                                : "Player"));
            out_room->host_slot = (slot && strcmp(slot, "1") == 0) ? 1 : 0;
            snprintf(out_room->name, sizeof(out_room->name), "%s",
                     name && name[0] ? name : "LAN Lobby");
            snprintf(out_room->game, sizeof(out_room->game), "%s",
                     game && game[0] ? game : expected_game);
            snprintf(out_room->game_version, sizeof(out_room->game_version),
                     "%s",
                     version && version[0] ? version : expected_version);
            out_room->started = 0;
            out_room->password[0] = '\0';
            out_room->input_delay =
                parse_direct_input_delay_line(delay_line, 2);
            /* Optional V3 lines — older hosts omit them. */
            out_room->rollback = parse_direct_bool_line(rollback_line, 0);
            out_room->input_prediction =
                parse_direct_prediction_line(pred_line, 4);
        }
        g->host = src; /* reply path may differ from typed dest after NAT */
        *out_guest = g;
        return RNET_LAN_DIRECT_OK;
    }
}

void rnet_lan_direct_guest_close(RNetLanDirectGuest **guest)
{
    if (!guest || !*guest)
        return;
    rnet_os_socket_destroy(&(*guest)->sock);
    free(*guest);
    *guest = NULL;
}

int rnet_lan_direct_guest_ping(RNetLanDirectGuest *guest)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!guest || !rnet_os_socket_valid(guest->sock))
        return RNET_LAN_DIRECT_ERR_IO;
    if (build_ping(buf, sizeof(buf), rnet_os_monotonic_ms()) != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return send_text(guest->sock, &guest->host, buf);
}

int rnet_lan_direct_guest_pump(RNetLanDirectGuest *guest, RNetLanLobby *room,
                               int *out_rtt_ms)
{
    char buf[RNET_DJ_MAX_PKT + 1];
    struct sockaddr_in src;
    int would_block = 0;
    int n;
    char *cursor;
    const char *magic;
    const char *op;

    if (!guest || !rnet_os_socket_valid(guest->sock))
        return RNET_LAN_DIRECT_ERR_IO;

    n = rnet_os_recvfrom(guest->sock, buf, sizeof(buf) - 1, &src, &would_block);
    if (n < 0)
        return would_block ? 0 : RNET_LAN_DIRECT_ERR_IO;
    if (n == 0)
        return 0;
    buf[n] = '\0';
    cursor = buf;
    magic = next_line(&cursor);
    op = next_line(&cursor);
    if (!magic || strcmp(magic, RNET_DJ_MAGIC) != 0 || !op)
        return 0;
    if (strcmp(op, "PING") == 0) {
        const char *ts = next_line(&cursor);
        char reply[RNET_DJ_MAX_PKT];
        if (build_pong(reply, sizeof(reply), ts) == 0)
            (void)send_text(guest->sock, &src, reply);
        return 0;
    }
    if (strcmp(op, "PONG") == 0) {
        const char *ts = next_line(&cursor);
        if (out_rtt_ms && rtt_from_pong_ts(ts, out_rtt_ms))
            return 3;
        return 0;
    }
    if (strcmp(op, "START") == 0) {
        const char *delay_line = next_line(&cursor);
        const char *rollback_line = next_line(&cursor);
        const char *pred_line = next_line(&cursor);
        if (room) {
            room->started = 1;
            room->input_delay = parse_direct_input_delay_line(delay_line, 2);
            room->rollback = parse_direct_bool_line(rollback_line, room->rollback);
            room->input_prediction =
                parse_direct_prediction_line(pred_line, room->input_prediction);
        }
        return 1;
    }
    if (strcmp(op, "CAPS") == 0) {
        const char *delay_line = next_line(&cursor);
        const char *rollback_line = next_line(&cursor);
        const char *pred_line = next_line(&cursor);
        if (room) {
            room->input_delay = parse_direct_input_delay_line(delay_line, 2);
            room->rollback = parse_direct_bool_line(rollback_line, room->rollback);
            room->input_prediction =
                parse_direct_prediction_line(pred_line, room->input_prediction);
        }
        return 0;
    }
    if (strcmp(op, "KICK") == 0 || strcmp(op, "CLOSE") == 0)
        return 2;
    return 0;
}

int rnet_lan_direct_guest_leave(RNetLanDirectGuest *guest)
{
    char buf[RNET_DJ_MAX_PKT];
    if (!guest || !rnet_os_socket_valid(guest->sock))
        return RNET_LAN_DIRECT_ERR_IO;
    if (build_simple(buf, sizeof(buf), "LEAVE") != 0)
        return RNET_LAN_DIRECT_ERR_ARGUMENT;
    return send_text(guest->sock, &guest->host, buf);
}
