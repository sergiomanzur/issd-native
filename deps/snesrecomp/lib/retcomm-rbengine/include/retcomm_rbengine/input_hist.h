#ifndef RETCOMM_RBENGINE_INPUT_HIST_H
#define RETCOMM_RBENGINE_INPUT_HIST_H

/*
 * Per-slot tick → RNetRbFrame history for rollback invent / contract.
 *
 * Local authority rows: is_predicted = 0.
 * Invent (hold-last) remotes: is_predicted = 1.
 * Late wire promote: replace the row in place (clears predicted).
 *
 * Pad layout conversion stays in the engine; this module speaks RNetRbFrame.
 */

#include <stdint.h>

#include "recomp_net/input_contract.h"
#include "recomp_net/rollback.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RBE_INPUT_HIST_DEPTH 128u
#define RBE_INPUT_HIST_MAX_SLOTS 8

typedef struct RbeInputHist {
    RNetRbFrame rows[RBE_INPUT_HIST_MAX_SLOTS][RBE_INPUT_HIST_DEPTH];
    int         slot_count;
    uint32_t    invent_count;
    uint32_t    promote_count;
    uint32_t    rewind_count;
} RbeInputHist;

void rbe_ih_reset(RbeInputHist *h, int slot_count);

void rbe_ih_frame_to_contract(const RNetRbFrame *frame, RNetInputContractFrame *out);

int rbe_ih_put(RbeInputHist *h, int slot, const RNetRbFrame *frame);
int rbe_ih_get(const RbeInputHist *h, int slot, uint32_t tick, RNetRbFrame *out);

/* Hold-last invent for missing remote at tick. Uses prior valid row for slot,
 * else neutral (buttons 0xFFFF, sticks 0). Marks is_predicted=1 and stores. */
int rbe_ih_invent_hold_last(RbeInputHist *h, int slot, uint32_t tick,
                            RNetRbFrame *out);

/* Neutral invent (buttons 0xFFFF). Seal gap-fill only — live MotK admit uses
 * hold-last so a held D-pad does not re-episode every tick. */
int rbe_ih_invent_idle(RbeInputHist *h, int slot, uint32_t tick, RNetRbFrame *out);

/* Replace a published predicted/auth row with authoritative wire (promote). */
int rbe_ih_promote(RbeInputHist *h, int slot, const RNetRbFrame *wire);

#ifdef __cplusplus
}
#endif

#endif /* RETCOMM_RBENGINE_INPUT_HIST_H */
