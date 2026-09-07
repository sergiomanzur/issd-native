#include "rnet_rings.h"

#include <string.h>

void rnet_ring_clear(RNetInputRing *ring)
{
    if (ring == NULL)
    {
        return;
    }
    memset(ring, 0, sizeof(*ring));
}

void rnet_ring_store(RNetInputRing *ring, const RNetInputSample *sample)
{
    RNetInputSample *dst;
    if ((ring == NULL) || (sample == NULL) || (sample->valid == 0))
    {
        return;
    }
    dst = &ring->slots[sample->tick % RNET_HISTORY_LENGTH];
    *dst = *sample;
}

int rnet_ring_get(const RNetInputRing *ring, rnet_u32 tick, RNetInputSample *out)
{
    const RNetInputSample *src;
    if ((ring == NULL) || (out == NULL))
    {
        return 0;
    }
    src = &ring->slots[tick % RNET_HISTORY_LENGTH];
    if ((src->valid == 0) || (src->tick != tick))
    {
        return 0;
    }
    *out = *src;
    return 1;
}

rnet_u32 rnet_ring_highest_valid(const RNetInputRing *ring)
{
    rnet_u32 best = 0;
    int found = 0;
    size_t i;
    if (ring == NULL)
    {
        return 0;
    }
    for (i = 0; i < RNET_HISTORY_LENGTH; ++i)
    {
        if (ring->slots[i].valid)
        {
            if (!found || ring->slots[i].tick > best)
            {
                best = ring->slots[i].tick;
                found = 1;
            }
        }
    }
    return found ? best : 0;
}
