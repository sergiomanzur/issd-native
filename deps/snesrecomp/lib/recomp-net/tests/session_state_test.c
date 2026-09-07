#include "recomp_net/recomp_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
static unsigned test_pid(void) { return (unsigned)_getpid(); }
#else
#include <unistd.h>
static unsigned test_pid(void) { return (unsigned)getpid(); }
#endif

enum { kHashCapacity = 512, kStateBytes = 100000 };

typedef struct HostCtx
{
    rnet_u8 slot;
    int published;
    int overflow;
    rnet_u32 hashes[kHashCapacity];
} HostCtx;

static int g_failures;
static rnet_u64 g_now_ms = 1000;

static void fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    g_failures++;
}

static void sample_local(rnet_u32 tick, RNetInputSample *out, void *opaque)
{
    HostCtx *ctx = (HostCtx *)opaque;
    memset(out, 0, sizeof(*out));
    out->size = 4;
    out->bytes[0] = ctx->slot;
    out->bytes[1] = (rnet_u8)tick;
    out->bytes[2] = (rnet_u8)(tick >> 8);
    out->bytes[3] = (rnet_u8)(0xa5u ^ ctx->slot);
    out->valid = 1;
}

static void publish(rnet_u32 tick, const RNetInputSample *by_slot, int slots,
                    void *opaque)
{
    HostCtx *ctx = (HostCtx *)opaque;
    rnet_u32 hash = rnet_checksum(&tick, sizeof(tick));
    int slot;
    for (slot = 0; slot < slots; ++slot)
    {
        hash ^= rnet_checksum(&by_slot[slot], sizeof(by_slot[slot]));
        hash *= 16777619u;
    }
    if (ctx->published < kHashCapacity)
        ctx->hashes[ctx->published] = hash;
    else
        ctx->overflow = 1;
    ctx->published++;
}

static rnet_u64 now_ms(void *opaque)
{
    (void)opaque;
    return g_now_ms;
}

static void pump_pair(RNetSession *a, RNetSession *b)
{
    g_now_ms++;
    rnet_session_pump(a);
    rnet_session_pump(b);
}

static int wait_running(RNetSession *a, RNetSession *b)
{
    int i;
    for (i = 0; i < 3000; ++i)
    {
        pump_pair(a, b);
        if (rnet_session_is_running(a) && rnet_session_is_running(b)) return 1;
    }
    return 0;
}

static int drive_frames(RNetSession *a, RNetSession *b, HostCtx *ha, HostCtx *hb,
                        int target_a, int target_b)
{
    int i;
    for (i = 0; i < 3000; ++i)
    {
        pump_pair(a, b);
        if (ha->published < target_a)
        {
            rnet_u32 tick = rnet_session_sim_tick(a);
            if (rnet_session_try_admit(a, tick)) rnet_session_advance(a);
        }
        if (hb->published < target_b)
        {
            rnet_u32 tick = rnet_session_sim_tick(b);
            if (rnet_session_try_admit(b, tick)) rnet_session_advance(b);
        }
        if (ha->published >= target_a && hb->published >= target_b) return 1;
    }
    return 0;
}

static void compare_hashes(const HostCtx *a, const HostCtx *b, int first, int count)
{
    int i;
    for (i = first; i < first + count; ++i)
    {
        if (a->hashes[i] != b->hashes[i])
        {
            fail("published input hashes differ");
            return;
        }
    }
}

int main(void)
{
    RNetConfig ca;
    RNetConfig cb;
    RNetHostVTable va;
    RNetHostVTable vb;
    HostCtx ha;
    HostCtx hb;
    RNetSession *a = NULL;
    RNetSession *b = NULL;
    rnet_u8 *state = NULL;
    char bind_a[64];
    char bind_b[64];
    unsigned base_port = 40000U + (test_pid() % 9000U) * 2U;
    int i;
    int match = -1;
    int before_load;
    int guest_ready = 0;
    int host_ready = 0;

    memset(&ha, 0, sizeof(ha));
    memset(&hb, 0, sizeof(hb));
    ha.slot = 0;
    hb.slot = 1;
    memset(&va, 0, sizeof(va));
    memset(&vb, 0, sizeof(vb));
    va.sample_local = sample_local;
    va.publish = publish;
    va.now_ms = now_ms;
    va.ctx = &ha;
    vb.sample_local = sample_local;
    vb.publish = publish;
    vb.now_ms = now_ms;
    vb.ctx = &hb;

    rnet_config_init_defaults(&ca);
    cb = ca;
    ca.local_slot = 0;
    cb.local_slot = 1;
    ca.input_delay = cb.input_delay = 3;
    ca.session_id = cb.session_id = 0x53544154u ^ test_pid();
    snprintf(bind_a, sizeof(bind_a), "127.0.0.1:%u", base_port);
    snprintf(bind_b, sizeof(bind_b), "127.0.0.1:%u", base_port + 1U);

    a = rnet_session_create(&ca, &va);
    b = rnet_session_create(&cb, &vb);
    if (a == NULL || b == NULL)
    {
        fail("session create");
        goto done;
    }
    if (rnet_session_start_lan(a, bind_a, bind_b) != 0 ||
        rnet_session_start_lan(b, bind_b, bind_a) != 0)
    {
        fail("LAN start");
        goto done;
    }
    if (!wait_running(a, b))
    {
        fail("sessions did not reach running");
        goto done;
    }
    if (!drive_frames(a, b, &ha, &hb, 180, 180))
    {
        fail("pre-state pipeline stalled");
        goto done;
    }
    compare_hashes(&ha, &hb, 0, 180);

    state = (rnet_u8 *)malloc(kStateBytes);
    if (state == NULL)
    {
        fail("state allocation");
        goto done;
    }
    for (i = 0; i < kStateBytes; ++i) state[i] = (rnet_u8)(i * 37 + 11);

    if (rnet_session_state_probe(a, RNET_STATE_OP_LOAD, 2, kStateBytes,
                                 rnet_checksum(state, kStateBytes)) != 0)
    {
        fail("load probe start");
        goto done;
    }
    before_load = ha.published;
    for (i = 0; i < 3000; ++i)
    {
        rnet_u8 op;
        rnet_u8 slot;
        rnet_u32 size;
        rnet_u32 crc;
        pump_pair(a, b);
        if (rnet_session_state_probe_pending(b, &op, &slot, &size, &crc))
        {
            if (op != RNET_STATE_OP_LOAD || slot != 2 || size != kStateBytes ||
                crc != rnet_checksum(state, kStateBytes))
                fail("load probe metadata");
            if (rnet_session_state_probe_reply(b, 0) != 0) fail("load probe reply");
            break;
        }
    }
    if (i == 3000) fail("guest did not receive load probe");
    if (rnet_session_try_admit(a, rnet_session_sim_tick(a)) ||
        rnet_session_try_admit(b, rnet_session_sim_tick(b)))
        fail("load probe did not stall admission");
    if (ha.published != before_load || hb.published != before_load)
        fail("published during load probe");

    for (i = 0; i < 3000; ++i)
    {
        pump_pair(a, b);
        if (rnet_session_state_probe_take_reply(a, &match)) break;
    }
    if (i == 3000 || match != 0)
    {
        fail("host did not receive hash miss");
        goto done;
    }
    if (rnet_session_state_begin(a, RNET_STATE_OP_LOAD, 2, state, kStateBytes) != 0)
    {
        fail("state transfer start");
        goto done;
    }

    for (i = 0; i < 3000 && (!guest_ready || !host_ready); ++i)
    {
        rnet_u8 op;
        rnet_u8 slot;
        const void *data;
        size_t size;
        pump_pair(a, b);
        if (!guest_ready && rnet_session_state_take_ready(b, &op, &slot, &data, &size))
        {
            guest_ready = 1;
            if (op != RNET_STATE_OP_LOAD || slot != 2 || size != kStateBytes ||
                memcmp(data, state, kStateBytes) != 0)
                fail("guest state payload mismatch");
        }
        if (!host_ready && rnet_session_state_take_ready(a, &op, &slot, &data, &size))
            host_ready = 1;
        if (rnet_session_try_admit(a, rnet_session_sim_tick(a)) ||
            rnet_session_try_admit(b, rnet_session_sim_tick(b)))
            fail("state transfer did not stall admission");
    }
    if (!guest_ready || !host_ready)
    {
        fail("state transfer did not complete");
        goto done;
    }

    rnet_session_state_finish(a, 1);
    rnet_session_state_finish(b, 1);
    {
        const rnet_u8 pad_a[4] = { 0, 0x31, 0, 0xa5 };
        const rnet_u8 pad_b[4] = { 1, 0x72, 0, 0xa4 };
        rnet_session_prime_delay_inputs(a, pad_a, sizeof(pad_a));
        rnet_session_prime_delay_inputs(b, pad_b, sizeof(pad_b));
    }
    if (rnet_session_sim_tick(a) != 0 || rnet_session_sim_tick(b) != 0)
        fail("hard resync did not reset sim tick");
    if (!drive_frames(a, b, &ha, &hb, before_load + 80, before_load + 80))
    {
        fail("post-load pipeline stalled");
        goto done;
    }
    compare_hashes(&ha, &hb, before_load, 80);
    if (rnet_session_input_desync(a, NULL, NULL, NULL) ||
        rnet_session_input_desync(b, NULL, NULL, NULL))
        fail("post-load input desync");
    if (ha.overflow || hb.overflow) fail("publish hash capacity exceeded");

done:
    free(state);
    rnet_session_destroy(a);
    rnet_session_destroy(b);
    if (g_failures == 0)
    {
        printf("session_state_test: ok (strict pipeline + %u-byte load resync)\n",
               (unsigned)kStateBytes);
        return 0;
    }
    fprintf(stderr, "session_state_test: %d failure(s)\n", g_failures);
    return 1;
}
