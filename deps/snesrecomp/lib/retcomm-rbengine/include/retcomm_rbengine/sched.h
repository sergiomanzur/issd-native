#ifndef RETCOMM_RBENGINE_SCHED_H
#define RETCOMM_RBENGINE_SCHED_H

/*
 * Rollback admission scheduler (policy only).
 *
 * Lifted from MotK psx_netplay_sched. Keeps peers paced, keeps the delay
 * cushion full, invents only on genuine runway starvation, and resolves D
 * from measured arrival latency. Never touches input history, tip-hold, or
 * snapshot rings — guest determinism is unaffected by changes here.
 *
 * Game-specific behavior (FMV lockstep, RTT estimate, episode active) enters
 * only through RbeSchedGates.
 */

#include <stdint.h>

#include "recomp_net/session.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RBE_SCHED_MAX_SLOTS 8

typedef struct RbeSchedGates {
    void *ctx;
    /* Required for pacing / grace. Prefer QPC / CLOCK_MONOTONIC. */
    uint32_t (*now_ms)(void *ctx);
    /* Optional POST/ICE RTT estimate in ms (0 = unknown / untrusted). */
    uint32_t (*rtt_ms)(void *ctx);
    /* 1 while a rollback episode is sealing/replaying (not TipHold Live). */
    uint8_t (*episode_active)(void *ctx);
    /* 1 while TipHold Live (seals open, host invents at tip). */
    uint8_t (*tip_holding)(void *ctx);
    /* 1 while host forbids invent (FMV media / settle / desync hold). */
    uint8_t (*lockstep_no_invent)(void *ctx);
    /* Optional stall tag when lockstep_no_invent (e.g. "fmv_media"). */
    const char *(*lockstep_stall_tag)(void *ctx);
    /* 1 during media that must not ratchet D or pile timesync debt. */
    uint8_t (*media_active)(void *ctx);
    /* Optional: DESYNC invent-hold (MotK §93) — affects stall tag only. */
    uint8_t (*desync_hold)(void *ctx);
    /* Optional host pre-admit hold (e.g. MotK rematch dig0 CRC gate).
     * Return 1 to stall; optionally set *tag_out. */
    uint8_t (*pre_admit_hold)(void *ctx, uint32_t sim, uint32_t wire,
                              const char **tag_out);
    /* Optional scorecard counters. */
    uint32_t (*episode_count)(void *ctx);
    uint64_t (*replay_ticks_total)(void *ctx);
} RbeSchedGates;

/* Live pointers into host session state (session may repoint on restart). */
typedef struct RbeSchedBridge {
    RNetSession **session;
    int *input_delay;      /* committed D, ticks */
    int *input_prediction; /* P cap, ticks */
    int *local_slot;
    int force_turn;        /* 1 = ICE relay-only — auto-delay floor applies */
    /* Optional: 1 while the session runs ROLLBACK (invent/episodes). The
     * min-D floor applies only then; delay-sync D is pure input latency. */
    int *rollback;
    RbeSchedGates gates;
} RbeSchedBridge;

void rbe_sched_bind(const RbeSchedBridge *bridge);

/* Clear session-scoped pacing/invent state. Called from rbe_sched_bind. */
void rbe_sched_reset_session(void);

/* sim→wire CONSUMPTION mapping. Default REAL-DELAY: guest tick T plays wire T;
 * local sample at admit(T) stored at T+D. RBE_RB_ZERO_DELAY=1 → legacy T plays
 * wire T+D (no cushion, permanent pred_depth 1). */
uint32_t rbe_sched_wire_for_sim(uint32_t sim_tick);
int rbe_sched_real_delay_enabled(void);

void rbe_sched_sync_delay_from_session(void);

/* Pre-admit gate: tip cadence, timesync throttle, cushion rebuild, auto-D.
 * Returns 1 = stall this admit, 0 = proceed. */
int rbe_sched_pre_admit(uint32_t sim, uint32_t wire, const RNetSessionStats *st);

/* Remote row missing at wire for slot. Returns 1 = stall (grace/freeze/cushion),
 * 0 = invent hold-last now (*reason_out set). */
int rbe_sched_on_remote_miss(int slot, uint32_t sim, uint32_t wire,
                             const RNetSessionStats *st, int pred,
                             const char **reason_out);

void rbe_sched_note_remote_hit(void);
void rbe_sched_post_admit(int any_invent);

void rbe_sched_set_admit_stall(const char *tag);
void rbe_sched_clear_admit_stall(void);
const char *rbe_sched_admit_stall_tag(void);

void rbe_sched_note_mispredict(uint32_t age);
void rbe_sched_note_episode_boundary(void);
void rbe_sched_arm_absurd_invent_catchup(void);

/* Alias kept for MotK-era call sites. */
void rbe_sched_timesync_on_episode_boundary(void);

#ifdef __cplusplus
}
#endif

#endif /* RETCOMM_RBENGINE_SCHED_H */
