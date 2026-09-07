/*
 * Two-process LAN delay-sync demo.
 *
 * Terminal A (slot 0 / sim authority):
 *   ./lan_delay_2p 0 7777
 *
 * Terminal B (slot 1):
 *   ./lan_delay_2p 1 0 127.0.0.1:7777
 *
 * Optional env: RNET_DELAY (default 2), RNET_SESSION_ID (default 1), RNET_TICKS (default 120)
 */
#if !defined(_WIN32)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "recomp_net/recomp_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(unsigned ms) { Sleep(ms); }
#else
#include <time.h>
static void sleep_ms(unsigned ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)(ms % 1000U) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

typedef struct HostCtx
{
    rnet_u8 local_slot;
    rnet_u32 last_published;
    int published_count;
} HostCtx;

static void sample_local(rnet_u32 tick, RNetInputSample *out, void *ctx)
{
    HostCtx *h = (HostCtx *)ctx;
    memset(out, 0, sizeof(*out));
    out->size = 4;
    out->bytes[0] = h->local_slot;
    out->bytes[1] = (rnet_u8)(tick & 0xFFu);
    out->bytes[2] = (rnet_u8)((tick >> 8) & 0xFFu);
    out->bytes[3] = 0xA5;
    out->valid = 1;
}

static void publish(rnet_u32 tick, const RNetInputSample *by_slot, int slots, void *ctx)
{
    HostCtx *h = (HostCtx *)ctx;
    int i;
    h->last_published = tick;
    h->published_count++;
    printf("[slot %u] admit sim=%u", (unsigned)h->local_slot, tick);
    for (i = 0; i < slots; ++i)
    {
        printf("  s%u:{%u bytes}", (unsigned)i, (unsigned)by_slot[i].size);
    }
    printf("\n");
    fflush(stdout);
}

static unsigned env_u(const char *name, unsigned def)
{
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0')
    {
        return def;
    }
    return (unsigned)strtoul(v, NULL, 10);
}

int main(int argc, char **argv)
{
    RNetConfig cfg;
    RNetHostVTable host;
    HostCtx ctx;
    RNetSession *session;
    char bind_spec[64];
    const char *peer;
    unsigned target_ticks;
    unsigned wait_iters = 0;
    rnet_u16 bind_port;

    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <local_slot> <bind_port> [peer_host:port]\n", argv[0]);
        return 1;
    }

    rnet_config_init_defaults(&cfg);
    cfg.local_slot = (rnet_u8)strtoul(argv[1], NULL, 10);
    cfg.slot_count = 2;
    cfg.input_delay = (rnet_u8)env_u("RNET_DELAY", 2);
    cfg.session_id = env_u("RNET_SESSION_ID", 1);
    bind_port = (rnet_u16)strtoul(argv[2], NULL, 10);
    peer = argc >= 4 ? argv[3] : "";
    snprintf(bind_spec, sizeof(bind_spec), "0.0.0.0:%u", (unsigned)bind_port);
    target_ticks = env_u("RNET_TICKS", 120);

    memset(&ctx, 0, sizeof(ctx));
    ctx.local_slot = cfg.local_slot;
    memset(&host, 0, sizeof(host));
    host.sample_local = sample_local;
    host.publish = publish;
    host.ctx = &ctx;

    printf("recomp-net %s  slot=%u delay=%u bind=%s peer=%s\n", rnet_version_string(),
           (unsigned)cfg.local_slot, (unsigned)cfg.input_delay, bind_spec,
           peer[0] ? peer : "(accept first)");

    session = rnet_session_create(&cfg, &host);
    if (session == NULL)
    {
        fprintf(stderr, "create failed\n");
        return 1;
    }
    if (rnet_session_start_lan(session, bind_spec, peer) != 0)
    {
        fprintf(stderr, "start_lan failed\n");
        rnet_session_destroy(session);
        return 1;
    }

    while (ctx.published_count < (int)target_ticks && wait_iters < 60000U)
    {
        rnet_session_pump(session);
        if (rnet_session_is_running(session))
        {
            rnet_u32 sim = rnet_session_sim_tick(session);
            if (rnet_session_try_admit(session, sim))
            {
                rnet_session_advance(session);
            }
        }
        sleep_ms(1);
        wait_iters++;
    }

    printf("done: published=%d final_sim=%u running=%d\n", ctx.published_count,
           rnet_session_sim_tick(session), rnet_session_is_running(session));
    rnet_session_destroy(session);
    return (ctx.published_count > 0) ? 0 : 2;
}
