#ifndef RECOMP_NET_CONFIG_H
#define RECOMP_NET_CONFIG_H

#include "recomp_net/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RNET_MAX_SLOTS 8
#define RNET_INPUT_MAX 32
#define RNET_HISTORY_LENGTH 128
#define RNET_DEFAULT_BUNDLE_REDUNDANCY 3

typedef struct RNetConfig
{
    /* Number of player slots in the session (2..RNET_MAX_SLOTS). */
    rnet_u8 slot_count;
    /* Local player slot index (0..slot_count-1). */
    rnet_u8 local_slot;
    /* Fixed input delay D in sim ticks (wire_tick = sim_tick + D). */
    rnet_u8 input_delay;
    /* How many prior INPUT frames to retransmit per packet. */
    rnet_u8 bundle_redundancy;
    /* Opaque session id negotiated out-of-band (must match peers). */
    rnet_u32 session_id;
    /* Host-defined protocol magic (default 0x524E4554 "RNET"). */
    rnet_u32 protocol_magic;
    /* Bit i set = lobby seat i is occupied by a real peer. 0 = all seats in
     * [0, slot_count) occupied (legacy). Sparse rooms (e.g. seats 0+2 in a
     * 4-max lobby) must clear empty bits so READY/admit do not wait forever
     * on a phantom remote. Empty seats use deterministic neutral samples. */
    rnet_u32 occupied_mask;
} RNetConfig;

static inline void rnet_config_init_defaults(RNetConfig *cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    cfg->slot_count = 2;
    cfg->local_slot = 0;
    cfg->input_delay = 2;
    cfg->bundle_redundancy = RNET_DEFAULT_BUNDLE_REDUNDANCY;
    cfg->session_id = 1;
    cfg->protocol_magic = 0x524E4554u; /* 'RNET' */
    cfg->occupied_mask = 0; /* all occupied */
}

/* 1 if seat is a real peer (or mask unset ⇒ every seat in range). */
static inline int rnet_config_slot_occupied(const RNetConfig *cfg, rnet_u8 slot)
{
    rnet_u32 mask;
    if (cfg == NULL || slot >= RNET_MAX_SLOTS || slot >= cfg->slot_count)
        return 0;
    mask = cfg->occupied_mask;
    if (mask == 0u)
        mask = (cfg->slot_count >= 32) ? 0xffffffffu
                                       : ((1u << cfg->slot_count) - 1u);
    return (mask & (1u << slot)) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_CONFIG_H */
