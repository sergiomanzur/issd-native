#ifndef RECOMP_NET_SESSION_H
#define RECOMP_NET_SESSION_H

#include "recomp_net/config.h"
#include "recomp_net/ice.h"
#include "recomp_net/input.h"
#include "recomp_net/rollback.h"
#include "recomp_net/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RNetSession RNetSession;

typedef struct RNetHostVTable
{
    /* Fill local pad sample for the given sim tick (called during try_admit). */
    void (*sample_local)(rnet_u32 tick, RNetInputSample *out, void *ctx);
    /* Publish resolved inputs for all slots after admission succeeds. */
    void (*publish)(rnet_u32 tick, const RNetInputSample *by_slot, int slots, void *ctx);
    /* Optional wall/monotonic clock; NULL uses platform monotonic ms. */
    rnet_u64 (*now_ms)(void *ctx);
    /* Emit ICE signaling toward the host lobby (may be NULL for LAN-only). */
    void (*on_signal)(const RNetSignal *msg, void *ctx);
    void *ctx;
} RNetHostVTable;

RNetSession *rnet_session_create(const RNetConfig *cfg, const RNetHostVTable *host);
void rnet_session_destroy(RNetSession *s);

/* Start raw UDP peer session. bind/peer are "host:port" or ":port" / "ip:port".
 * peer may be NULL/empty for host-style LAN: the first valid session packet
 * selects the peer endpoint. */
int rnet_session_start_lan(RNetSession *s, const char *bind_hostport, const char *peer_hostport);

/*
 * Host-as-relay: bind UDP and fan-out delay-sync datagrams to every learned
 * guest seat. Transport role is independent of sim local_slot (lobby owner
 * hubs; guests dial host_endpoint). Use when slot_count >= 3 and the lobby
 * server input relay is not enabled.
 */
int rnet_session_start_lan_hub(RNetSession *s, const char *bind_hostport);

/*
 * Start ICE-backed session (requires RNET_ENABLE_ICE). After create, host must
 * exchange signals via on_signal / rnet_session_push_signal until COMPLETED.
 */
int rnet_session_start_ice(RNetSession *s, const RNetIceConfig *ice);

/* Recv datagrams, poll ICE, send pending INPUT, drive bootstrap. */
void rnet_session_pump(RNetSession *s);

/*
 * Park until a LAN datagram may be readable (or timeout_ms). Prefer this over
 * Sleep/Delay in admit barriers. ICE falls back to sleep.
 * Returns 1 if the socket reported readable, 0 on timeout/unavailable.
 */
int rnet_session_wait_recv(RNetSession *s, int timeout_ms);

/*
 * Returns 1 when gameplay inputs for sim_tick (wire=sim) are present for
 * every remote, their pipelined INPUT_CONFIRM hashes agree, and publish has
 * been called. A fresh local sample and confirmation are prepared at
 * wire=sim+D. Returns 0 to stall, including on input desync.
 */
int rnet_session_try_admit(RNetSession *s, rnet_u32 sim_tick);

/* Call after the host completes one authoritative sim step. */
void rnet_session_advance(RNetSession *s);

/*
 * Returns 1 if an INPUT_CONFIRM hash disagreement was flagged. Optional outs
 * receive the desync sim tick and local/remote hashes.
 */
int rnet_session_input_desync(const RNetSession *s, rnet_u32 *tick, rnet_u32 *local_hash, rnet_u32 *remote_hash);

/** Best-effort BYE so the peer can drop immediately instead of waiting for timeout. */
int rnet_session_send_bye(RNetSession *s);

/**
 * Non-zero if peer sent BYE or no valid UDP for timeout_ms after first RX.
 * Pass ~1500 for a snappy disconnect while still tolerating brief stalls.
 * Pass 0 for BYE-only (no silence timeout) — used during load barriers and
 * while LINKING before HELLO (rematch boot can exceed 15s on one peer).
 * Before any peer RX, LINKING/RUNNING uses a ≥90s link budget (not *10).
 */
int rnet_session_peer_disconnected(const RNetSession *s, rnet_u64 timeout_ms);

/**
 * Stamp last_peer_rx_ms = now without clearing peer_gone.
 * Call after a pump when entering an admit barrier so free-run / heavy dig
 * between vblanks cannot false-trip the RUNNING silence budget. BYE still wins.
 */
void rnet_session_touch_peer_liveness(RNetSession *s);

/* Deliver an inbound signaling message from the lobby. */
void rnet_session_push_signal(RNetSession *s, const RNetSignal *msg);

rnet_u8 rnet_session_committed_delay(const RNetSession *s);
/*
 * Schedule a mid-session input_delay change at a future sim tick and emit
 * DELAY_SYNC. Both peers apply when sim_tick reaches effective_tick.
 * new_delay is clamped to 2..20. Returns 1 on accept, 0 on reject.
 */
int rnet_session_request_delay_change(RNetSession *s, rnet_u8 new_delay);
int rnet_session_local_slot(const RNetSession *s);
rnet_u32 rnet_session_sim_tick(const RNetSession *s);
int rnet_session_is_running(const RNetSession *s);
RNetIceState rnet_session_ice_state(const RNetSession *s);

/* Why try_admit last returned 0 (or OK when last call admitted). */
typedef enum RNetAdmitStall
{
    RNET_ADMIT_OK = 0,
    RNET_ADMIT_NOT_RUNNING,
    RNET_ADMIT_STATE_XFER,
    RNET_ADMIT_SIM_MISMATCH,
    RNET_ADMIT_DESYNC,
    RNET_ADMIT_WAIT_LOCAL_INPUT,
    RNET_ADMIT_WAIT_REMOTE_INPUT,
    RNET_ADMIT_WAIT_CONFIRM
} RNetAdmitStall;

const char *rnet_admit_stall_name(RNetAdmitStall reason);

/*
 * Snapshot for catch-up / diagnostics. Filled by rnet_session_get_stats.
 * ice_* strings are NUL-terminated buffers owned by this struct.
 */
typedef struct RNetSessionStats
{
    rnet_u32 sim_tick;
    rnet_u8 delay;
    rnet_u8 local_slot;
    rnet_u8 slot_count;
    int is_running;
    int peer_gone;
    rnet_u64 last_peer_rx_age_ms;
    rnet_u32 highest_remote_wire;
    int remote_lead; /* highest_remote_wire - sim_tick (can be negative) */
    RNetAdmitStall last_stall;
    rnet_u32 consecutive_stalls;
    rnet_u32 admit_ok_count;
    rnet_u32 stall_streaks; /* completed stall streaks (not barrier spins) */
    rnet_u32 last_admit_wait_ms;
    rnet_u32 max_admit_wait_ms;
    int input_desync;
    rnet_u32 desync_tick;
    rnet_u32 desync_local_hash;
    rnet_u32 desync_remote_hash;
    RNetIceState ice_state;
    char ice_path[16];   /* host|srflx|prflx|relay|lan|pending|failed|unknown */
    char ice_local[96];
    char ice_remote[96];
    int state_busy;
    rnet_u8 state_op;
    int state_sender;          /* 1 while local peer is the STATE sender */
    rnet_u32 state_bytes_total; /* active xfer size (0 if idle) */
    rnet_u32 state_bytes_acked; /* sender: peer ACK; receiver: contiguity */
    rnet_u32 packets_rx;
    rnet_u32 input_bundle_sends;
} RNetSessionStats;

void rnet_session_get_stats(const RNetSession *s, RNetSessionStats *out);

/* Savestate / SRAM transfer ops for rnet_session_state_begin / probe. */
#define RNET_STATE_OP_SAVE 0 /* peer stores file; admit stalls until probe/xfer done */
#define RNET_STATE_OP_LOAD 1 /* stalls admit until guest has blob */
#define RNET_STATE_OP_SRAM 2 /* stalls admit until guest has blob */
#define RNET_STATE_OP_RB_KF 3 /* rollback FMV media keyframe (raw boot_state snap) */
#define RNET_STATE_OP_BOOT 4 /* post-BIOS host snap barrier (boot_state blob) */

/*
 * Hash probe (host→guest): announce (op, slot, size, crc).
 * - SAVE + size==0: coordinate local save (no INPUT/admit stall — savestate_poll).
 * - LOAD + size==0: post-load ready rendezvous (no INPUT stall — late applier
 *   still needs tip rows; app freezes sim until mutual ready + hard_resync).
 * - BOOT + size==0: post-boot-snap ready rendezvous (same INPUT rules as LOAD).
 * - size!=0: hash announce; stalls until probe_finish or following transfer.
 */
int rnet_session_state_probe(RNetSession *s, rnet_u8 op, rnet_u8 slot, rnet_u32 total_size,
                             rnet_u32 payload_crc);
/* Host: 1 when guest replied; *match_out = guest already has identical blob. */
int rnet_session_state_probe_take_reply(RNetSession *s, int *match_out);
/* Guest: 1 if a host probe is waiting for a local hash/coord answer. */
int rnet_session_state_probe_pending(const RNetSession *s, rnet_u8 *op_out, rnet_u8 *slot_out,
                                     rnet_u32 *size_out, rnet_u32 *crc_out);
/* Guest: answer after hashing local file (or finishing coord save). */
int rnet_session_state_probe_reply(RNetSession *s, int match);
/* Clear probe state (host after match-skip, or after starting a transfer). */
void rnet_session_state_probe_finish(RNetSession *s);

/*
 * Host-only (local_slot == 0) chunked blob transfer. Stalls try_admit until the
 * peer ACKs the full payload (all ops). Prefer probe-first; call begin only on
 * hash miss. payload_crc in BEGIN is verified by the guest before ready.
 */
int rnet_session_state_begin(RNetSession *s, rnet_u8 op, rnet_u8 slot, const void *data, size_t size);
int rnet_session_state_busy(const RNetSession *s);
/* 1 when transfer complete and blob ready for guest apply/store (or host finish). */
int rnet_session_state_take_ready(RNetSession *s, rnet_u8 *op_out, rnet_u8 *slot_out, const void **data_out,
                                  size_t *size_out);
/* After apply/store: clear transfer; hard_resync clears input rings on LOAD. */
void rnet_session_state_finish(RNetSession *s, int hard_resync);
/* Post-load resync: clear local + remote rings + confirm, sim_tick → 0.
 * Call once at mutual ready, then prime_delay_inputs on both peers and wait
 * for try_admit (do not drop the app barrier until admit succeeds). */
void rnet_session_hard_resync(RNetSession *s);
/* After hard_resync: seed local delay tip with opaque pad bytes and send so the
 * peer can admit immediately once the ready rendezvous completes. */
void rnet_session_prime_delay_inputs(RNetSession *s, const rnet_u8 *bytes, rnet_u16 size);
/* Suppress INPUT bundle emits (LOAD apply/ready until prime). Admit still runs
 * while savestate_pending so the restore can execute. */
void rnet_session_set_input_send_suppress(RNetSession *s, int suppress);

/*
 * Rollback FRAME_COMMIT helpers (RNET_PKT_RB_FRAME_COMMIT). Delay-sync peers
 * ignore these opcodes. Hosts call send after each sim tick with their master
 * state digest; pump stores the latest peer commit for take_*.
 */
int rnet_session_send_rb_frame_commit(RNetSession *s, rnet_u32 through_tick,
                                      rnet_u32 state_hash);
/* 1 if a peer FRAME_COMMIT is pending; copies and clears it. */
/* PSX-Link group scoping: accept rollback episode/state packets only from
 * `slot` (-1 = all, default). Input-plane packets are never filtered. */
void rnet_session_set_rb_peer_slot(RNetSession *s, int slot);
int rnet_session_take_rb_frame_commit(RNetSession *s, rnet_u32 *through_tick,
                                      rnet_u32 *state_hash);

/*
 * GBA Multi transfer barrier (RNET_PKT_SIO_MULTI_XFER): exchange SIOMLT_SEND
 * out-of-band at 0 effective delay. Not pad INPUT — callers must not put Multi
 * words through D-delayed samples while this path is active.
 */
/* confirm_pad: lo=Confirm IRQ watermark, hi=VBlank mod 256. */
int rnet_session_send_sio_multi_xfer(RNetSession *s, rnet_u8 unit_id, rnet_u32 seq,
                                     rnet_u16 send, rnet_u16 confirm_pad);
/* 1 if a peer Multi xfer is pending; copies and clears one queue entry. */
int rnet_session_poll_sio_multi_xfer(RNetSession *s, rnet_u8 *unit_id, rnet_u32 *seq,
                                     rnet_u16 *send, rnet_u16 *confirm_pad);

/*
 * Rollback helpers over the delay-sync tip rings (no try_admit / confirm).
 * prepare_local_tip: sample+store local tip for sim_tick+D and emit INPUT.
 * peek_remote_input: non-destructive read of a remote ring row (wire tick).
 */
int rnet_session_prepare_local_tip(RNetSession *s, rnet_u32 sim_tick);
/* Non-destructive ring peek (local or remote) at a wire tick. */
int rnet_session_peek_input(const RNetSession *s, int slot, rnet_u32 wire_tick,
                            RNetInputSample *out);
int rnet_session_peek_remote_input(const RNetSession *s, int slot, rnet_u32 wire_tick,
                                   RNetInputSample *out);
/* §56 pipeline diagnostics: ms since the remote row for wire_tick arrived
 * (first-wins latch). 0xffffffff if unknown / not yet arrived. Consumption
 * slack at admit = this value; ~0 means rows arrive just-in-time (no cushion
 * in the time domain even when tick-domain lead looks healthy). */
rnet_u32 rnet_session_remote_arrival_age_ms(const RNetSession *s, int slot,
                                            rnet_u32 wire_tick);

/* Force session clock (rollback load / resim). Does not clear rings. */
void rnet_session_set_sim_tick(RNetSession *s, rnet_u32 sim_tick);

/* Clear remote tip rings (and peer-ack watermark). Prefer not to call on
 * Live realign — full wipe → tip=0 → mutual pcap FREEZE (MotK §49b). Kept
 * for hard_resync-style paths that intentionally re-prime. */
void rnet_session_clear_remote_inputs(RNetSession *s);

/*
 * Rollback episode wire (opcodes 20–23, 25). MotK drains via take_* after pump.
 * Seal-row take copies up to RNET_RB_SEAL_ROWS_CHUNK_MAX frames.
 *
 * RB_SYNC op codes (the wire byte historically called "initiator"):
 *   NACK  — follower refuses/cannot follow. target_tick carries the
 *           follower's confirmed frontier (0 = unknown) so the initiator can
 *           demote its watermark to a mutually provable tick.
 *   BEGIN — initiator opens or tip-extends an episode.
 *   ABORT — sender tore its episode down. mismatch_tick carries the shared
 *           RNET_RB_ABORT_CLASS_* cooldown class, load_tick the sender's
 *           realign tick (0 = none). Receiver mirrors teardown + cooldown so
 *           both peers re-arm on the same schedule.
 *   COMMIT — sender committed its episode (tip-hold or legacy) and left it.
 *           load_tick carries the committed tick, epoch_id the episode. This
 *           is distinct from RESOLVED: RESOLVED is a hash-confirm WATERMARK
 *           that also rises on live-play interval confirms, so it cannot
 *           prove "peer committed and left the episode" — conflating the two
 *           made the tip-extend abandon fire on stale watermarks and fork
 *           the cores (MotK 2026-08-02 soak, tick 768).
 */
#define RNET_RB_SYNC_OP_NACK 0u
#define RNET_RB_SYNC_OP_BEGIN 1u
#define RNET_RB_SYNC_OP_ABORT 2u
#define RNET_RB_SYNC_OP_COMMIT 3u

/* RB_SYNC flags (BEGIN): initiator-authoritative episode attributes the
 * follower must adopt verbatim so both peers run the same episode shape. */
#define RNET_RB_SYNC_FLAG_LIGHT_TIP 0x01u /* light-tip class (skip ready-ACK RTT) */
#define RNET_RB_SYNC_FLAG_REREPLAY 0x02u  /* tip-extend requires re-replay from load */
#define RNET_RB_SYNC_FLAG_MEDIA_KF 0x04u  /* FMV media keyframe episode (§97) */

/* Shared cooldown classes carried by ABORT (mismatch_tick field). */
#define RNET_RB_ABORT_CLASS_REALIGN 0u  /* clean realign heal: no cooldown */
#define RNET_RB_ABORT_CLASS_ABORT 1u    /* generic episode failure */
#define RNET_RB_ABORT_CLASS_STORM 2u    /* repeated aborts: long cooldown */
#define RNET_RB_ABORT_CLASS_NO_SNAP 3u  /* no usable snap: longest cooldown */

int rnet_session_send_rb_sync(RNetSession *s, rnet_u32 epoch_id, rnet_u32 mismatch_tick,
                              rnet_u32 load_tick, rnet_u32 target_tick,
                              rnet_u8 corrected_slot, rnet_u8 op, rnet_u8 flags);
int rnet_session_take_rb_sync(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *mismatch_tick,
                              rnet_u32 *load_tick, rnet_u32 *target_tick,
                              rnet_u8 *corrected_slot, rnet_u8 *op, rnet_u8 *flags);

int rnet_session_send_rb_seal_rows(RNetSession *s, rnet_u32 epoch_id, rnet_u32 mismatch_tick,
                                   rnet_u32 target_tick, rnet_u8 slot, rnet_u32 row_begin,
                                   const RNetRbFrame *rows, rnet_u16 row_count);
int rnet_session_take_rb_seal_rows(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *mismatch_tick,
                                   rnet_u32 *target_tick, rnet_u8 *slot, rnet_u32 *row_begin,
                                   RNetRbFrame *rows, rnet_u16 *row_count);

int rnet_session_send_rb_baseline(RNetSession *s, rnet_u32 epoch_id, rnet_u32 load_tick,
                                  rnet_u32 digest_master, rnet_u32 digest_a, rnet_u32 digest_b,
                                  rnet_u32 digest_c);
int rnet_session_take_rb_baseline(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *load_tick,
                                  rnet_u32 *digest_master, rnet_u32 *digest_a, rnet_u32 *digest_b,
                                  rnet_u32 *digest_c);

int rnet_session_send_rb_post(RNetSession *s, rnet_u32 epoch_id, rnet_u32 target_tick,
                              rnet_u32 digest_master, rnet_u32 input_digest, rnet_u8 match);
int rnet_session_take_rb_post(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *target_tick,
                              rnet_u32 *digest_master, rnet_u32 *input_digest, rnet_u8 *match);

int rnet_session_send_rb_resolved(RNetSession *s, rnet_u32 resolved_through);
int rnet_session_take_rb_resolved(RNetSession *s, rnet_u32 *resolved_through);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_SESSION_H */
