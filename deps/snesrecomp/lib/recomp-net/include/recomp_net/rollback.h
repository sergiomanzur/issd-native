#ifndef RECOMP_NET_ROLLBACK_H
#define RECOMP_NET_ROLLBACK_H

/*
 * Shared rollback episode orchestration — game-agnostic core.
 *
 * recomp-net owns the episode FSM (when to rewind vs promote), the correction
 * tuple, the sealed input table, and the resolved-through / shared frontier
 * watermarks. The host owns everything game-specific: snapshot save/load, the
 * deterministic sim step, state digests, and (initially) input prediction and
 * the wire transport that delivers seal/baseline/sync packets.
 *
 * This is the second rollback layer after the portable input contract
 * (recomp_net/input_contract.h). It is transport-agnostic: hosts call the API
 * from their own network ingress (Phase 2 keeps BattleShip's netpeer as the
 * transporter); protocol/ICE alignment is Phase 3.
 *
 * Single-threaded session ownership, same as delay-sync RNetSession.
 */

#include <stdint.h>
#include <stddef.h>

#include "recomp_net/input_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Seal span covers the deepest resim span a host can issue; matches the
 * frame-commit validation cadence + slack used by the reference host. */
#define RNET_RB_SEAL_MAX_SPAN 128u
/* Peer-seal completion is a uint64_t bitmask — tip-extend cannot grow the
 * sealed span past this even when seal_max_span is larger. Hosts must
 * tip-hold-commit and open a fresh episode when extend would exceed it. */
#define RNET_RB_PEER_SEAL_MASK_BITS 64u
#define RNET_RB_MAX_SLOTS 8
/* Tip episode: target - load at or below this may skip the ready-ACK RTT
 * (digests still compared). Sized for tip-extend re-replay after TipHold. */
#define RNET_RB_LIGHT_TIP_MAX_DEPTH 16u
/* Live quiet window after POST match (TipHold) so a physical press→release
 * (typically 6–15 ticks later) tip-extends the same episode instead of a
 * second seal/baseline handshake. Also the cap for suggest_target slack. */
#define RNET_RB_TIP_RUNWAY_DEFAULT 12u
/* Initial seal headroom past live tip when opening an episode. Kept small —
 * TipHold covers the release; burning a long runway in catch-up finishes
 * before the finger lifts. */
#define RNET_RB_TIP_SEAL_SLACK_DEFAULT 2u
/* corr.flags */
#define RNET_RB_CORR_LIGHT_TIP 0x01u

typedef enum RNetRbPhase
{
    nRNetRbPhaseLive = 0,
    nRNetRbPhaseSealInputs,
    nRNetRbPhaseAwaitingBaseline,
    nRNetRbPhaseReplay,
    nRNetRbPhaseVerify,
    /* POST matched: seals stay open for tip-extend while host runs Live. */
    nRNetRbPhaseTipHold,
    nRNetRbPhaseCommit,
    nRNetRbPhaseAbort
} RNetRbPhase;

typedef enum RNetRbRole
{
    nRNetRbRoleInitiator = 0,
    nRNetRbRoleFollower
} RNetRbRole;

typedef enum RNetRbEventType
{
    nRNetRbEventNone = 0,
    nRNetRbEventInputMismatch,
    nRNetRbEventPeerSymmetric,
    nRNetRbEventStateDiverge,
    nRNetRbEventFrameCommit
} RNetRbEventType;

typedef struct RNetRbCorrection
{
    uint32_t epoch_id;
    uint32_t mismatch_tick;
    uint32_t load_tick;
    uint32_t target_tick;
    int32_t slot;            /* corrected player slot (-1 = whole state) */
    uint8_t initiator;       /* 1 when this host started the episode */
    uint8_t from_peer_notify;
    uint8_t flags;           /* RNET_RB_CORR_LIGHT_TIP, … */
} RNetRbCorrection;

/* Opaque per-slot input row the library seals and replays. The library never
 * interprets the payload beyond the stick-replace contract view.
 * analog: host pad type (0 = digital / poll 0x41, 1 = DualShock / 0x73).
 * Carried on the seal wire in RNetRbWireFrame.source (was always 0). */
typedef struct RNetRbFrame
{
    uint32_t tick;
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
    uint8_t is_predicted;
    uint8_t is_valid;
    uint8_t analog;
} RNetRbFrame;

typedef struct RNetRbEvent
{
    RNetRbEventType type;
    int32_t slot;
    uint32_t mismatch_tick;
    uint32_t target_tick;
    uint32_t load_tick;
    uint32_t epoch_id;
    uint8_t follower_local_auth;
} RNetRbEvent;

/*
 * Host callbacks. save/load/advance/digest are required; the gates mirror the
 * input contract and may be NULL (portable defaults). All runs on the host's
 * thread; the library does not spawn work.
 */
typedef struct RNetRollbackVTable
{
    void *ctx;
    /* Persist a snapshot at sim tick (library requests the deepest needed). */
    int (*save_state)(void *ctx, uint32_t tick);
    /* Restore the snapshot captured at sim tick before replay. */
    int (*load_state)(void *ctx, uint32_t tick);
    /* Advance exactly one deterministic sim tick using resolved inputs the
     * host published (sealed local + confirmed/peer-sealed remote rows). */
    int (*advance_sim)(void *ctx, uint32_t tick);
    /* Digest of canonical state at tick for agreement comparison; partition
     * selects a subsystem (0 = master). Must be identical across peers. */
    uint32_t (*state_digest)(void *ctx, uint32_t tick, uint32_t partition);
    /* 1 when the peer state/master-hash watermark has agreed through tick
     * (frame-commit). Backs input-contract hash_confirm_promote. */
    uint8_t (*hash_confirm_through)(void *ctx, uint32_t tick);
    /* Sample the authoritative row for a slot at a wire tick from the host's
     * input history (for sealing + self-seal fallback). */
    uint8_t (*get_input_row)(void *ctx, int32_t slot, uint32_t tick, RNetRbFrame *out_frame);
    /* Stick-replace contract gates; NULL = portable defaults. */
    RNetInputContractHostGates stick_gates;
} RNetRollbackVTable;

typedef struct RNetRbSession RNetRbSession;

typedef struct RNetRbConfig
{
    uint32_t local_slot;       /* this host's player slot */
    uint32_t delay;            /* committed input delay D */
    uint32_t seal_max_span;    /* <= RNET_RB_SEAL_MAX_SPAN; 0 = default */
    /* Active seats in this match (1..RNET_RB_MAX_SLOTS). Peer-seal completion
     * only waits on slots in [0, slot_count) excluding local_slot. 0 => 2. */
    uint32_t slot_count;
    /* TipHold quiet window after POST match (0 = finalize immediately;
     * RNET_RB_TIP_RUNWAY_DEFAULT recommended for digital hosts). Also the
     * maximum slack suggest_target may add (capped by tip_seal_slack). */
    uint32_t tip_runway;
    /* Initial seal headroom past max(sim, mismatch). 0 → TIP_SEAL_SLACK_DEFAULT
     * when tip_runway > 0; set UINT32_MAX to force 0 slack. */
    uint32_t tip_seal_slack;
    /* Max (target - load) depth eligible to skip the ready-ACK RTT (see
     * rnet_rb_is_light_tip_candidate). 0 → RNET_RB_LIGHT_TIP_MAX_DEPTH.
     * Hosts that widen tip_runway for TipHold coalescing (a tip-extended
     * episode's eventual depth can approach tip_runway) should set this to
     * match tip_runway — otherwise every episode that coalesces past the
     * library default of 16 silently loses the light-tip fast path and pays
     * a second RTT it didn't need to. Clamped like tip_runway (max 32). */
    uint32_t light_tip_max_depth;
} RNetRbConfig;

/* Lifecycle. */
RNetRbSession *rnet_rb_create(const RNetRbConfig *cfg, const RNetRollbackVTable *vt);
void rnet_rb_destroy(RNetRbSession *s);
void rnet_rb_session_reset(RNetRbSession *s);

/* Phase / tuple introspection (read-only). */
RNetRbPhase rnet_rb_get_phase(const RNetRbSession *s);
uint8_t rnet_rb_is_active(const RNetRbSession *s);
uint8_t rnet_rb_is_resimulating(const RNetRbSession *s);
uint8_t rnet_rb_is_tip_holding(const RNetRbSession *s);
uint32_t rnet_rb_get_tip_runway(const RNetRbSession *s);
uint32_t rnet_rb_get_tip_seal_slack(const RNetRbSession *s);
uint32_t rnet_rb_get_light_tip_max_depth(const RNetRbSession *s);
uint32_t rnet_rb_get_epoch_id(const RNetRbSession *s);
uint32_t rnet_rb_get_mismatch_tick(const RNetRbSession *s);
uint32_t rnet_rb_get_load_tick(const RNetRbSession *s);
uint32_t rnet_rb_get_target_tick(const RNetRbSession *s);
int32_t rnet_rb_get_corrected_slot(const RNetRbSession *s);
uint8_t rnet_rb_is_from_peer_notify(const RNetRbSession *s);
uint8_t rnet_rb_get_corr_flags(const RNetRbSession *s);

/* 1 when (target - load) <= RNET_RB_LIGHT_TIP_MAX_DEPTH and load is at/after
 * the shared frontier (resolved_through). Hosts may skip ready-ACK RTT.
 * Convenience wrapper over rnet_rb_is_light_tip_candidate_ex using the
 * library-wide default depth; a session created with a non-default
 * cfg.light_tip_max_depth should call the _ex form (or just
 * rnet_rb_recommend_light_tip, which already does). */
uint8_t rnet_rb_is_light_tip_candidate(uint32_t load_tick, uint32_t target_tick,
                                       uint32_t resolved_through);
/* Same as above with an explicit max-depth ceiling — use this (with
 * rnet_rb_get_light_tip_max_depth(session)) when precomputing the flag for a
 * session configured with a non-default light_tip_max_depth. */
uint8_t rnet_rb_is_light_tip_candidate_ex(uint32_t load_tick, uint32_t target_tick,
                                          uint32_t resolved_through, uint32_t max_depth);
uint8_t rnet_rb_recommend_light_tip(const RNetRbSession *s);

/*
 * Correction entry points. Host calls begin_episode when it (or a symmetric
 * peer notice) identifies a mismatch; the library takes the phase to
 * SealInputs. The host then drives sealing and peer exchange, advancing the
 * FSM with set_phase as its transport confirms baseline/replay/verify.
 */
void rnet_rb_begin_episode(RNetRbSession *s, const RNetRbCorrection *corr);
void rnet_rb_set_phase(RNetRbSession *s, RNetRbPhase phase);

/*
 * Tip-extend / edge coalesce: grow target_tick and append seal rows for
 * (old_target, new_target]. Allowed in SealInputs / AwaitingBaseline /
 * Replay / Verify / TipHold. Verify drops to Replay; TipHold stays TipHold
 * (host schedules rereplay only when Live already invented past the tip).
 * Returns 1 on success (including already-at-target).
 *
 * Also refreshes local-authority sealed rows in the new range from
 * get_input_row (host must promote late wire into history first).
 */
uint8_t rnet_rb_extend_target(RNetRbSession *s, uint32_t new_target);

/* 1 if extend_target(new_target) would succeed (sealed, phase OK, span fits
 * seal_max_span and RNET_RB_PEER_SEAL_MASK_BITS). Does not mutate. */
uint8_t rnet_rb_can_extend_target(const RNetRbSession *s, uint32_t new_target);

/* Re-sample sealed rows for one slot from host history over [from, to]
 * inclusive (clamped to the sealed span). Used after promoting late wire
 * for a tick already inside the current target (press sealed, release
 * arrives before tip). */
uint8_t rnet_rb_resign_slot_range(RNetRbSession *s, int32_t slot, uint32_t from_tick,
                                  uint32_t to_tick);

/* Suggested target = max(sim_tip, mismatch) + tip_seal_slack (small;
 * TipHold covers physical release coalesce). */
uint32_t rnet_rb_suggest_target(const RNetRbSession *s, uint32_t mismatch_tick,
                                uint32_t sim_tip);

/* After POST digests match: advance resolved_through, keep seals, enter
 * TipHold so the host can run Live while late edges tip-extend. Returns 1
 * on success. Host calls session_reset (or on_post_match→Commit) when the
 * tip_runway quiet window expires. */
uint8_t rnet_rb_enter_tip_hold(RNetRbSession *s);

/* Stick-replace decision over a published vs authoritative wire row; thin
 * wrapper so hosts can resolve rewind-vs-promote with the shared contract and
 * their gates before queueing an episode. */
RNetInputContractDecision rnet_rb_decide_stick_replace(RNetRbSession *s,
                                                       const RNetInputContractFrame *published,
                                                       const RNetInputContractFrame *wire,
                                                       uint8_t completed_sim);

/* Sealed input table. Local-authority rows seal from the host's input history;
 * peer-authority rows arrive via apply_peer_seal_rows. The sealed table is the
 * sole replay read set. */
/* begin_tick..target_tick inclusive — pass load_tick (not only mismatch) so
 * Replay can publish sealed pads for every resim quantum. */
void rnet_rb_seal_inputs(RNetRbSession *s, uint32_t begin_tick, uint32_t target_tick,
                         int32_t correction_slot);
uint8_t rnet_rb_inputs_sealed(const RNetRbSession *s);
uint8_t rnet_rb_tick_in_sealed_span(const RNetRbSession *s, uint32_t tick);
uint8_t rnet_rb_get_sealed_frame(const RNetRbSession *s, int32_t slot, uint32_t tick,
                                 RNetRbFrame *out_frame);
/* 1 iff the sealed row for (slot, tick) is safe to sim with: local seat with a
 * valid row, a peer-delivered SEAL_ROWS row, or a wire-confirmed
 * (!is_predicted) locally-sealed row. Predicted remote rows are NOT
 * authoritative — arming them forks the peers when the prediction is wrong. */
uint8_t rnet_rb_seat_row_authoritative(const RNetRbSession *s, int32_t slot, uint32_t tick);
uint32_t rnet_rb_get_seal_span(const RNetRbSession *s);
uint32_t rnet_rb_get_seal_base_tick(const RNetRbSession *s);

/* Peer seal-row exchange (host transports the chunks; library validates tuple
 * compatibility, marks completion, and gates forward replay). */
uint8_t rnet_rb_apply_peer_seal_rows(RNetRbSession *s, uint32_t epoch_id, uint32_t mismatch_tick,
                                     uint32_t target_tick, int32_t slot, uint32_t row_begin,
                                     const RNetRbFrame *rows, uint32_t row_count);
uint8_t rnet_rb_peer_seal_rows_complete(const RNetRbSession *s, int32_t slot);
uint8_t rnet_rb_all_peer_seal_rows_complete(const RNetRbSession *s);
uint8_t rnet_rb_export_seal_rows_chunk(const RNetRbSession *s, int32_t slot, uint32_t row_begin,
                                       uint32_t max_rows, RNetRbFrame *out_frames,
                                       uint32_t *out_row_count);

/* Shared frontier / resolved-through watermark (highest sim tick agreed with
 * peers). Drives live-sim caps and input-contract hash_confirm_promote. */
uint32_t rnet_rb_resolved_through(const RNetRbSession *s);
void rnet_rb_set_peer_convergence(RNetRbSession *s, uint32_t peer_target);
/* Pull resolved_through down to tick when a follow-NACK / unilateral tip is
 * refused (tick < current). set_peer_convergence only advances — without a
 * demote, session_reset keeps a poisoned frontier and the next light-tip /
 * HC advance reopens the refused load. No-op when tick >= current. */
void rnet_rb_demote_resolved_through(RNetRbSession *s, uint32_t tick);

/* Episode resolution. Host calls on_post_match / on_post_diverge after the
 * post-replay digest comparison; library commits the sealed rows or deepens /
 * aborts. */
void rnet_rb_on_post_match(RNetRbSession *s);
void rnet_rb_on_post_diverge(RNetRbSession *s);
void rnet_rb_commit_promote_sealed(RNetRbSession *s);

/* Event queue (peer symmetric notices, frame-commit) drained by the host. */
void rnet_rb_enqueue_event(RNetRbSession *s, const RNetRbEvent *event);
uint8_t rnet_rb_drain_next_event(RNetRbSession *s, RNetRbEvent *out_event);
uint8_t rnet_rb_has_pending_events(const RNetRbSession *s);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_ROLLBACK_H */
