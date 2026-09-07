#include "recomp_net/ice_rtt.h"

#include "ice/rnet_ice_internal.h"
#include "platform/rnet_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RNET_ICE_RTT_MAGIC "RNETIR1"
#define RNET_ICE_RTT_PING_MS 2000ULL
#define RNET_ICE_RTT_RELAY_FALLBACK_MS 3500ULL

struct RNetIceRttProbe
{
    RNetIceAgent *agent;
    RNetIceRttSignalEmitFn emit;
    void *user;
    int rtt_ms;
    rnet_u64 next_ping_ms;
    rnet_u64 open_ms;
    int relay_fallback_tried;
    char path[16];
};

#if defined(RNET_ENABLE_ICE)

static void ice_rtt_emit_bridge(const RNetSignal *msg, void *user)
{
    RNetIceRttProbe *p = (RNetIceRttProbe *)user;
    if (!p || !p->emit || !msg)
        return;
    p->emit(msg, p->user);
}

static int parse_pong_ts(const char *ts, int *out_rtt_ms)
{
    unsigned long long sent = 0;
    rnet_u64 now;
    int ms;
    if (!ts || !out_rtt_ms || sscanf(ts, "%llu", &sent) != 1)
        return 0;
    now = rnet_os_monotonic_ms();
    if ((rnet_u64)sent > now)
        return 0;
    ms = (int)(now - (rnet_u64)sent);
    if (ms < 0)
        ms = 0;
    if (ms > 60000)
        ms = 60000;
    *out_rtt_ms = ms;
    return 1;
}

static void ice_rtt_send_ping(RNetIceRttProbe *p)
{
    char buf[96];
    int n;
    rnet_u64 now;
    if (!p || !p->agent)
        return;
    now = rnet_os_monotonic_ms();
    n = snprintf(buf, sizeof(buf), "%s\nPING\n%llu\n", RNET_ICE_RTT_MAGIC,
                 (unsigned long long)now);
    if (n <= 0 || (size_t)n >= sizeof(buf))
        return;
    if (rnet_ice_agent_send(p->agent, (const rnet_u8 *)buf, (size_t)n) >= 0)
        p->next_ping_ms = now + RNET_ICE_RTT_PING_MS;
}

static void ice_rtt_handle_app(RNetIceRttProbe *p, const char *msg, size_t len)
{
    char copy[160];
    char *cursor;
    const char *magic;
    const char *op;
    const char *ts;
    if (!p || !msg || len == 0 || len >= sizeof(copy))
        return;
    memcpy(copy, msg, len);
    copy[len] = '\0';
    cursor = copy;
    magic = cursor;
    {
        char *nl = strchr(cursor, '\n');
        if (!nl)
            return;
        *nl = '\0';
        cursor = nl + 1;
    }
    if (strcmp(magic, RNET_ICE_RTT_MAGIC) != 0)
        return;
    op = cursor;
    {
        char *nl = strchr(cursor, '\n');
        if (!nl)
            return;
        *nl = '\0';
        cursor = nl + 1;
    }
    ts = cursor;
    {
        char *nl = strchr(cursor, '\n');
        if (nl)
            *nl = '\0';
    }
    if (strcmp(op, "PING") == 0) {
        char reply[96];
        int n = snprintf(reply, sizeof(reply), "%s\nPONG\n%s\n", RNET_ICE_RTT_MAGIC,
                         ts ? ts : "0");
        if (n > 0 && (size_t)n < sizeof(reply))
            (void)rnet_ice_agent_send(p->agent, (const rnet_u8 *)reply, (size_t)n);
        return;
    }
    if (strcmp(op, "PONG") == 0) {
        int ms = 0;
        if (parse_pong_ts(ts, &ms))
            p->rtt_ms = ms;
    }
}

int rnet_ice_rtt_open(RNetIceRttProbe **out, const RNetIceConfig *ice,
                      RNetIceRttSignalEmitFn emit, void *user)
{
    RNetIceRttProbe *p;
    RNetIceConfig local;
    if (!out || !ice)
        return -1;
    *out = NULL;
    p = (RNetIceRttProbe *)calloc(1, sizeof(*p));
    if (!p)
        return -1;
    local = *ice;
    p->emit = emit;
    p->user = user;
    p->rtt_ms = -1;
    p->open_ms = rnet_os_monotonic_ms();
    p->next_ping_ms = 0;
    snprintf(p->path, sizeof(p->path), "pending");
    p->agent = rnet_ice_agent_create(&local, ice_rtt_emit_bridge, p);
    if (!p->agent) {
        free(p);
        return -1;
    }
    if (rnet_ice_agent_start_gathering(p->agent) != 0) {
        rnet_ice_agent_destroy(p->agent);
        free(p);
        return -1;
    }
    *out = p;
    return 0;
}

void rnet_ice_rtt_close(RNetIceRttProbe **probe)
{
    if (!probe || !*probe)
        return;
    rnet_ice_agent_destroy((*probe)->agent);
    free(*probe);
    *probe = NULL;
}

void rnet_ice_rtt_push_signal(RNetIceRttProbe *probe, const RNetSignal *msg)
{
    if (!probe || !probe->agent || !msg)
        return;
    rnet_ice_agent_push_signal(probe->agent, msg);
}

void rnet_ice_rtt_pump(RNetIceRttProbe *probe)
{
    RNetIceState st;
    rnet_u64 now;
    rnet_u8 buf[256];
    size_t n = 0;

    if (!probe || !probe->agent)
        return;

    rnet_ice_agent_poll(probe->agent);
    st = rnet_ice_agent_state(probe->agent);
    now = rnet_os_monotonic_ms();

    /* Auto relay fallback when host/srflx cannot complete (CGNAT). */
    if (!probe->relay_fallback_tried &&
        !rnet_ice_agent_is_force_relay(probe->agent) &&
        rnet_ice_agent_has_turn(probe->agent) &&
        st != RNET_ICE_STATE_CONNECTED && st != RNET_ICE_STATE_COMPLETED &&
        st != RNET_ICE_STATE_FAILED &&
        (now - probe->open_ms) >= RNET_ICE_RTT_RELAY_FALLBACK_MS) {
        if (rnet_ice_agent_restart_force_relay(probe->agent) == 0) {
            probe->relay_fallback_tried = 1;
            probe->open_ms = now;
            probe->rtt_ms = -1;
            snprintf(probe->path, sizeof(probe->path), "pending");
        } else {
            probe->relay_fallback_tried = 1;
        }
        rnet_ice_agent_poll(probe->agent);
        st = rnet_ice_agent_state(probe->agent);
    }

    if (st == RNET_ICE_STATE_CONNECTED || st == RNET_ICE_STATE_COMPLETED) {
        rnet_ice_agent_selected_info(probe->agent, probe->path, sizeof(probe->path),
                                     NULL, 0, NULL, 0);
        while (rnet_ice_agent_recv(probe->agent, buf, sizeof(buf) - 1, &n) == 0 && n > 0) {
            buf[n] = '\0';
            ice_rtt_handle_app(probe, (const char *)buf, n);
        }
        if (probe->next_ping_ms == 0 || now >= probe->next_ping_ms)
            ice_rtt_send_ping(probe);
    } else if (st == RNET_ICE_STATE_FAILED) {
        snprintf(probe->path, sizeof(probe->path), "failed");
    }
}

RNetIceState rnet_ice_rtt_state(const RNetIceRttProbe *probe)
{
    if (!probe || !probe->agent)
        return RNET_ICE_STATE_IDLE;
    return rnet_ice_agent_state(probe->agent);
}

int rnet_ice_rtt_ms(const RNetIceRttProbe *probe)
{
    return probe ? probe->rtt_ms : -1;
}

void rnet_ice_rtt_selected_path(const RNetIceRttProbe *probe, char *path_out,
                                size_t path_len)
{
    if (!path_out || path_len == 0)
        return;
    if (!probe) {
        snprintf(path_out, path_len, "none");
        return;
    }
    snprintf(path_out, path_len, "%s", probe->path[0] ? probe->path : "pending");
}

#else /* !RNET_ENABLE_ICE */

int rnet_ice_rtt_open(RNetIceRttProbe **out, const RNetIceConfig *ice,
                      RNetIceRttSignalEmitFn emit, void *user)
{
    (void)ice;
    (void)emit;
    (void)user;
    if (out)
        *out = NULL;
    return -1;
}

void rnet_ice_rtt_close(RNetIceRttProbe **probe)
{
    if (probe)
        *probe = NULL;
}

void rnet_ice_rtt_push_signal(RNetIceRttProbe *probe, const RNetSignal *msg)
{
    (void)probe;
    (void)msg;
}

void rnet_ice_rtt_pump(RNetIceRttProbe *probe)
{
    (void)probe;
}

RNetIceState rnet_ice_rtt_state(const RNetIceRttProbe *probe)
{
    (void)probe;
    return RNET_ICE_STATE_IDLE;
}

int rnet_ice_rtt_ms(const RNetIceRttProbe *probe)
{
    (void)probe;
    return -1;
}

void rnet_ice_rtt_selected_path(const RNetIceRttProbe *probe, char *path_out,
                                size_t path_len)
{
    (void)probe;
    if (path_out && path_len)
        snprintf(path_out, path_len, "none");
}

#endif /* RNET_ENABLE_ICE */
