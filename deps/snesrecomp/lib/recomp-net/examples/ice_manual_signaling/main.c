/*
 * Manual ICE signaling demo.
 *
 * With RNET_ENABLE_ICE=ON, two peers exchange SDP/candidates via files:
 *   ./ice_manual_signaling controlling signal_a_to_b.txt signal_b_to_a.txt
 *   ./ice_manual_signaling controlled  signal_b_to_a.txt signal_a_to_b.txt
 *
 * Without ICE, start_ice returns -1 and the program exits cleanly.
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
    FILE *out_fp;
    const char *out_path;
    int signals_written;
} HostCtx;

static void sample_local(rnet_u32 tick, RNetInputSample *out, void *ctx)
{
    (void)ctx;
    memset(out, 0, sizeof(*out));
    out->size = 2;
    out->bytes[0] = (rnet_u8)(tick & 0xFFu);
    out->bytes[1] = 0x42;
    out->valid = 1;
}

static void publish(rnet_u32 tick, const RNetInputSample *by_slot, int slots, void *ctx)
{
    (void)by_slot;
    (void)slots;
    (void)ctx;
    printf("admit sim=%u\n", tick);
}

static void on_signal(const RNetSignal *msg, void *ctx)
{
    HostCtx *h = (HostCtx *)ctx;
    if (h == NULL || msg == NULL || h->out_fp == NULL)
    {
        return;
    }
    fprintf(h->out_fp, "%d %u %s\n", (int)msg->type, (unsigned)msg->flag, msg->text);
    fflush(h->out_fp);
    h->signals_written++;
    printf("emit signal type=%d flag=%u text_len=%zu\n", (int)msg->type, (unsigned)msg->flag,
           strlen(msg->text));
}

static int consume_inbound(RNetSession *session, const char *path, long *offset)
{
    FILE *fp;
    char line[2200];
    RNetSignal sig;
    long pos;

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return 0;
    }
    if (fseek(fp, *offset, SEEK_SET) != 0)
    {
        fclose(fp);
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        int type = 0;
        unsigned flag = 0;
        char *text;
        char *space1;
        char *space2;

        space1 = strchr(line, ' ');
        if (space1 == NULL)
        {
            continue;
        }
        *space1 = '\0';
        type = atoi(line);
        space2 = strchr(space1 + 1, ' ');
        if (space2 == NULL)
        {
            continue;
        }
        *space2 = '\0';
        flag = (unsigned)strtoul(space1 + 1, NULL, 10);
        text = space2 + 1;
        {
            size_t n = strlen(text);
            while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r'))
            {
                text[--n] = '\0';
            }
        }
        memset(&sig, 0, sizeof(sig));
        sig.type = (RNetSignalType)type;
        sig.flag = (rnet_u8)flag;
        snprintf(sig.text, sizeof(sig.text), "%s", text);
        rnet_session_push_signal(session, &sig);
        printf("push signal type=%d\n", type);
    }
    pos = ftell(fp);
    if (pos >= 0)
    {
        *offset = pos;
    }
    fclose(fp);
    return 1;
}

int main(int argc, char **argv)
{
    RNetConfig cfg;
    RNetIceConfig ice;
    RNetHostVTable host;
    HostCtx ctx;
    RNetSession *session;
    int controlling = 0;
    const char *out_path;
    const char *in_path;
    long in_offset = 0;
    unsigned i;

    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <controlling|controlled> <out_signal_file> <in_signal_file>\n", argv[0]);
        return 1;
    }
    controlling = (strcmp(argv[1], "controlling") == 0) ? 1 : 0;
    out_path = argv[2];
    in_path = argv[3];

    rnet_config_init_defaults(&cfg);
    cfg.local_slot = controlling ? 0 : 1;
    cfg.slot_count = 2;
    rnet_ice_config_init_defaults(&ice);
    ice.controlling = (rnet_u8)controlling;

    memset(&ctx, 0, sizeof(ctx));
    ctx.out_path = out_path;
    ctx.out_fp = fopen(out_path, "ab");
    if (ctx.out_fp == NULL)
    {
        fprintf(stderr, "cannot open %s for append\n", out_path);
        return 1;
    }

    memset(&host, 0, sizeof(host));
    host.sample_local = sample_local;
    host.publish = publish;
    host.on_signal = on_signal;
    host.ctx = &ctx;

    printf("recomp-net %s ICE demo (%s)\n", rnet_version_string(), argv[1]);
    session = rnet_session_create(&cfg, &host);
    if (session == NULL)
    {
        fclose(ctx.out_fp);
        return 1;
    }

    if (rnet_session_start_ice(session, &ice) != 0)
    {
        fprintf(stderr, "start_ice failed (build with -DRNET_ENABLE_ICE=ON and libjuice)\n");
        rnet_session_destroy(session);
        fclose(ctx.out_fp);
        return 2;
    }

    for (i = 0; i < 15000U; ++i)
    {
        consume_inbound(session, in_path, &in_offset);
        rnet_session_pump(session);
        printf("\rice=%s signals_out=%d    ", rnet_ice_state_name(rnet_session_ice_state(session)),
               ctx.signals_written);
        fflush(stdout);
        if (rnet_session_ice_state(session) == RNET_ICE_STATE_COMPLETED)
        {
            printf("\nICE completed\n");
            break;
        }
        if (rnet_session_ice_state(session) == RNET_ICE_STATE_FAILED)
        {
            printf("\nICE failed\n");
            break;
        }
        sleep_ms(20);
    }

    rnet_session_destroy(session);
    fclose(ctx.out_fp);
    return 0;
}
