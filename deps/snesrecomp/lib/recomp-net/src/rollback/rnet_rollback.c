/*
 * Shared rollback episode orchestration — game-agnostic core.
 *
 * The library owns the FSM, correction tuple, sealed input table, and the
 * resolved-through watermark. The host owns snapshots, the sim step, digests,
 * and transport. See include/recomp_net/rollback.h and docs/rollback.md.
 *
 * Modelled on BattleShip's netrollback_episode FSM; SSB-specific gates, span
 * digests over subsystem hashes, and packet opcodes stay host-side.
 */

#include "recomp_net/rollback.h"

#include <stdlib.h>
#include <string.h>

#define RNET_RB_EVENT_QUEUE_MAX 8u

struct RNetRbSession
{
    RNetRbConfig cfg;
    RNetRollbackVTable vt;

    RNetRbPhase phase;
    RNetRbCorrection corr;

    /* Sealed input table: [span][slot]. Span indexes offset from seal_base_tick
     * (usually load_tick so Replay can publish every resim tick). */
    RNetRbFrame *sealed;      /* sealed_max_span * RNET_RB_MAX_SLOTS */
    uint32_t sealed_span;
    uint32_t seal_base_tick;
    uint8_t inputs_sealed;

    /* Per-slot peer-seal completion bitmask over the sealed span. */
    uint64_t peer_seal_mask[RNET_RB_MAX_SLOTS];

    uint32_t resolved_through; /* shared frontier watermark */

    RNetRbEvent events[RNET_RB_EVENT_QUEUE_MAX];
    uint32_t event_head;
    uint32_t event_tail;
};

static size_t rnet_rb_seal_index(uint32_t offset, uint32_t slot)
{
    return (size_t)offset * RNET_RB_MAX_SLOTS + (size_t)slot;
}

static uint8_t rnet_rb_slot_valid(int32_t slot)
{
    return ((slot >= 0) && ((uint32_t)slot < RNET_RB_MAX_SLOTS)) ? 1u : 0u;
}

RNetRbSession *rnet_rb_create(const RNetRbConfig *cfg, const RNetRollbackVTable *vt)
{
    RNetRbSession *s;

    if ((cfg == NULL) || (vt == NULL) || (vt->load_state == NULL) ||
        (vt->advance_sim == NULL) || (vt->get_input_row == NULL))
    {
        return NULL;
    }
    s = (RNetRbSession *)calloc(1u, sizeof(*s));
    if (s == NULL)
    {
        return NULL;
    }
    s->cfg = *cfg;
    s->vt = *vt;
    if ((s->cfg.seal_max_span == 0u) || (s->cfg.seal_max_span > RNET_RB_SEAL_MAX_SPAN))
    {
        s->cfg.seal_max_span = RNET_RB_SEAL_MAX_SPAN;
    }
    if (s->cfg.slot_count == 0u)
    {
        s->cfg.slot_count = 2u; /* delay-sync default: P2 */
    }
    if (s->cfg.slot_count > RNET_RB_MAX_SLOTS)
    {
        s->cfg.slot_count = RNET_RB_MAX_SLOTS;
    }
    if (s->cfg.local_slot >= s->cfg.slot_count)
    {
        free(s);
        return NULL;
    }
    if (s->cfg.tip_runway > 32u)
    {
        s->cfg.tip_runway = 32u;
    }
    /* tip_seal_slack: 0 with tip_runway → default slack; UINT32_MAX → force 0. */
    if (s->cfg.tip_seal_slack == 0u && s->cfg.tip_runway > 0u)
    {
        s->cfg.tip_seal_slack = RNET_RB_TIP_SEAL_SLACK_DEFAULT;
    }
    else if (s->cfg.tip_seal_slack == 0xffffffffu)
    {
        s->cfg.tip_seal_slack = 0u;
    }
    if (s->cfg.tip_seal_slack > 8u)
    {
        s->cfg.tip_seal_slack = 8u;
    }
    if (s->cfg.light_tip_max_depth == 0u)
    {
        s->cfg.light_tip_max_depth = RNET_RB_LIGHT_TIP_MAX_DEPTH;
    }
    if (s->cfg.light_tip_max_depth > 32u)
    {
        s->cfg.light_tip_max_depth = 32u;
    }
    s->sealed = (RNetRbFrame *)calloc((size_t)s->cfg.seal_max_span * RNET_RB_MAX_SLOTS,
                                      sizeof(RNetRbFrame));
    if (s->sealed == NULL)
    {
        free(s);
        return NULL;
    }
    rnet_rb_session_reset(s);
    return s;
}

void rnet_rb_destroy(RNetRbSession *s)
{
    if (s == NULL)
    {
        return;
    }
    free(s->sealed);
    free(s);
}

void rnet_rb_session_reset(RNetRbSession *s)
{
    uint32_t keep_resolved;
    if (s == NULL)
    {
        return;
    }
    /* resolved_through is a session watermark across episodes — keep it so
     * light-tip / tip-extend policy still sees the shared frontier. */
    keep_resolved = s->resolved_through;
    s->phase = nRNetRbPhaseLive;
    memset(&s->corr, 0, sizeof(s->corr));
    s->corr.slot = -1;
    s->sealed_span = 0u;
    s->seal_base_tick = 0u;
    s->inputs_sealed = 0u;
    memset(s->peer_seal_mask, 0, sizeof(s->peer_seal_mask));
    s->resolved_through = keep_resolved;
    s->event_head = 0u;
    s->event_tail = 0u;
}

RNetRbPhase rnet_rb_get_phase(const RNetRbSession *s)
{
    return (s != NULL) ? s->phase : nRNetRbPhaseLive;
}

uint8_t rnet_rb_is_active(const RNetRbSession *s)
{
    return ((s != NULL) && (s->phase != nRNetRbPhaseLive)) ? 1u : 0u;
}

uint8_t rnet_rb_is_resimulating(const RNetRbSession *s)
{
    if (s == NULL)
    {
        return 0u;
    }
    return ((s->phase == nRNetRbPhaseAwaitingBaseline) || (s->phase == nRNetRbPhaseReplay) ||
            (s->phase == nRNetRbPhaseVerify))
               ? 1u
               : 0u;
}

uint8_t rnet_rb_is_tip_holding(const RNetRbSession *s)
{
    return ((s != NULL) && (s->phase == nRNetRbPhaseTipHold)) ? 1u : 0u;
}

uint32_t rnet_rb_get_tip_runway(const RNetRbSession *s)
{
    return (s != NULL) ? s->cfg.tip_runway : 0u;
}

uint32_t rnet_rb_get_tip_seal_slack(const RNetRbSession *s)
{
    return (s != NULL) ? s->cfg.tip_seal_slack : 0u;
}

uint32_t rnet_rb_get_light_tip_max_depth(const RNetRbSession *s)
{
    return (s != NULL) ? s->cfg.light_tip_max_depth : RNET_RB_LIGHT_TIP_MAX_DEPTH;
}

uint32_t rnet_rb_get_epoch_id(const RNetRbSession *s) { return (s != NULL) ? s->corr.epoch_id : 0u; }
uint32_t rnet_rb_get_mismatch_tick(const RNetRbSession *s) { return (s != NULL) ? s->corr.mismatch_tick : 0u; }
uint32_t rnet_rb_get_load_tick(const RNetRbSession *s) { return (s != NULL) ? s->corr.load_tick : 0u; }
uint32_t rnet_rb_get_target_tick(const RNetRbSession *s) { return (s != NULL) ? s->corr.target_tick : 0u; }
int32_t rnet_rb_get_corrected_slot(const RNetRbSession *s) { return (s != NULL) ? s->corr.slot : -1; }
uint8_t rnet_rb_is_from_peer_notify(const RNetRbSession *s)
{
    return ((s != NULL) && (s->corr.from_peer_notify != 0u)) ? 1u : 0u;
}
uint8_t rnet_rb_get_corr_flags(const RNetRbSession *s)
{
    return (s != NULL) ? s->corr.flags : 0u;
}

uint8_t rnet_rb_is_light_tip_candidate_ex(uint32_t load_tick, uint32_t target_tick,
                                          uint32_t resolved_through, uint32_t max_depth)
{
    uint32_t depth;
    if (target_tick < load_tick)
    {
        return 0u;
    }
    depth = target_tick - load_tick;
    if (depth > max_depth)
    {
        return 0u;
    }
    /* Tip-aligned: load is at or after the last agreed frontier (or first
     * episode with no frontier yet and depth still tiny). */
    if (resolved_through == 0u)
    {
        return (depth <= 2u) ? 1u : 0u;
    }
    return (load_tick >= resolved_through) ? 1u : 0u;
}

uint8_t rnet_rb_is_light_tip_candidate(uint32_t load_tick, uint32_t target_tick,
                                       uint32_t resolved_through)
{
    return rnet_rb_is_light_tip_candidate_ex(load_tick, target_tick, resolved_through,
                                             RNET_RB_LIGHT_TIP_MAX_DEPTH);
}

uint8_t rnet_rb_recommend_light_tip(const RNetRbSession *s)
{
    if (s == NULL)
    {
        return 0u;
    }
    /* Post-begin the classification lives in corr.flags (initiator-decided,
     * wire-propagated). Never re-derive from the local resolved_through here:
     * peers' watermarks can differ, and a per-peer re-derivation makes the
     * baseline burst counts asymmetric. */
    return ((s->corr.flags & RNET_RB_CORR_LIGHT_TIP) != 0u) ? 1u : 0u;
}

uint32_t rnet_rb_suggest_target(const RNetRbSession *s, uint32_t mismatch_tick,
                                uint32_t sim_tip)
{
    uint32_t target;
    uint32_t slack;
    target = (sim_tip > mismatch_tick) ? sim_tip : mismatch_tick;
    slack = (s != NULL) ? s->cfg.tip_seal_slack : 0u;
    if (slack > 0u && target <= (0xffffffffu - slack))
    {
        target += slack;
    }
    return target;
}

void rnet_rb_begin_episode(RNetRbSession *s, const RNetRbCorrection *corr)
{
    if ((s == NULL) || (corr == NULL))
    {
        return;
    }
    s->corr = *corr;
    /* Light-tip is initiator-authoritative: followers (from_peer_notify) take
     * the wire flags verbatim so both peers classify the episode identically
     * even when their local resolved_through watermarks differ. */
    if (corr->from_peer_notify == 0u &&
        rnet_rb_is_light_tip_candidate_ex(s->corr.load_tick, s->corr.target_tick,
                                          s->resolved_through,
                                          s->cfg.light_tip_max_depth) != 0u)
    {
        s->corr.flags = (uint8_t)(s->corr.flags | RNET_RB_CORR_LIGHT_TIP);
    }
    s->inputs_sealed = 0u;
    s->sealed_span = 0u;
    memset(s->peer_seal_mask, 0, sizeof(s->peer_seal_mask));
    s->phase = nRNetRbPhaseSealInputs;
}

static void rnet_rb_fill_local_row(RNetRbSession *s, uint32_t tick, uint32_t offset,
                                   uint32_t slot)
{
    RNetRbFrame *dst = &s->sealed[rnet_rb_seal_index(offset, slot)];
    memset(dst, 0, sizeof(*dst));
    dst->tick = tick;
    if (slot == s->cfg.local_slot)
    {
        RNetRbFrame row;
        if (s->vt.get_input_row(s->vt.ctx, (int32_t)slot, tick, &row) != 0u)
        {
            *dst = row;
            dst->tick = tick;
        }
    }
    else
    {
        /* Remote seat: pre-seal from host history ONLY when the row is
         * wire-confirmed (!is_predicted) — that is the owner's transmitted
         * pad, byte-identical to what the owner seals locally, so it is
         * authoritative without waiting for the peer's SEAL_ROWS. Predicted
         * rows stay unsealed: 2026-08-02 soak forked when a tip-extend left
         * the remote seat zeroed, the arm fell back to live hist
         * (s1=---- at arm 4179) and the peers simmed different pads. */
        RNetRbFrame row;
        if (s->vt.get_input_row(s->vt.ctx, (int32_t)slot, tick, &row) != 0u &&
            row.is_valid != 0u && row.is_predicted == 0u)
        {
            *dst = row;
            dst->tick = tick;
            if (offset < 64u)
            {
                s->peer_seal_mask[slot] |= (1ull << offset);
            }
        }
    }
}

uint8_t rnet_rb_can_extend_target(const RNetRbSession *s, uint32_t new_target)
{
    uint32_t old_target;
    uint32_t begin;
    uint32_t new_span;

    if ((s == NULL) || (s->inputs_sealed == 0u))
    {
        return 0u;
    }
    if ((s->phase == nRNetRbPhaseLive) || (s->phase == nRNetRbPhaseCommit) ||
        (s->phase == nRNetRbPhaseAbort))
    {
        return 0u;
    }
    old_target = s->corr.target_tick;
    if (new_target <= old_target)
    {
        return 1u; /* no-op / Verify|TipHold→Replay */
    }
    begin = s->seal_base_tick;
    if (new_target < begin)
    {
        return 0u;
    }
    new_span = new_target - begin + 1u;
    if (new_span > s->cfg.seal_max_span)
    {
        return 0u;
    }
    if (new_span > RNET_RB_PEER_SEAL_MASK_BITS)
    {
        return 0u;
    }
    return 1u;
}

uint8_t rnet_rb_extend_target(RNetRbSession *s, uint32_t new_target)
{
    uint32_t old_target;
    uint32_t begin;
    uint32_t new_span;
    uint32_t offset;
    uint32_t slot;

    if (rnet_rb_can_extend_target(s, new_target) == 0u)
    {
        return 0u;
    }
    old_target = s->corr.target_tick;
    if (new_target <= old_target)
    {
        /* Verify→Replay so host can rereplay. TipHold stays TipHold — host
         * calls schedule/rereplay only when Live already invented past tip. */
        if (s->phase == nRNetRbPhaseVerify)
        {
            s->phase = nRNetRbPhaseReplay;
        }
        return 1u;
    }
    begin = s->seal_base_tick;
    new_span = new_target - begin + 1u;
    for (offset = s->sealed_span; offset < new_span; ++offset)
    {
        uint32_t tick = begin + offset;
        for (slot = 0u; slot < RNET_RB_MAX_SLOTS; ++slot)
        {
            rnet_rb_fill_local_row(s, tick, offset, slot);
        }
        /* New offsets need peer rows again. */
    }
    s->sealed_span = new_span;
    s->corr.target_tick = new_target;
    if (s->phase == nRNetRbPhaseVerify)
    {
        s->phase = nRNetRbPhaseReplay;
    }
    /* TipHold remains TipHold (MotK invent-cap + optional short rereplay). */
    return 1u;
}

uint8_t rnet_rb_enter_tip_hold(RNetRbSession *s)
{
    if ((s == NULL) || (s->phase != nRNetRbPhaseVerify) || (s->inputs_sealed == 0u))
    {
        return 0u;
    }
    rnet_rb_commit_promote_sealed(s);
    s->phase = nRNetRbPhaseTipHold;
    return 1u;
}

uint8_t rnet_rb_resign_slot_range(RNetRbSession *s, int32_t slot, uint32_t from_tick,
                                  uint32_t to_tick)
{
    uint32_t tick;
    if ((s == NULL) || (s->inputs_sealed == 0u) || (rnet_rb_slot_valid(slot) == 0u))
    {
        return 0u;
    }
    if (to_tick < from_tick)
    {
        return 0u;
    }
    for (tick = from_tick; tick <= to_tick; ++tick)
    {
        uint32_t offset;
        RNetRbFrame row;
        RNetRbFrame *dst;
        if (rnet_rb_tick_in_sealed_span(s, tick) == 0u)
        {
            continue;
        }
        offset = tick - s->seal_base_tick;
        dst = &s->sealed[rnet_rb_seal_index(offset, (uint32_t)slot)];
        if (s->vt.get_input_row(s->vt.ctx, slot, tick, &row) == 0u)
        {
            continue;
        }
        *dst = row;
        dst->tick = tick;
        /* Host promoted wire into history — this offset is authoritative.
         * Predicted rows are NOT authority for a remote seat: crediting the
         * mask for invented pads let the resim arm ticks the seat owner had
         * not confirmed, forking the peers when the prediction was wrong. */
        if (row.is_valid != 0u &&
            (((uint32_t)slot == s->cfg.local_slot) || (row.is_predicted == 0u)))
        {
            s->peer_seal_mask[(uint32_t)slot] |= (1ull << offset);
        }
    }
    return 1u;
}

void rnet_rb_set_phase(RNetRbSession *s, RNetRbPhase phase)
{
    if (s == NULL)
    {
        return;
    }
    s->phase = phase;
    if ((phase == nRNetRbPhaseCommit) || (phase == nRNetRbPhaseAbort))
    {
        /* Terminal: advance the resolved-through watermark to the target so
         * the shared frontier reflects the agreed span. */
        if (s->corr.target_tick > s->resolved_through)
        {
            s->resolved_through = s->corr.target_tick;
        }
    }
}

RNetInputContractDecision rnet_rb_decide_stick_replace(RNetRbSession *s,
                                                       const RNetInputContractFrame *published,
                                                       const RNetInputContractFrame *wire,
                                                       uint8_t completed_sim)
{
    RNetInputContractParams params;

    if (s == NULL)
    {
        return nRNetInputContractRewind;
    }
    rnet_input_contract_params_init_defaults(&params);
    return rnet_input_contract_stick_replace_decide(published, wire, completed_sim, &params,
                                                    &s->vt.stick_gates);
}

void rnet_rb_seal_inputs(RNetRbSession *s, uint32_t begin_tick, uint32_t target_tick,
                         int32_t correction_slot)
{
    uint32_t span;
    uint32_t offset;
    uint32_t slot;

    (void)correction_slot;
    if (s == NULL)
    {
        return;
    }
    span = (target_tick >= begin_tick) ? (target_tick - begin_tick + 1u) : 0u;
    if (span == 0u)
    {
        return;
    }
    if (span > s->cfg.seal_max_span)
    {
        span = s->cfg.seal_max_span;
    }
    /* Seal local-authority rows from the host's input history; peer-authority
     * slots arrive via apply_peer_seal_rows. begin_tick is typically load_tick
     * so every Replay quantum has a sealed row. */
    s->seal_base_tick = begin_tick;
    s->sealed_span = 0u;
    for (offset = 0u; offset < span; ++offset)
    {
        uint32_t tick = begin_tick + offset;
        for (slot = 0u; slot < RNET_RB_MAX_SLOTS; ++slot)
        {
            rnet_rb_fill_local_row(s, tick, offset, slot);
        }
    }
    s->sealed_span = span;
    s->inputs_sealed = 1u;
    /* Keep corr.target in sync with the sealed tip. */
    if (target_tick >= begin_tick)
    {
        s->corr.target_tick = begin_tick + span - 1u;
    }
}

uint8_t rnet_rb_inputs_sealed(const RNetRbSession *s)
{
    return ((s != NULL) && (s->inputs_sealed != 0u)) ? 1u : 0u;
}

uint8_t rnet_rb_tick_in_sealed_span(const RNetRbSession *s, uint32_t tick)
{
    if ((s == NULL) || (s->inputs_sealed == 0u))
    {
        return 0u;
    }
    return ((tick >= s->seal_base_tick) && (tick < s->seal_base_tick + s->sealed_span)) ? 1u
                                                                                         : 0u;
}

uint8_t rnet_rb_get_sealed_frame(const RNetRbSession *s, int32_t slot, uint32_t tick,
                                 RNetRbFrame *out_frame)
{
    uint32_t offset;

    if ((s == NULL) || (out_frame == NULL) || (rnet_rb_tick_in_sealed_span(s, tick) == 0u) ||
        (rnet_rb_slot_valid(slot) == 0u))
    {
        return 0u;
    }
    offset = tick - s->seal_base_tick;
    *out_frame = s->sealed[rnet_rb_seal_index(offset, (uint32_t)slot)];
    return out_frame->is_valid;
}

uint32_t rnet_rb_get_seal_span(const RNetRbSession *s)
{
    return (s != NULL) ? s->sealed_span : 0u;
}

uint32_t rnet_rb_get_seal_base_tick(const RNetRbSession *s)
{
    return (s != NULL) ? s->seal_base_tick : 0u;
}

uint8_t rnet_rb_apply_peer_seal_rows(RNetRbSession *s, uint32_t epoch_id, uint32_t mismatch_tick,
                                     uint32_t target_tick, int32_t slot, uint32_t row_begin,
                                     const RNetRbFrame *rows, uint32_t row_count)
{
    uint32_t i;

    if ((s == NULL) || (rows == NULL) || (rnet_rb_inputs_sealed(s) == 0u) ||
        (rnet_rb_slot_valid(slot) == 0u))
    {
        return 0u;
    }
    /* Tuple compatibility: wire "mismatch" field carries seal_base_tick.
     * Tip-extend: peer may advertise a higher target — grow to match. */
    if ((epoch_id != s->corr.epoch_id) || (mismatch_tick != s->seal_base_tick))
    {
        return 0u;
    }
    if (target_tick < s->corr.target_tick)
    {
        return 0u; /* stale */
    }
    if (target_tick > s->corr.target_tick)
    {
        if (rnet_rb_extend_target(s, target_tick) == 0u)
        {
            return 0u;
        }
    }
    for (i = 0u; i < row_count; ++i)
    {
        uint32_t offset = row_begin + i;
        if (offset >= s->sealed_span)
        {
            break;
        }
        /* Reject empty/invalid rows — a wrong-seat export used to set the mask
         * with is_valid=0 and falsely complete SealInputs. */
        if (rows[i].is_valid == 0u)
        {
            continue;
        }
        {
            RNetRbFrame *dst = &s->sealed[rnet_rb_seal_index(offset, (uint32_t)slot)];
            /* Tip-extend MotK soak: FOLLOW invent-idle sealed their local seat
             * (Live stalled at invent-cap → hist miss) and overwrote the
             * initiator's wire-promoted authoritative resign. Never let a
             * predicted peer row clobber a non-predicted seal; still credit
             * the mask so SealInputs can complete. */
            if ((rows[i].is_predicted != 0u) && (dst->is_valid != 0u) &&
                (dst->is_predicted == 0u))
            {
                s->peer_seal_mask[(uint32_t)slot] |= (1ull << offset);
                continue;
            }
            *dst = rows[i];
            dst->tick = s->seal_base_tick + offset;
            s->peer_seal_mask[(uint32_t)slot] |= (1ull << offset);
        }
    }
    return 1u;
}

uint8_t rnet_rb_seat_row_authoritative(const RNetRbSession *s, int32_t slot, uint32_t tick)
{
    uint32_t offset;
    const RNetRbFrame *row;
    if ((s == NULL) || (rnet_rb_tick_in_sealed_span(s, tick) == 0u) ||
        (rnet_rb_slot_valid(slot) == 0u))
    {
        return 0u;
    }
    offset = tick - s->seal_base_tick;
    row = &s->sealed[rnet_rb_seal_index(offset, (uint32_t)slot)];
    if ((uint32_t)slot == s->cfg.local_slot)
    {
        return row->is_valid;
    }
    if ((offset < 64u) && ((s->peer_seal_mask[(uint32_t)slot] & (1ull << offset)) != 0ull))
    {
        return 1u;
    }
    return ((row->is_valid != 0u) && (row->is_predicted == 0u)) ? 1u : 0u;
}

uint8_t rnet_rb_peer_seal_rows_complete(const RNetRbSession *s, int32_t slot)
{
    uint64_t want;

    if ((s == NULL) || (rnet_rb_inputs_sealed(s) == 0u) || (rnet_rb_slot_valid(slot) == 0u))
    {
        return 0u;
    }
    want = (s->sealed_span >= 64u) ? ~0ull : ((1ull << s->sealed_span) - 1ull);
    return ((s->peer_seal_mask[(uint32_t)slot] & want) == want) ? 1u : 0u;
}

uint8_t rnet_rb_all_peer_seal_rows_complete(const RNetRbSession *s)
{
    uint32_t slot;
    uint32_t n;

    if ((s == NULL) || (rnet_rb_inputs_sealed(s) == 0u))
    {
        return 0u;
    }
    /* Only active match seats — waiting on unused slots (up to MAX=8) deadlocks
     * 2P MotK episodes forever in SealInputs. */
    n = s->cfg.slot_count;
    if (n == 0u || n > RNET_RB_MAX_SLOTS)
    {
        n = RNET_RB_MAX_SLOTS;
    }
    for (slot = 0u; slot < n; ++slot)
    {
        if (slot == s->cfg.local_slot)
        {
            continue;
        }
        if (rnet_rb_peer_seal_rows_complete(s, (int32_t)slot) == 0u)
        {
            return 0u;
        }
    }
    return 1u;
}

uint8_t rnet_rb_export_seal_rows_chunk(const RNetRbSession *s, int32_t slot, uint32_t row_begin,
                                       uint32_t max_rows, RNetRbFrame *out_frames,
                                       uint32_t *out_row_count)
{
    uint32_t n;

    if ((s == NULL) || (out_frames == NULL) || (out_row_count == NULL) ||
        (rnet_rb_inputs_sealed(s) == 0u) || (rnet_rb_slot_valid(slot) == 0u))
    {
        return 0u;
    }
    if (row_begin >= s->sealed_span)
    {
        *out_row_count = 0u;
        return 1u;
    }
    n = s->sealed_span - row_begin;
    if (n > max_rows)
    {
        n = max_rows;
    }
    /* Chunk carries only this slot's rows. */
    {
        uint32_t i;
        for (i = 0u; i < n; ++i)
        {
            out_frames[i] = s->sealed[rnet_rb_seal_index(row_begin + i, (uint32_t)slot)];
        }
    }
    *out_row_count = n;
    return 1u;
}

uint32_t rnet_rb_resolved_through(const RNetRbSession *s)
{
    return (s != NULL) ? s->resolved_through : 0u;
}

void rnet_rb_set_peer_convergence(RNetRbSession *s, uint32_t peer_target)
{
    if ((s != NULL) && (peer_target > s->resolved_through))
    {
        s->resolved_through = peer_target;
    }
}

void rnet_rb_demote_resolved_through(RNetRbSession *s, uint32_t tick)
{
    if ((s != NULL) && (tick < s->resolved_through))
    {
        s->resolved_through = tick;
    }
}

void rnet_rb_on_post_match(RNetRbSession *s)
{
    if (s == NULL)
    {
        return;
    }
    rnet_rb_commit_promote_sealed(s);
    rnet_rb_set_phase(s, nRNetRbPhaseCommit);
}

void rnet_rb_on_post_diverge(RNetRbSession *s)
{
    if (s == NULL)
    {
        return;
    }
    /* Stay in Verify; host decides to deepen the load or abort. */
    rnet_rb_set_phase(s, nRNetRbPhaseVerify);
}

void rnet_rb_commit_promote_sealed(RNetRbSession *s)
{
    /* Sealed rows become authoritative in the host's history on commit; the
     * host owns the actual promotion into its ledger during advance/replay.
     * Here we just mark the watermark. */
    if ((s != NULL) && (s->corr.target_tick > s->resolved_through))
    {
        s->resolved_through = s->corr.target_tick;
    }
}

void rnet_rb_enqueue_event(RNetRbSession *s, const RNetRbEvent *event)
{
    uint32_t next;

    if ((s == NULL) || (event == NULL))
    {
        return;
    }
    next = (s->event_tail + 1u) % RNET_RB_EVENT_QUEUE_MAX;
    if (next == s->event_head)
    {
        return; /* full: drop oldest discipline is host policy */
    }
    s->events[s->event_tail] = *event;
    s->event_tail = next;
}

uint8_t rnet_rb_drain_next_event(RNetRbSession *s, RNetRbEvent *out_event)
{
    if ((s == NULL) || (out_event == NULL) || (s->event_head == s->event_tail))
    {
        return 0u;
    }
    *out_event = s->events[s->event_head];
    s->event_head = (s->event_head + 1u) % RNET_RB_EVENT_QUEUE_MAX;
    return 1u;
}

uint8_t rnet_rb_has_pending_events(const RNetRbSession *s)
{
    return ((s != NULL) && (s->event_head != s->event_tail)) ? 1u : 0u;
}
