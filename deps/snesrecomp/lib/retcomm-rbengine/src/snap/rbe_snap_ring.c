#include "retcomm_rbengine/snap_ring.h"

#include <stdlib.h>
#include <string.h>

typedef struct RbeSnapSlot {
    int      valid;
    uint32_t tick;
    uint8_t *data;
    size_t   size;
} RbeSnapSlot;

struct RbeSnapRing {
    RbeSnapSlot *slots;
    uint32_t     depth;
    uint32_t     count;
    uint32_t     head;
};

static void slot_clear(RbeSnapSlot *s)
{
    free(s->data);
    s->data = NULL;
    s->size = 0;
    s->tick = 0;
    s->valid = 0;
}

static int find_slot(const RbeSnapRing *r, uint32_t tick)
{
    uint32_t i;
    if (!r)
        return -1;
    for (i = 0; i < r->depth; i++) {
        if (r->slots[i].valid && r->slots[i].tick == tick)
            return (int)i;
    }
    return -1;
}

RbeSnapRing *rbe_snap_ring_create(uint32_t depth)
{
    RbeSnapRing *r;
    if (depth == 0)
        depth = RBE_SNAP_RING_DEFAULT_DEPTH;
    if (depth > 512u)
        depth = 512u;
    r = (RbeSnapRing *)calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->slots = (RbeSnapSlot *)calloc(depth, sizeof(RbeSnapSlot));
    if (!r->slots) {
        free(r);
        return NULL;
    }
    r->depth = depth;
    return r;
}

void rbe_snap_ring_destroy(RbeSnapRing *r)
{
    if (!r)
        return;
    rbe_snap_ring_clear(r);
    free(r->slots);
    free(r);
}

void rbe_snap_ring_clear(RbeSnapRing *r)
{
    uint32_t i;
    if (!r)
        return;
    for (i = 0; i < r->depth; i++)
        slot_clear(&r->slots[i]);
    r->count = 0;
    r->head = 0;
}

uint32_t rbe_snap_ring_depth(const RbeSnapRing *r)
{
    return r ? r->depth : 0u;
}

uint32_t rbe_snap_ring_count(const RbeSnapRing *r)
{
    return r ? r->count : 0u;
}

int rbe_snap_ring_has(const RbeSnapRing *r, uint32_t tick)
{
    return find_slot(r, tick) >= 0;
}

int rbe_snap_ring_store(RbeSnapRing *r, uint32_t tick, uint8_t *data, size_t size)
{
    int existing;
    RbeSnapSlot *s;
    if (!r || !data || !size)
        return 0;

    existing = find_slot(r, tick);
    if (existing >= 0) {
        s = &r->slots[existing];
        free(s->data);
        s->data = data;
        s->size = size;
        s->tick = tick;
        s->valid = 1;
        return 1;
    }

    if (r->count < r->depth) {
        uint32_t i;
        for (i = 0; i < r->depth; i++) {
            uint32_t idx = (r->head + i) % r->depth;
            if (!r->slots[idx].valid) {
                s = &r->slots[idx];
                s->data = data;
                s->size = size;
                s->tick = tick;
                s->valid = 1;
                r->count++;
                r->head = (idx + 1u) % r->depth;
                return 1;
            }
        }
    }

    {
        uint32_t i;
        int victim = -1;
        uint32_t oldest = 0xffffffffu;
        for (i = 0; i < r->depth; i++) {
            if (!r->slots[i].valid)
                continue;
            if (r->slots[i].tick <= oldest) {
                oldest = r->slots[i].tick;
                victim = (int)i;
            }
        }
        if (victim < 0) {
            free(data);
            return 0;
        }
        s = &r->slots[victim];
        free(s->data);
        s->data = data;
        s->size = size;
        s->tick = tick;
        s->valid = 1;
        r->head = ((uint32_t)victim + 1u) % r->depth;
        return 1;
    }
}

const uint8_t *rbe_snap_ring_peek(const RbeSnapRing *r, uint32_t tick, size_t *size_out)
{
    int idx = find_slot(r, tick);
    if (idx < 0) {
        if (size_out)
            *size_out = 0;
        return NULL;
    }
    if (size_out)
        *size_out = r->slots[idx].size;
    return r->slots[idx].data;
}

int rbe_snap_ring_save(RbeSnapRing *r, uint32_t tick, const RbeSnapVTable *vt)
{
    uint8_t *data = NULL;
    size_t len = 0;
    if (!r || !vt || !vt->serialize)
        return 0;
    if (!vt->serialize(vt->ctx, tick, &data, &len) || !data || !len)
        return 0;
    if (!rbe_snap_ring_store(r, tick, data, len))
        return 0;
    return 1;
}

int rbe_snap_ring_load(RbeSnapRing *r, uint32_t tick, const RbeSnapVTable *vt)
{
    size_t size = 0;
    const uint8_t *data;
    if (!r || !vt || !vt->deserialize)
        return 0;
    data = rbe_snap_ring_peek(r, tick, &size);
    if (!data || !size)
        return 0;
    return vt->deserialize(vt->ctx, tick, data, size);
}

int rbe_snap_ring_drop_tick(RbeSnapRing *r, uint32_t tick)
{
    int idx = find_slot(r, tick);
    if (idx < 0)
        return 0;
    slot_clear(&r->slots[idx]);
    if (r->count)
        r->count--;
    return 1;
}

uint32_t rbe_snap_ring_drop_after(RbeSnapRing *r, uint32_t tick)
{
    uint32_t i, n = 0;
    if (!r)
        return 0;
    for (i = 0; i < r->depth; i++) {
        if (!r->slots[i].valid)
            continue;
        if (r->slots[i].tick > tick) {
            slot_clear(&r->slots[i]);
            if (r->count)
                r->count--;
            n++;
        }
    }
    return n;
}

uint32_t rbe_snap_ring_oldest_tick(const RbeSnapRing *r)
{
    uint32_t i, oldest = 0xffffffffu;
    int any = 0;
    if (!r)
        return 0;
    for (i = 0; i < r->depth; i++) {
        if (!r->slots[i].valid)
            continue;
        if (!any || r->slots[i].tick < oldest) {
            oldest = r->slots[i].tick;
            any = 1;
        }
    }
    return any ? oldest : 0u;
}

uint32_t rbe_snap_ring_newest_tick(const RbeSnapRing *r)
{
    uint32_t i, newest = 0;
    int any = 0;
    if (!r)
        return 0;
    for (i = 0; i < r->depth; i++) {
        if (!r->slots[i].valid)
            continue;
        if (!any || r->slots[i].tick > newest) {
            newest = r->slots[i].tick;
            any = 1;
        }
    }
    return any ? newest : 0u;
}
