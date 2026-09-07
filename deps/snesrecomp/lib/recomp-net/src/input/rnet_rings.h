#ifndef RNET_RINGS_H
#define RNET_RINGS_H

#include "recomp_net/input.h"
#include "recomp_net/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RNetInputRing
{
    RNetInputSample slots[RNET_HISTORY_LENGTH];
} RNetInputRing;

void rnet_ring_clear(RNetInputRing *ring);
void rnet_ring_store(RNetInputRing *ring, const RNetInputSample *sample);
int rnet_ring_get(const RNetInputRing *ring, rnet_u32 tick, RNetInputSample *out);
rnet_u32 rnet_ring_highest_valid(const RNetInputRing *ring);

#ifdef __cplusplus
}
#endif

#endif /* RNET_RINGS_H */
