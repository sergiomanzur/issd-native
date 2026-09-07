#include "retcomm_rbengine/snap_ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL: %s\n", msg);                                         \
            failures++;                                                        \
        } else {                                                               \
            printf("ok:   %s\n", msg);                                         \
        }                                                                      \
    } while (0)

static uint8_t *mkblob(uint32_t tick, size_t *out_len)
{
    uint8_t *p = (uint8_t *)malloc(8);
    if (!p)
        return NULL;
    memcpy(p, &tick, 4);
    memcpy(p + 4, "SNAP", 4);
    *out_len = 8;
    return p;
}

static int stub_serialize(void *ctx, uint32_t tick, uint8_t **out, size_t *len)
{
    (void)ctx;
    *out = mkblob(tick, len);
    return *out != NULL;
}

static int stub_deserialize(void *ctx, uint32_t tick, const uint8_t *data, size_t len)
{
    uint32_t t = 0;
    (void)ctx;
    if (!data || len != 8)
        return 0;
    memcpy(&t, data, 4);
    return t == tick && memcmp(data + 4, "SNAP", 4) == 0;
}

int main(void)
{
    RbeSnapRing *r = rbe_snap_ring_create(4);
    size_t n = 0;
    const uint8_t *peek;
    uint32_t t;
    uint8_t *b;
    size_t blen;
    RbeSnapVTable vt;

    CHECK(r != NULL, "create");
    CHECK(rbe_snap_ring_depth(r) == 4u, "depth");

    for (t = 10; t < 14; t++) {
        b = mkblob(t, &blen);
        CHECK(b && rbe_snap_ring_store(r, t, b, blen), "store");
    }
    CHECK(rbe_snap_ring_count(r) == 4u, "full");
    CHECK(rbe_snap_ring_oldest_tick(r) == 10u, "oldest");
    CHECK(rbe_snap_ring_newest_tick(r) == 13u, "newest");

    peek = rbe_snap_ring_peek(r, 12, &n);
    CHECK(peek && n == 8 && memcmp(peek, "\x0c\x00\x00\x00SNAP", 8) == 0, "peek");

    b = mkblob(14, &blen);
    CHECK(b && rbe_snap_ring_store(r, 14, b, blen), "evict store");
    CHECK(!rbe_snap_ring_has(r, 10), "oldest evicted");
    CHECK(rbe_snap_ring_oldest_tick(r) == 11u, "new oldest");

    memset(&vt, 0, sizeof(vt));
    vt.serialize = stub_serialize;
    vt.deserialize = stub_deserialize;
    CHECK(rbe_snap_ring_save(r, 100, &vt), "save vtable");
    CHECK(rbe_snap_ring_load(r, 100, &vt), "load vtable");

    CHECK(rbe_snap_ring_drop_after(r, 12) >= 1u, "drop_after");
    rbe_snap_ring_clear(r);
    CHECK(rbe_snap_ring_count(r) == 0u, "clear");
    rbe_snap_ring_destroy(r);

    r = rbe_snap_ring_create(0);
    CHECK(r != NULL && rbe_snap_ring_depth(r) == RBE_SNAP_RING_DEFAULT_DEPTH,
          "create(0) uses default depth");
    rbe_snap_ring_destroy(r);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
