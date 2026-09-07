#include "recomp_net/ice_xfer.h"

#include "ice/rnet_ice_internal.h"
#include "platform/rnet_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XFER_MAGIC "RNETXF1"
#define XFER_MAGIC_LEN 7
#define XFER_PAYLOAD 1024u
#define XFER_WINDOW 16u
#define XFER_RETRY_MS 200ull
#define XFER_CONNECT_MS 25000ull
#define XFER_Q 16
#define XFER_READY 4
#define XFER_RECV_MAX 2048

enum {
    XF_DATA = 1,
    XF_ACK = 2,
    XF_FAIL = 3
};

typedef struct XferBlob {
    uint8_t *data;
    size_t len;
    uint32_t id;
    uint64_t unacked;
    uint64_t next_off;
    rnet_u64 last_send_ms;
} XferBlob;

struct RNetIceXfer {
    RNetIceAgent *agent;
    RNetIceXferSignalEmitFn emit;
    void *user;
    rnet_u64 open_ms;
    int failed;
    char error[96];
    uint32_t next_id;
    XferBlob send_q[XFER_Q];
    int send_n;
    uint8_t *recv_buf;
    size_t recv_total;
    size_t recv_got;
    uint32_t recv_id;
    int recv_active;
    struct {
        uint8_t *data;
        size_t len;
    } ready[XFER_READY];
    int ready_n;
};

#if defined(RNET_ENABLE_ICE)

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64(uint8_t *p, uint64_t v)
{
    put_u32(p, (uint32_t)v);
    put_u32(p + 4, (uint32_t)(v >> 32));
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const uint8_t *p)
{
    return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p + 4) << 32);
}

static void xfer_emit_bridge(const RNetSignal *msg, void *user)
{
    RNetIceXfer *x = (RNetIceXfer *)user;
    if (!x || !x->emit || !msg)
        return;
    x->emit(msg, x->user);
}

static void free_blob(XferBlob *b)
{
    free(b->data);
    memset(b, 0, sizeof(*b));
}

static void fail_xfer(RNetIceXfer *x, const char *err)
{
    if (!x || x->failed)
        return;
    x->failed = 1;
    snprintf(x->error, sizeof(x->error), "%s", err && err[0] ? err : "transfer failed");
}

static void send_fail_pkt(RNetIceXfer *x, const char *err)
{
    uint8_t pkt[128];
    size_t nerr;
    if (!x || !x->agent)
        return;
    nerr = err && err[0] ? strlen(err) : 0;
    if (nerr > 80)
        nerr = 80;
    memcpy(pkt, XFER_MAGIC, XFER_MAGIC_LEN);
    pkt[XFER_MAGIC_LEN] = XF_FAIL;
    pkt[XFER_MAGIC_LEN + 1] = (uint8_t)nerr;
    if (nerr)
        memcpy(pkt + XFER_MAGIC_LEN + 2, err, nerr);
    (void)rnet_ice_agent_send(x->agent, pkt, XFER_MAGIC_LEN + 2 + nerr);
}

static void send_ack(RNetIceXfer *x, uint32_t blob_id, uint64_t got)
{
    uint8_t pkt[7 + 1 + 4 + 8];
    memcpy(pkt, XFER_MAGIC, XFER_MAGIC_LEN);
    pkt[XFER_MAGIC_LEN] = XF_ACK;
    put_u32(pkt + XFER_MAGIC_LEN + 1, blob_id);
    put_u64(pkt + XFER_MAGIC_LEN + 5, got);
    (void)rnet_ice_agent_send(x->agent, pkt, sizeof(pkt));
}

static void send_data(RNetIceXfer *x, const XferBlob *b, uint64_t off)
{
    uint8_t pkt[7 + 1 + 4 + 8 + 8 + 2 + XFER_PAYLOAD];
    uint16_t n;
    size_t hdr = 7 + 1 + 4 + 8 + 8 + 2;
    if (!b || off > b->len)
        return;
    n = (uint16_t)((b->len - (size_t)off) > XFER_PAYLOAD ? XFER_PAYLOAD
                                                         : (b->len - (size_t)off));
    memcpy(pkt, XFER_MAGIC, XFER_MAGIC_LEN);
    pkt[XFER_MAGIC_LEN] = XF_DATA;
    put_u32(pkt + 8, b->id);
    put_u64(pkt + 12, (uint64_t)b->len);
    put_u64(pkt + 20, off);
    put_u16(pkt + 28, n);
    if (n)
        memcpy(pkt + hdr, b->data + (size_t)off, n);
    (void)rnet_ice_agent_send(x->agent, pkt, hdr + n);
}

static void handle_ack(RNetIceXfer *x, uint32_t blob_id, uint64_t got)
{
    XferBlob *b;
    if (x->send_n <= 0)
        return;
    b = &x->send_q[0];
    if (b->id != blob_id)
        return;
    if (got < b->unacked)
        return;
    b->unacked = got;
    if (got > b->next_off)
        b->next_off = got;
    if (b->unacked >= b->len) {
        free_blob(b);
        x->send_n--;
        if (x->send_n > 0)
            memmove(&x->send_q[0], &x->send_q[1],
                    (size_t)x->send_n * sizeof(x->send_q[0]));
        memset(&x->send_q[x->send_n], 0, sizeof(x->send_q[0]));
    }
}

static int push_ready(RNetIceXfer *x, uint8_t *data, size_t len)
{
    if (x->ready_n >= XFER_READY) {
        free(data);
        return -1;
    }
    x->ready[x->ready_n].data = data;
    x->ready[x->ready_n].len = len;
    x->ready_n++;
    return 0;
}

static void handle_data(RNetIceXfer *x, uint32_t blob_id, uint64_t total, uint64_t off,
                        const uint8_t *payload, uint16_t n)
{
    if (total > (size_t)-1 / 2) {
        fail_xfer(x, "package too large");
        return;
    }
    if (!x->recv_active || x->recv_id != blob_id) {
        free(x->recv_buf);
        x->recv_buf = NULL;
        x->recv_got = 0;
        x->recv_total = (size_t)total;
        x->recv_id = blob_id;
        x->recv_active = 1;
        if (total > 0) {
            x->recv_buf = (uint8_t *)malloc((size_t)total);
            if (!x->recv_buf) {
                fail_xfer(x, "out of memory");
                x->recv_active = 0;
                return;
            }
        }
    }
    if (off != x->recv_got) {
        send_ack(x, blob_id, (uint64_t)x->recv_got);
        return;
    }
    if (n > 0) {
        if (off + n > x->recv_total) {
            fail_xfer(x, "chunk overrun");
            return;
        }
        memcpy(x->recv_buf + (size_t)off, payload, n);
        x->recv_got += n;
    }
    send_ack(x, blob_id, (uint64_t)x->recv_got);
    if (x->recv_got >= x->recv_total) {
        (void)push_ready(x, x->recv_buf, x->recv_total);
        x->recv_buf = NULL;
        x->recv_active = 0;
        x->recv_got = 0;
        x->recv_total = 0;
    }
}

static void handle_pkt(RNetIceXfer *x, const uint8_t *buf, size_t len)
{
    uint8_t type;
    if (!buf || len < XFER_MAGIC_LEN + 1)
        return;
    if (memcmp(buf, XFER_MAGIC, XFER_MAGIC_LEN) != 0)
        return;
    type = buf[XFER_MAGIC_LEN];
    if (type == XF_FAIL) {
        char err[81];
        uint8_t nerr = (len > XFER_MAGIC_LEN + 1) ? buf[XFER_MAGIC_LEN + 1] : 0;
        if ((size_t)nerr + XFER_MAGIC_LEN + 2 > len)
            nerr = 0;
        err[0] = '\0';
        if (nerr) {
            memcpy(err, buf + XFER_MAGIC_LEN + 2, nerr);
            err[nerr] = '\0';
        }
        fail_xfer(x, err[0] ? err : "peer failed");
        return;
    }
    if (type == XF_ACK && len >= 7 + 1 + 4 + 8) {
        handle_ack(x, get_u32(buf + 8), get_u64(buf + 12));
        return;
    }
    if (type == XF_DATA && len >= 7 + 1 + 4 + 8 + 8 + 2) {
        uint16_t n = get_u16(buf + 28);
        if ((size_t)30 + n > len)
            return;
        handle_data(x, get_u32(buf + 8), get_u64(buf + 12), get_u64(buf + 20),
                    buf + 30, n);
    }
}

int rnet_ice_xfer_open(RNetIceXfer **out, const RNetIceConfig *ice,
                       RNetIceXferSignalEmitFn emit, void *user)
{
    RNetIceXfer *x;
    if (!out || !ice)
        return -1;
    *out = NULL;
    x = (RNetIceXfer *)calloc(1, sizeof(*x));
    if (!x)
        return -1;
    x->emit = emit;
    x->user = user;
    x->open_ms = rnet_os_monotonic_ms();
    x->agent = rnet_ice_agent_create(ice, xfer_emit_bridge, x);
    if (!x->agent) {
        free(x);
        return -1;
    }
    if (rnet_ice_agent_start_gathering(x->agent) != 0) {
        rnet_ice_agent_destroy(x->agent);
        free(x);
        return -1;
    }
    *out = x;
    return 0;
}

void rnet_ice_xfer_close(RNetIceXfer **xfer)
{
    int i;
    RNetIceXfer *x;
    if (!xfer || !*xfer)
        return;
    x = *xfer;
    *xfer = NULL;
    rnet_ice_agent_destroy(x->agent);
    for (i = 0; i < x->send_n; ++i)
        free_blob(&x->send_q[i]);
    for (i = 0; i < x->ready_n; ++i)
        free(x->ready[i].data);
    free(x->recv_buf);
    free(x);
}

void rnet_ice_xfer_push_signal(RNetIceXfer *xfer, const RNetSignal *msg)
{
    if (!xfer || !xfer->agent || !msg)
        return;
    rnet_ice_agent_push_signal(xfer->agent, msg);
}

void rnet_ice_xfer_pump(RNetIceXfer *xfer)
{
    uint8_t buf[XFER_RECV_MAX];
    size_t n = 0;
    RNetIceState st;
    rnet_u64 now;
    XferBlob *b;
    if (!xfer || !xfer->agent || xfer->failed)
        return;
    rnet_ice_agent_poll(xfer->agent);
    st = rnet_ice_agent_state(xfer->agent);
    now = rnet_os_monotonic_ms();
    if (st == RNET_ICE_STATE_FAILED) {
        fail_xfer(xfer, "ICE failed");
        return;
    }
    if (st != RNET_ICE_STATE_CONNECTED && st != RNET_ICE_STATE_COMPLETED) {
        if (now - xfer->open_ms >= XFER_CONNECT_MS)
            fail_xfer(xfer, "ICE connect timed out");
        return;
    }
    while (rnet_ice_agent_recv(xfer->agent, buf, sizeof(buf), &n) == 0 && n > 0)
        handle_pkt(xfer, buf, n);
    if (xfer->failed || xfer->send_n <= 0)
        return;
    b = &xfer->send_q[0];
    if (b->unacked >= b->len)
        return;
    if (b->next_off > b->unacked + (uint64_t)XFER_WINDOW * XFER_PAYLOAD)
        b->next_off = b->unacked;
    if (now - b->last_send_ms >= XFER_RETRY_MS && b->next_off > b->unacked)
        b->next_off = b->unacked;
    while (b->next_off < b->len &&
           b->next_off < b->unacked + (uint64_t)XFER_WINDOW * XFER_PAYLOAD) {
        send_data(xfer, b, b->next_off);
        {
            uint64_t nsend = b->len - b->next_off;
            if (nsend > XFER_PAYLOAD)
                nsend = XFER_PAYLOAD;
            b->next_off += nsend;
        }
        b->last_send_ms = now;
        if (b->len == 0)
            break;
    }
    if (b->len == 0 && b->unacked == 0 &&
        (b->last_send_ms == 0 || now - b->last_send_ms >= XFER_RETRY_MS)) {
        send_data(xfer, b, 0);
        b->last_send_ms = now;
    }
}

RNetIceState rnet_ice_xfer_state(const RNetIceXfer *xfer)
{
    if (!xfer || !xfer->agent)
        return RNET_ICE_STATE_IDLE;
    return rnet_ice_agent_state(xfer->agent);
}

int rnet_ice_xfer_queue_blob(RNetIceXfer *xfer, uint8_t *data, size_t len)
{
    XferBlob *b;
    if (!xfer || xfer->failed)
        return -1;
    if (len > 0 && !data)
        return -1;
    if (xfer->send_n >= XFER_Q) {
        free(data);
        return -1;
    }
    b = &xfer->send_q[xfer->send_n++];
    memset(b, 0, sizeof(*b));
    b->data = data;
    b->len = len;
    b->id = ++xfer->next_id;
    return 0;
}

int rnet_ice_xfer_send_idle(const RNetIceXfer *xfer)
{
    return (!xfer || xfer->send_n <= 0) ? 1 : 0;
}

int rnet_ice_xfer_take_blob(RNetIceXfer *xfer, uint8_t **data, size_t *len)
{
    int i;
    if (!xfer || !data || !len || xfer->ready_n <= 0)
        return 0;
    *data = xfer->ready[0].data;
    *len = xfer->ready[0].len;
    for (i = 1; i < xfer->ready_n; ++i)
        xfer->ready[i - 1] = xfer->ready[i];
    xfer->ready_n--;
    memset(&xfer->ready[xfer->ready_n], 0, sizeof(xfer->ready[0]));
    return 1;
}

int rnet_ice_xfer_progress(const RNetIceXfer *xfer)
{
    if (!xfer)
        return -1;
    if (xfer->failed)
        return -2;
    if (xfer->recv_active && xfer->recv_total > 0)
        return (int)((xfer->recv_got * 100ull) / xfer->recv_total);
    if (xfer->send_n > 0 && xfer->send_q[0].len > 0)
        return (int)((xfer->send_q[0].unacked * 100ull) / xfer->send_q[0].len);
    if (xfer->send_n > 0 || xfer->ready_n > 0)
        return 0;
    return -1;
}

int rnet_ice_xfer_failed(const RNetIceXfer *xfer, char *err, size_t err_cap)
{
    if (!xfer || !xfer->failed)
        return 0;
    if (err && err_cap) {
        snprintf(err, err_cap, "%s", xfer->error);
    }
    return 1;
}

void rnet_ice_xfer_fail(RNetIceXfer *xfer, const char *err)
{
    if (!xfer)
        return;
    fail_xfer(xfer, err);
    send_fail_pkt(xfer, xfer->error);
}

#else /* !RNET_ENABLE_ICE */

int rnet_ice_xfer_open(RNetIceXfer **out, const RNetIceConfig *ice,
                       RNetIceXferSignalEmitFn emit, void *user)
{
    (void)ice;
    (void)emit;
    (void)user;
    if (out)
        *out = NULL;
    return -1;
}

void rnet_ice_xfer_close(RNetIceXfer **xfer)
{
    if (xfer)
        *xfer = NULL;
}

void rnet_ice_xfer_push_signal(RNetIceXfer *xfer, const RNetSignal *msg)
{
    (void)xfer;
    (void)msg;
}

void rnet_ice_xfer_pump(RNetIceXfer *xfer)
{
    (void)xfer;
}

RNetIceState rnet_ice_xfer_state(const RNetIceXfer *xfer)
{
    (void)xfer;
    return RNET_ICE_STATE_IDLE;
}

int rnet_ice_xfer_queue_blob(RNetIceXfer *xfer, uint8_t *data, size_t len)
{
    (void)xfer;
    (void)len;
    free(data);
    return -1;
}

int rnet_ice_xfer_send_idle(const RNetIceXfer *xfer)
{
    (void)xfer;
    return 1;
}

int rnet_ice_xfer_take_blob(RNetIceXfer *xfer, uint8_t **data, size_t *len)
{
    (void)xfer;
    (void)data;
    (void)len;
    return 0;
}

int rnet_ice_xfer_progress(const RNetIceXfer *xfer)
{
    (void)xfer;
    return -1;
}

int rnet_ice_xfer_failed(const RNetIceXfer *xfer, char *err, size_t err_cap)
{
    (void)xfer;
    (void)err;
    (void)err_cap;
    return 0;
}

void rnet_ice_xfer_fail(RNetIceXfer *xfer, const char *err)
{
    (void)xfer;
    (void)err;
}

#endif /* RNET_ENABLE_ICE */
