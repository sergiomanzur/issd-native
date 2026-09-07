#ifndef RETCOMM_RBENGINE_HASH_CONFIRM_H
#define RETCOMM_RBENGINE_HASH_CONFIRM_H

/*
 * Local digest ring + peer FRAME_COMMIT matching → resolved_through watermark.
 *
 * rbe_hc_confirm_through(T) is 1 iff every tick in (prev_resolved, T] has a
 * local digest that matched a peer FRAME_COMMIT (contiguous from the prior
 * watermark). Bind to RNetInputContractHostGates.hash_confirm_promote /
 * RNetRollbackVTable.hash_confirm_through.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RBE_HC_RING 128u

typedef struct RbeHashConfirm {
    uint32_t local_tick[RBE_HC_RING];
    uint32_t local_digest[RBE_HC_RING];
    uint8_t  local_valid[RBE_HC_RING];
    uint32_t peer_tick[RBE_HC_RING];
    uint32_t peer_digest[RBE_HC_RING];
    uint8_t  peer_valid[RBE_HC_RING];
    uint32_t resolved_through; /* inclusive; 0 = none yet (tick 0 may match) */
    uint8_t  resolved_valid;   /* 0 until first match advances watermark */
} RbeHashConfirm;

void rbe_hc_reset(RbeHashConfirm *hc);

/* Clear the ring and set resolved_through = last_ok so the next compared tick
 * is last_ok+1. Used at Replay entry to drop live invent FRAME_COMMITs. */
void rbe_hc_prime_after(RbeHashConfirm *hc, uint32_t last_ok);

void rbe_hc_note_local(RbeHashConfirm *hc, uint32_t tick, uint32_t digest);
void rbe_hc_note_peer(RbeHashConfirm *hc, uint32_t tick, uint32_t digest);

uint32_t rbe_hc_resolved_through(const RbeHashConfirm *hc);
uint8_t  rbe_hc_confirm_through(const RbeHashConfirm *hc, uint32_t tick);

uint8_t rbe_hc_local_digest(const RbeHashConfirm *hc, uint32_t tick,
                            uint32_t *digest_out);
uint8_t rbe_hc_peer_digest(const RbeHashConfirm *hc, uint32_t tick,
                           uint32_t *digest_out);

uint8_t rbe_hc_peek_mismatch(const RbeHashConfirm *hc, uint32_t *tick_out,
                             uint32_t *local_out, uint32_t *peer_out);

/* Heal a stuck watermark when the next tick aged out of the ring and is no
 * longer a live mismatch. Returns 1 if the watermark moved. */
uint8_t rbe_hc_heal_stale_gap(RbeHashConfirm *hc);

#ifdef __cplusplus
}
#endif

#endif /* RETCOMM_RBENGINE_HASH_CONFIRM_H */
