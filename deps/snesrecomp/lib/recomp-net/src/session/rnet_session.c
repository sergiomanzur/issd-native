#include "recomp_net/recomp_net.h"

#include "ice/rnet_ice_internal.h"
#include "input/rnet_rings.h"
#include "platform/rnet_platform.h"
#include "protocol/rnet_protocol.h"
#include "transport/rnet_transport.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *rnet_version_string(void)
{
    return "0.1.0";
}

rnet_u32 rnet_checksum(const void *data, size_t len)
{
    return rnet_proto_checksum((const rnet_u8 *)data, len);
}

typedef enum RNetSessionPhase
{
    RNET_PHASE_IDLE = 0,
    RNET_PHASE_LINKING,
    RNET_PHASE_READY,
    RNET_PHASE_RUNNING
} RNetSessionPhase;

struct RNetSession
{
    RNetConfig cfg;
    RNetHostVTable host;
    RNetTransport transport;
    RNetIceAgent *ice;
    RNetSessionPhase phase;
    RNetInputRing local_ring;
    RNetInputRing remote_rings[RNET_MAX_SLOTS];
    rnet_u8 peer_ready[RNET_MAX_SLOTS];
    rnet_u8 local_ready;
    rnet_u8 start_sent;
    rnet_u8 delay;
    /* Mid-session DELAY_SYNC: apply when sim_tick reaches effective_tick. */
    int delay_pending;
    rnet_u8 delay_pending_value;
    rnet_u32 delay_pending_effective;
    rnet_u64 delay_pending_last_tx_ms;
    rnet_u32 sim_tick;
    rnet_u32 highest_remote_ack;
    rnet_u64 last_hello_ms;
    rnet_u64 last_ready_ms;
    rnet_u64 last_input_ms;
    rnet_u32 last_input_tip;
    int last_input_tip_valid;
    int is_sim_authority; /* local_slot == 0 sends START */
    /* Resolved hashes and peer confirmations are prepared up
     * to D ticks ahead, so strict agreement normally completes before admit. */
    rnet_u32 published_tick[RNET_HISTORY_LENGTH];
    rnet_u32 published_hash[RNET_HISTORY_LENGTH];
    rnet_u8 published_valid[RNET_HISTORY_LENGTH];
    rnet_u32 peer_history_tick[RNET_HISTORY_LENGTH][RNET_MAX_SLOTS];
    rnet_u32 peer_history_hash[RNET_HISTORY_LENGTH][RNET_MAX_SLOTS];
    rnet_u8 peer_history_valid[RNET_HISTORY_LENGTH][RNET_MAX_SLOTS];
    rnet_u64 confirm_last_sent_ms[RNET_HISTORY_LENGTH];
    int input_desync;
    rnet_u32 desync_tick;
    rnet_u32 desync_local_hash;
    rnet_u32 desync_remote_hash;
    /* Peer liveness: any valid packet stamps last_peer_rx_ms; BYE sets peer_gone. */
    rnet_u64 last_peer_rx_ms;
    rnet_u64 session_start_ms;
    int peer_gone;
    /* Host→guest savestate / SRAM transfer (chunked + ACK). */
    int state_active;
    int state_sender;
    int state_ready;
    int state_stall_sim; /* probe + all transfers stall admit until finished */
    rnet_u8 state_op;
    rnet_u8 state_slot;
    rnet_u32 state_xfer_id;
    rnet_u32 state_next_xfer_id;
    rnet_u32 state_finished_xfer_id;
    rnet_u32 state_total;
    rnet_u32 state_crc;
    rnet_u32 state_contiguity; /* receiver: bytes from 0 received; sender: unused */
    rnet_u32 state_peer_ack;   /* sender: peer contiguous ACK */
    rnet_u32 state_send_cursor;
    rnet_u8 *state_buf;
    rnet_u8 state_rx_bits[(RNET_STATE_MAX_CHUNKS + 7u) / 8u];
    rnet_u64 state_last_tx_ms;
    rnet_u64 state_last_ack_ms;
    rnet_u64 state_last_begin_ms;
    rnet_u64 state_last_progress_log_ms;
    rnet_u32 state_last_progress_acked;
    /* AIMD pacing for STATE_CHUNK (esp. ICE/TURN — juice drops on flood). */
    rnet_u32 state_cwnd;        /* in-flight byte budget */
    rnet_u32 state_chunks_cap;  /* max chunks emitted per pump */
    rnet_u32 state_ack_timeout_ms;
    /* Survives state_clear — warm-start the next ICE transfer in-session. */
    rnet_u32 state_sticky_cwnd;
    rnet_u32 state_sticky_chunks;
    rnet_u64 state_xfer_start_ms;
    /* Hash probe before transfer (host announce → guest reply). */
    int state_probe_active;
    int state_probe_sender;
    int state_probe_reply_ready; /* host: guest answered */
    int state_probe_pending;     /* guest: awaiting app reply */
    int state_probe_match;
    rnet_u8 state_probe_op;
    rnet_u8 state_probe_slot;
    rnet_u32 state_probe_size;
    rnet_u32 state_probe_crc;
    rnet_u64 state_probe_last_tx_ms;
    /* When set, pump must not emit INPUT bundles. Used across LOAD apply/ready
     * so pre-resync tip rows cannot clobber the post-hard_resync epoch
     * (tick % RNET_HISTORY_LENGTH collisions). Cleared by prime_delay_inputs. */
    int input_send_suppress;
    /* Bumped on hard_resync. INPUT/CONFIRM carry this; other-epoch packets are
     * dropped so in-flight tips from a prior sim_tick=0 era cannot first-wins. */
    rnet_u16 input_epoch;
    /* Diagnostics (JSONL / HUD). */
    RNetAdmitStall last_stall;
    rnet_u32 consecutive_stalls;
    rnet_u32 admit_ok_count;
    rnet_u32 stall_streaks;
    rnet_u64 stall_started_ms;
    rnet_u32 last_admit_wait_ms;
    rnet_u32 max_admit_wait_ms;
    rnet_u32 packets_rx;
    rnet_u32 input_bundle_sends;
    /* §56 pipeline diagnostics: monotonic arrival stamp per remote wire row
     * (first-wins, mirrors remote_rings latching). Consumption slack =
     * now - arrival when the row is finally needed at admit. */
    rnet_u64 remote_arr_ms[RNET_MAX_SLOTS][RNET_HISTORY_LENGTH];
    rnet_u32 remote_arr_tick[RNET_MAX_SLOTS][RNET_HISTORY_LENGTH];
    /* ICE TURN auto-fallback timers (monotonic ms). */
    rnet_u64 ice_attempt_ms;
    rnet_u64 ice_completed_ms;
    /* Peer RB_FRAME_COMMIT queue (host drains via take_*). */
#define RNET_RB_FC_QUEUE 64
    /* PSX-Link group scoping: when >= 0, rollback EPISODE/STATE packets
     * (SYNC, BASELINE, POST, STATE_*) are accepted only from this slot —
     * episode coordination is per-console-group in link sessions. FRAME_COMMIT
     * is NOT filtered (all seats emit the same machine-level pair fold), and
     * input-plane packets (INPUT, INPUT_CONFIRM, SEAL_ROWS, RESOLVED) are
     * session-wide. -1 = accept all (default). */
    int rb_peer_slot;
    rnet_u32 rb_fc_tick[RNET_RB_FC_QUEUE];
    rnet_u32 rb_fc_hash[RNET_RB_FC_QUEUE];
    int rb_fc_q_head; /* next write */
    int rb_fc_q_tail; /* next read */
    int rb_fc_q_count;

    /* Peer RB episode control queues (latest-wins / small FIFO). */
#define RNET_RB_CTRL_QUEUE 8
    struct {
        rnet_u32 epoch_id, mismatch_tick, load_tick, target_tick;
        rnet_u8 corrected_slot, initiator, flags;
    } rb_sync_q[RNET_RB_CTRL_QUEUE];
    int rb_sync_head, rb_sync_tail, rb_sync_count;

    struct {
        rnet_u32 epoch_id, mismatch_tick, target_tick, row_begin;
        rnet_u8 slot;
        rnet_u16 row_count;
        RNetRbWireFrame rows[RNET_RB_SEAL_ROWS_CHUNK_MAX];
    } rb_seal_q[RNET_RB_CTRL_QUEUE];
    int rb_seal_head, rb_seal_tail, rb_seal_count;

    struct {
        rnet_u32 epoch_id, load_tick, digest_master, digest_a, digest_b, digest_c;
    } rb_base_q[RNET_RB_CTRL_QUEUE];
    int rb_base_head, rb_base_tail, rb_base_count;

    struct {
        rnet_u32 epoch_id, target_tick, digest_master, input_digest;
        rnet_u8 match;
    } rb_post_q[RNET_RB_CTRL_QUEUE];
    int rb_post_head, rb_post_tail, rb_post_count;

    rnet_u32 rb_resolved_q[RNET_RB_CTRL_QUEUE];
    int rb_resolved_head, rb_resolved_tail, rb_resolved_count;

    /* Peer GBA Multi SEND barrier (0-delay; not pad INPUT). */
#define RNET_SIO_XFER_QUEUE 32
    struct {
        rnet_u32 seq;
        rnet_u16 send;
        rnet_u16 confirm_pad; /* lo=confirm IRQ, hi=vblank mod 256 */
        rnet_u8 unit_id;
    } sio_xfer_q[RNET_SIO_XFER_QUEUE];
    int sio_xfer_head, sio_xfer_tail, sio_xfer_count;
};

static rnet_u64 session_now(RNetSession *s)
{
    if (s->host.now_ms != NULL)
    {
        return s->host.now_ms(s->host.ctx);
    }
    return rnet_os_monotonic_ms();
}

const char *rnet_admit_stall_name(RNetAdmitStall reason)
{
    switch (reason) {
    case RNET_ADMIT_OK: return "ok";
    case RNET_ADMIT_NOT_RUNNING: return "not_running";
    case RNET_ADMIT_STATE_XFER: return "state_xfer";
    case RNET_ADMIT_SIM_MISMATCH: return "sim_mismatch";
    case RNET_ADMIT_DESYNC: return "desync";
    case RNET_ADMIT_WAIT_LOCAL_INPUT: return "wait_local_input";
    case RNET_ADMIT_WAIT_REMOTE_INPUT: return "wait_remote_input";
    case RNET_ADMIT_WAIT_CONFIRM: return "wait_confirm";
    default: return "unknown";
    }
}

static void note_admit_stall(RNetSession *s, RNetAdmitStall reason)
{
    rnet_u64 now;
    if (s == NULL)
        return;
    s->last_stall = reason;
    if (s->consecutive_stalls == 0)
        s->stall_streaks++;
    s->consecutive_stalls++;
    now = session_now(s);
    if (s->stall_started_ms == 0)
        s->stall_started_ms = now ? now : 1;
    s->last_admit_wait_ms = (rnet_u32)(now - s->stall_started_ms);
    if (s->last_admit_wait_ms > s->max_admit_wait_ms)
        s->max_admit_wait_ms = s->last_admit_wait_ms;
}

static void note_admit_ok(RNetSession *s)
{
    rnet_u64 now;
    if (s == NULL)
        return;
    now = session_now(s);
    if (s->stall_started_ms != 0) {
        s->last_admit_wait_ms = (rnet_u32)(now - s->stall_started_ms);
        if (s->last_admit_wait_ms > s->max_admit_wait_ms)
            s->max_admit_wait_ms = s->last_admit_wait_ms;
        s->stall_started_ms = 0;
    } else {
        s->last_admit_wait_ms = 0;
    }
    s->last_stall = RNET_ADMIT_OK;
    s->consecutive_stalls = 0;
    s->admit_ok_count++;
}

static void send_raw(RNetSession *s, const rnet_u8 *buf, int len);
static void send_input_bundle(RNetSession *s);
static void apply_pending_delay(RNetSession *s);
static void emit_delay_sync(RNetSession *s, rnet_u8 new_delay, rnet_u32 effective_tick);
static void seed_delay_prefix(RNetSession *s);
static void state_clear(RNetSession *s);
static void state_probe_clear(RNetSession *s);
static void state_send_ack(RNetSession *s);
static void state_drive_sender(RNetSession *s);
static void state_drive_probe(RNetSession *s);
static void state_on_begin(RNetSession *s, const RNetDecodedPacket *pkt);
static void state_on_chunk(RNetSession *s, const RNetDecodedPacket *pkt);
static void state_on_ack(RNetSession *s, const RNetDecodedPacket *pkt);
static void state_on_probe(RNetSession *s, const RNetDecodedPacket *pkt);
static void state_on_probe_reply(RNetSession *s, const RNetDecodedPacket *pkt);

#if defined(RNET_ENABLE_ICE)
static void ice_emit_bridge(const RNetSignal *msg, void *user)
{
    RNetSession *s = (RNetSession *)user;
    if ((s != NULL) && (s->host.on_signal != NULL) && (msg != NULL))
    {
        s->host.on_signal(msg, s->host.ctx);
    }
}

static int ice_send_bridge(void *ice_ctx, const rnet_u8 *buf, size_t len)
{
    return rnet_ice_agent_send((RNetIceAgent *)ice_ctx, buf, len);
}

static int ice_recv_bridge(void *ice_ctx, rnet_u8 *buf, size_t cap, size_t *out_len)
{
    return rnet_ice_agent_recv((RNetIceAgent *)ice_ctx, buf, cap, out_len);
}
#endif /* RNET_ENABLE_ICE */

static int remote_tick_in_live_window(const RNetSession *s, rnet_u8 slot, rnet_u32 tick)
{
    rnet_u32 tip;
    rnet_u32 slop;
    rnet_u32 lo;
    rnet_u32 hi;
    rnet_u32 remote_hi;
    rnet_u32 ancient;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
    {
        return 1;
    }
    tip = rnet_wire_tick_from_sim(s->sim_tick, s->delay);
    slop = (rnet_u32)s->cfg.bundle_redundancy + 8u;
    if (slop < 8u)
    {
        slop = 8u;
    }
    lo = (s->sim_tick > slop) ? (s->sim_tick - slop) : 0u;
    hi = tip + slop;
    if (tick >= lo && tick <= hi)
    {
        return 1;
    }
    /* Rematch / asymmetric boot: the faster peer invents up to P ahead, then
     * pcap_freeze. By then lo = sim-slop can sit above the stalled peer tip
     * (e.g. remote=5, sim=15, lo=7) so tip+1 is dropped forever and freeze
     * never clears. Accept gap-filling tips that extend the remote watermark
     * even when below lo; still reject ancient hard_resync residue. */
    if (slot >= s->cfg.slot_count)
    {
        return 0;
    }
    remote_hi = rnet_ring_highest_valid(&s->remote_rings[slot]);
    ancient = 64u;
    if (tick > remote_hi && tick <= hi &&
        (s->sim_tick <= ancient || tick + ancient >= s->sim_tick))
    {
        return 1;
    }
    return 0;
}

static void store_remote_frame(RNetSession *s, rnet_u8 slot, const RNetWireFrame *frame)
{
    RNetInputSample sample;
    RNetInputSample existing;
    if ((s == NULL) || (frame == NULL) || (slot >= s->cfg.slot_count) || (slot == s->cfg.local_slot))
    {
        return;
    }
    /* Drop previous-epoch residue after hard_resync (sim_tick→0). Those ticks
     * share ring slots with the new tip via tick%HISTORY and first-wins would
     * otherwise keep remotes_ready_for_sim failing until the peer stops. */
    if (!remote_tick_in_live_window(s, slot, frame->tick))
    {
        return;
    }
    /* First-wins: later retransmits must not overwrite a latched wire row. */
    if (rnet_ring_get(&s->remote_rings[slot], frame->tick, &existing))
    {
        return;
    }
    memset(&sample, 0, sizeof(sample));
    sample.tick = frame->tick;
    sample.size = frame->size;
    if (frame->size > 0)
    {
        memcpy(sample.bytes, frame->bytes, frame->size);
    }
    sample.valid = 1;
    rnet_ring_store(&s->remote_rings[slot], &sample);
    {
        rnet_u32 idx = frame->tick % RNET_HISTORY_LENGTH;
        rnet_u64 now = session_now(s);
        s->remote_arr_tick[slot][idx] = frame->tick;
        s->remote_arr_ms[slot][idx] = now ? now : 1u;
    }
}

static rnet_u32 hash_resolved_inputs(rnet_u32 sim_tick, const RNetInputSample *by_slot, int slots)
{
    rnet_u8 buf[4 + RNET_MAX_SLOTS * (2 + RNET_INPUT_MAX)];
    size_t n = 0;
    int i;

    buf[n++] = (rnet_u8)(sim_tick & 0xFFu);
    buf[n++] = (rnet_u8)((sim_tick >> 8) & 0xFFu);
    buf[n++] = (rnet_u8)((sim_tick >> 16) & 0xFFu);
    buf[n++] = (rnet_u8)((sim_tick >> 24) & 0xFFu);
    for (i = 0; i < slots; ++i)
    {
        rnet_u16 sz = by_slot[i].size;
        if (sz > RNET_INPUT_MAX)
        {
            sz = RNET_INPUT_MAX;
        }
        buf[n++] = (rnet_u8)(sz & 0xFFu);
        buf[n++] = (rnet_u8)((sz >> 8) & 0xFFu);
        if (sz > 0)
        {
            memcpy(buf + n, by_slot[i].bytes, sz);
            n += sz;
        }
    }
    return rnet_proto_checksum(buf, n);
}

static void send_input_confirm_tick(RNetSession *s, rnet_u32 tick,
                                    rnet_u32 hash)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int len;
    rnet_u64 now;

    if (s == NULL) return;
    len = rnet_proto_encode_input_confirm(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot,
                                          s->input_epoch, tick, hash);
    send_raw(s, buf, len);
    now = session_now(s);
    s->confirm_last_sent_ms[tick % RNET_HISTORY_LENGTH] = now;
}

static void handle_decoded(RNetSession *s, const RNetDecodedPacket *pkt)
{
    int i;
    if ((s == NULL) || (pkt == NULL))
    {
        return;
    }
    if (pkt->session_id != s->cfg.session_id)
    {
        return;
    }
    switch (pkt->type)
    {
    case RNET_PKT_HELLO:
        if (pkt->slot_count != s->cfg.slot_count)
        {
            break;
        }
        /* Peer is alive; move toward READY once we have exchanged HELLO. */
        if (s->phase == RNET_PHASE_LINKING)
        {
            s->phase = RNET_PHASE_READY;
        }
        /* Guest adopts host delay before RUNNING (host slot 0 is authoritative). */
        if (s->cfg.local_slot != 0 && pkt->local_slot == 0)
        {
            rnet_u8 hello_delay = pkt->delay;
            if (hello_delay >= 2 && hello_delay <= 20 && hello_delay != s->delay)
            {
                if (s->phase != RNET_PHASE_RUNNING || s->sim_tick == 0)
                {
                    s->delay = hello_delay;
                    s->cfg.input_delay = hello_delay;
                    if (s->phase != RNET_PHASE_RUNNING)
                        seed_delay_prefix(s);
                }
                else if (s->sim_tick > 0)
                {
                    static int delay_mismatch_logged;
                    if (!delay_mismatch_logged)
                    {
                        fprintf(stderr,
                                "recomp-net: HELLO delay mismatch local=%u peer=%u "
                                "after RUNNING — not adopting\n",
                                (unsigned)s->delay, (unsigned)hello_delay);
                        delay_mismatch_logged = 1;
                    }
                }
            }
        }
        break;
    case RNET_PKT_READY:
        if (pkt->local_slot < s->cfg.slot_count)
        {
            s->peer_ready[pkt->local_slot] = 1;
        }
        break;
    case RNET_PKT_START:
        if (s->phase != RNET_PHASE_RUNNING)
        {
            s->sim_tick = pkt->start_tick;
            s->phase = RNET_PHASE_RUNNING;
            seed_delay_prefix(s);
            send_input_bundle(s);
        }
        break;
    case RNET_PKT_INPUT:
        /* Drop prior-epoch tips (rapid rematch hard_resync → sim=0). */
        if (pkt->input_epoch != s->input_epoch)
        {
            break;
        }
        for (i = 0; i < pkt->frame_count; ++i)
        {
            store_remote_frame(s, pkt->local_slot, &pkt->frames[i]);
        }
        if (pkt->ack_tick > s->highest_remote_ack)
        {
            s->highest_remote_ack = pkt->ack_tick;
        }
        break;
    case RNET_PKT_DELAY_SYNC:
        /* Immediate when already past effective_tick (or not RUNNING yet).
         * Otherwise queue so both peers commit on the same sim tick. */
        if (pkt->effective_tick <= s->sim_tick || s->phase != RNET_PHASE_RUNNING)
        {
            s->delay = pkt->new_delay;
            s->cfg.input_delay = pkt->new_delay;
            s->delay_pending = 0;
        }
        else
        {
            s->delay_pending = 1;
            s->delay_pending_value = pkt->new_delay;
            s->delay_pending_effective = pkt->effective_tick;
        }
        break;
    case RNET_PKT_INPUT_CONFIRM:
        if (pkt->input_epoch != s->input_epoch)
        {
            break;
        }
        if (pkt->local_slot < s->cfg.slot_count)
        {
            rnet_u32 index = pkt->confirm_sim_tick % RNET_HISTORY_LENGTH;
            s->peer_history_tick[index][pkt->local_slot] = pkt->confirm_sim_tick;
            s->peer_history_hash[index][pkt->local_slot] = pkt->confirm_hash;
            s->peer_history_valid[index][pkt->local_slot] = 1;
            if (s->published_valid[index] &&
                s->published_tick[index] == pkt->confirm_sim_tick &&
                s->published_hash[index] != pkt->confirm_hash)
            {
                s->input_desync = 1;
                s->desync_tick = pkt->confirm_sim_tick;
                s->desync_local_hash = s->published_hash[index];
                s->desync_remote_hash = pkt->confirm_hash;
            }
        }
        break;
    case RNET_PKT_BYE:
        if (pkt->local_slot != s->cfg.local_slot)
        {
            s->peer_gone = 1;
        }
        break;
    case RNET_PKT_STATE_BEGIN:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        state_on_begin(s, pkt);
        break;
    case RNET_PKT_STATE_CHUNK:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        state_on_chunk(s, pkt);
        break;
    case RNET_PKT_STATE_ACK:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        state_on_ack(s, pkt);
        break;
    case RNET_PKT_STATE_PROBE:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        state_on_probe(s, pkt);
        break;
    case RNET_PKT_STATE_PROBE_REPLY:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        state_on_probe_reply(s, pkt);
        break;
    case RNET_PKT_SIO_MULTI_XFER:
        if (pkt->local_slot != s->cfg.local_slot)
        {
            rnet_u16 pad = (rnet_u16)(pkt->sio_confirm |
                                      ((rnet_u16)pkt->sio_vblank << 8));
            if (s->sio_xfer_count < RNET_SIO_XFER_QUEUE)
            {
                s->sio_xfer_q[s->sio_xfer_head].seq = pkt->sio_xfer_seq;
                s->sio_xfer_q[s->sio_xfer_head].send = pkt->sio_send;
                s->sio_xfer_q[s->sio_xfer_head].unit_id = pkt->sio_unit_id;
                s->sio_xfer_q[s->sio_xfer_head].confirm_pad = pad;
                s->sio_xfer_head = (s->sio_xfer_head + 1) % RNET_SIO_XFER_QUEUE;
                s->sio_xfer_count++;
            }
            else
            {
                /* Drop oldest so a live Cable Club burst can still progress. */
                s->sio_xfer_tail = (s->sio_xfer_tail + 1) % RNET_SIO_XFER_QUEUE;
                s->sio_xfer_count--;
                s->sio_xfer_q[s->sio_xfer_head].seq = pkt->sio_xfer_seq;
                s->sio_xfer_q[s->sio_xfer_head].send = pkt->sio_send;
                s->sio_xfer_q[s->sio_xfer_head].unit_id = pkt->sio_unit_id;
                s->sio_xfer_q[s->sio_xfer_head].confirm_pad = pad;
                s->sio_xfer_head = (s->sio_xfer_head + 1) % RNET_SIO_XFER_QUEUE;
                s->sio_xfer_count++;
            }
        }
        break;
    case RNET_PKT_RB_FRAME_COMMIT:
        /* Not rb_peer_slot-filtered: PSX-Link seats all emit the same
         * machine-level pair fold, so every peer's commit is comparable. */
        if (pkt->local_slot != s->cfg.local_slot)
        {
            if (s->rb_fc_q_count < RNET_RB_FC_QUEUE)
            {
                s->rb_fc_tick[s->rb_fc_q_head] = pkt->rb_through_tick;
                s->rb_fc_hash[s->rb_fc_q_head] = pkt->rb_state_hash;
                s->rb_fc_q_head = (s->rb_fc_q_head + 1) % RNET_RB_FC_QUEUE;
                s->rb_fc_q_count++;
            }
            else
            {
                /* Drop oldest so the watermark can still track the tip. */
                s->rb_fc_q_tail = (s->rb_fc_q_tail + 1) % RNET_RB_FC_QUEUE;
                s->rb_fc_q_count--;
                s->rb_fc_tick[s->rb_fc_q_head] = pkt->rb_through_tick;
                s->rb_fc_hash[s->rb_fc_q_head] = pkt->rb_state_hash;
                s->rb_fc_q_head = (s->rb_fc_q_head + 1) % RNET_RB_FC_QUEUE;
                s->rb_fc_q_count++;
            }
        }
        break;
    case RNET_PKT_RB_SYNC:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        if (pkt->local_slot != s->cfg.local_slot &&
            s->rb_sync_count < RNET_RB_CTRL_QUEUE)
        {
            s->rb_sync_q[s->rb_sync_head].epoch_id = pkt->rb_epoch_id;
            s->rb_sync_q[s->rb_sync_head].mismatch_tick = pkt->rb_mismatch_tick;
            s->rb_sync_q[s->rb_sync_head].load_tick = pkt->rb_load_tick;
            s->rb_sync_q[s->rb_sync_head].target_tick = pkt->rb_target_tick;
            s->rb_sync_q[s->rb_sync_head].corrected_slot = pkt->rb_corrected_slot;
            s->rb_sync_q[s->rb_sync_head].initiator = pkt->rb_initiator;
            s->rb_sync_q[s->rb_sync_head].flags = pkt->rb_flags;
            s->rb_sync_head = (s->rb_sync_head + 1) % RNET_RB_CTRL_QUEUE;
            s->rb_sync_count++;
        }
        break;
    case RNET_PKT_RB_SEAL_ROWS:
        if (pkt->local_slot != s->cfg.local_slot &&
            s->rb_seal_count < RNET_RB_CTRL_QUEUE)
        {
            s->rb_seal_q[s->rb_seal_head].epoch_id = pkt->rb_epoch_id;
            s->rb_seal_q[s->rb_seal_head].mismatch_tick = pkt->rb_mismatch_tick;
            s->rb_seal_q[s->rb_seal_head].target_tick = pkt->rb_target_tick;
            s->rb_seal_q[s->rb_seal_head].row_begin = pkt->rb_row_begin;
            s->rb_seal_q[s->rb_seal_head].slot = pkt->rb_slot;
            s->rb_seal_q[s->rb_seal_head].row_count = pkt->rb_row_count;
            if (pkt->rb_row_count > 0)
            {
                rnet_u16 n = pkt->rb_row_count;
                if (n > RNET_RB_SEAL_ROWS_CHUNK_MAX)
                    n = RNET_RB_SEAL_ROWS_CHUNK_MAX;
                memcpy(s->rb_seal_q[s->rb_seal_head].rows, pkt->rb_rows,
                       sizeof(RNetRbWireFrame) * n);
                s->rb_seal_q[s->rb_seal_head].row_count = n;
            }
            s->rb_seal_head = (s->rb_seal_head + 1) % RNET_RB_CTRL_QUEUE;
            s->rb_seal_count++;
        }
        break;
    case RNET_PKT_RB_BASELINE:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        if (pkt->local_slot != s->cfg.local_slot &&
            s->rb_base_count < RNET_RB_CTRL_QUEUE)
        {
            s->rb_base_q[s->rb_base_head].epoch_id = pkt->rb_epoch_id;
            s->rb_base_q[s->rb_base_head].load_tick = pkt->rb_load_tick;
            s->rb_base_q[s->rb_base_head].digest_master = pkt->rb_digest_master;
            s->rb_base_q[s->rb_base_head].digest_a = pkt->rb_digest_a;
            s->rb_base_q[s->rb_base_head].digest_b = pkt->rb_digest_b;
            s->rb_base_q[s->rb_base_head].digest_c = pkt->rb_digest_c;
            s->rb_base_head = (s->rb_base_head + 1) % RNET_RB_CTRL_QUEUE;
            s->rb_base_count++;
        }
        break;
    case RNET_PKT_RB_POST:
        if (s->rb_peer_slot >= 0 && (int)pkt->local_slot != s->rb_peer_slot)
        {
            break;
        }
        if (pkt->local_slot != s->cfg.local_slot &&
            s->rb_post_count < RNET_RB_CTRL_QUEUE)
        {
            s->rb_post_q[s->rb_post_head].epoch_id = pkt->rb_epoch_id;
            s->rb_post_q[s->rb_post_head].target_tick = pkt->rb_target_tick;
            s->rb_post_q[s->rb_post_head].digest_master = pkt->rb_digest_master;
            s->rb_post_q[s->rb_post_head].input_digest = pkt->rb_input_digest;
            s->rb_post_q[s->rb_post_head].match = pkt->rb_match;
            s->rb_post_head = (s->rb_post_head + 1) % RNET_RB_CTRL_QUEUE;
            s->rb_post_count++;
        }
        break;
    case RNET_PKT_RB_RESOLVED:
        if (pkt->local_slot != s->cfg.local_slot &&
            s->rb_resolved_count < RNET_RB_CTRL_QUEUE)
        {
            s->rb_resolved_q[s->rb_resolved_head] = pkt->rb_resolved_through;
            s->rb_resolved_head = (s->rb_resolved_head + 1) % RNET_RB_CTRL_QUEUE;
            s->rb_resolved_count++;
        }
        break;
    default:
        break;
    }
}

static void send_raw(RNetSession *s, const rnet_u8 *buf, int len)
{
    if ((s == NULL) || (buf == NULL) || (len <= 0))
    {
        return;
    }
    (void)rnet_transport_send(&s->transport, buf, (size_t)len);
}

static void pump_recv(RNetSession *s)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    RNetDecodedPacket pkt;
    int guard = s->state_active ? 512 : 64;

    while (guard-- > 0)
    {
        n = rnet_transport_recv(&s->transport, buf, sizeof(buf));
        if (n <= 0)
        {
            break;
        }
        if (rnet_proto_decode(buf, (size_t)n, s->cfg.protocol_magic, &pkt) == 0 &&
            pkt.session_id == s->cfg.session_id)
        {
            rnet_transport_accept_pending_peer(&s->transport);
            s->last_peer_rx_ms = session_now(s);
            s->packets_rx++;
            handle_decoded(s, &pkt);
        }
    }
}

static void state_probe_clear(RNetSession *s)
{
    if (s == NULL)
    {
        return;
    }
    s->state_probe_active = 0;
    s->state_probe_sender = 0;
    s->state_probe_reply_ready = 0;
    s->state_probe_pending = 0;
    s->state_probe_match = 0;
    s->state_probe_op = 0;
    s->state_probe_slot = 0;
    s->state_probe_size = 0;
    s->state_probe_crc = 0;
    s->state_probe_last_tx_ms = 0;
    if (!s->state_active)
    {
        s->state_stall_sim = 0;
    }
}

/* ICE AIMD for multi‑MB MotK .pst over TURN. Cold-start used to be 8 KiB / 4
 * chunks (far too shy); AIMD still backs off when juice drops. LAN keeps a
 * wide fixed budget. Sticky cwnd warm-starts the next in-session transfer. */
#define RNET_STATE_ICE_CWND_MIN        (8u * 1024u)
#define RNET_STATE_ICE_CWND_START      (32u * 1024u)
#define RNET_STATE_ICE_CWND_MAX        (256u * 1024u)
#define RNET_STATE_ICE_CHUNKS_MIN      4u
#define RNET_STATE_ICE_CHUNKS_START    16u
#define RNET_STATE_ICE_CHUNKS_MAX      64u
#define RNET_STATE_ICE_AI_CWND         (8u * 1024u)
#define RNET_STATE_ICE_AI_CHUNKS       2u
#define RNET_STATE_ICE_ACK_TO_MIN_MS   40u
#define RNET_STATE_ICE_ACK_TO_START_MS 80u
#define RNET_STATE_ICE_ACK_TO_MAX_MS   180u
#define RNET_STATE_LAN_CWND            (256u * 1024u)
#define RNET_STATE_LAN_CHUNKS          128u
#define RNET_STATE_LAN_ACK_TO_MS       40u

static int state_transport_is_ice(const RNetSession *s)
{
    return s != NULL && s->transport.mode == RNET_TRANSPORT_ICE;
}

static void state_pacing_reset(RNetSession *s)
{
    if (s == NULL)
        return;
    if (state_transport_is_ice(s))
    {
        rnet_u32 cwnd = RNET_STATE_ICE_CWND_START;
        rnet_u32 chunks = RNET_STATE_ICE_CHUNKS_START;
        if (s->state_sticky_cwnd >= RNET_STATE_ICE_CWND_START)
        {
            cwnd = s->state_sticky_cwnd;
            if (cwnd > RNET_STATE_ICE_CWND_MAX)
                cwnd = RNET_STATE_ICE_CWND_MAX;
        }
        if (s->state_sticky_chunks >= RNET_STATE_ICE_CHUNKS_START)
        {
            chunks = s->state_sticky_chunks;
            if (chunks > RNET_STATE_ICE_CHUNKS_MAX)
                chunks = RNET_STATE_ICE_CHUNKS_MAX;
        }
        s->state_cwnd = cwnd;
        s->state_chunks_cap = chunks;
        s->state_ack_timeout_ms = RNET_STATE_ICE_ACK_TO_START_MS;
    }
    else
    {
        s->state_cwnd = RNET_STATE_LAN_CWND;
        s->state_chunks_cap = RNET_STATE_LAN_CHUNKS;
        s->state_ack_timeout_ms = RNET_STATE_LAN_ACK_TO_MS;
    }
}

static void state_pacing_on_ack_progress(RNetSession *s)
{
    if (s == NULL || !state_transport_is_ice(s))
        return;
    /* Additive increase: +8 KiB and +2 chunks/pump per advancing ACK. */
    if (s->state_cwnd < RNET_STATE_ICE_CWND_MAX)
    {
        s->state_cwnd += RNET_STATE_ICE_AI_CWND;
        if (s->state_cwnd > RNET_STATE_ICE_CWND_MAX)
            s->state_cwnd = RNET_STATE_ICE_CWND_MAX;
    }
    if (s->state_chunks_cap < RNET_STATE_ICE_CHUNKS_MAX)
    {
        s->state_chunks_cap += RNET_STATE_ICE_AI_CHUNKS;
        if (s->state_chunks_cap > RNET_STATE_ICE_CHUNKS_MAX)
            s->state_chunks_cap = RNET_STATE_ICE_CHUNKS_MAX;
    }
    if (s->state_ack_timeout_ms > RNET_STATE_ICE_ACK_TO_MIN_MS)
    {
        s->state_ack_timeout_ms -= 5u;
        if (s->state_ack_timeout_ms < RNET_STATE_ICE_ACK_TO_MIN_MS)
            s->state_ack_timeout_ms = RNET_STATE_ICE_ACK_TO_MIN_MS;
    }
}

static void state_pacing_on_timeout(RNetSession *s)
{
    if (s == NULL || !state_transport_is_ice(s))
        return;
    /* Multiplicative decrease — juice/TURN drop when we outrun the relay. */
    s->state_cwnd /= 2u;
    if (s->state_cwnd < RNET_STATE_ICE_CWND_MIN)
        s->state_cwnd = RNET_STATE_ICE_CWND_MIN;
    s->state_chunks_cap /= 2u;
    if (s->state_chunks_cap < RNET_STATE_ICE_CHUNKS_MIN)
        s->state_chunks_cap = RNET_STATE_ICE_CHUNKS_MIN;
    s->state_ack_timeout_ms += 20u;
    if (s->state_ack_timeout_ms > RNET_STATE_ICE_ACK_TO_MAX_MS)
        s->state_ack_timeout_ms = RNET_STATE_ICE_ACK_TO_MAX_MS;
    /* Decay sticky so the next transfer does not restart at a failed peak. */
    if (s->state_sticky_cwnd > s->state_cwnd)
        s->state_sticky_cwnd = s->state_cwnd;
    if (s->state_sticky_chunks > s->state_chunks_cap)
        s->state_sticky_chunks = s->state_chunks_cap;
}

static void state_pacing_remember_success(RNetSession *s)
{
    if (s == NULL || !state_transport_is_ice(s))
        return;
    if (s->state_cwnd > s->state_sticky_cwnd)
        s->state_sticky_cwnd = s->state_cwnd;
    if (s->state_chunks_cap > s->state_sticky_chunks)
        s->state_sticky_chunks = s->state_chunks_cap;
}

static void state_clear(RNetSession *s)
{
    if (s == NULL)
    {
        return;
    }
    free(s->state_buf);
    s->state_buf = NULL;
    s->state_active = 0;
    s->state_sender = 0;
    s->state_ready = 0;
    s->state_stall_sim = s->state_probe_active ? 1 : 0;
    s->state_op = 0;
    s->state_slot = 0;
    s->state_xfer_id = 0;
    s->state_total = 0;
    s->state_crc = 0;
    s->state_contiguity = 0;
    s->state_peer_ack = 0;
    s->state_send_cursor = 0;
    memset(s->state_rx_bits, 0, sizeof(s->state_rx_bits));
    s->state_last_tx_ms = 0;
    s->state_last_ack_ms = 0;
    s->state_last_begin_ms = 0;
    s->state_last_progress_log_ms = 0;
    s->state_last_progress_acked = 0;
    s->state_xfer_start_ms = 0;
    s->state_cwnd = 0;
    s->state_chunks_cap = 0;
    s->state_ack_timeout_ms = 0;
}

static void state_rx_set_chunk(RNetSession *s, rnet_u32 chunk_index)
{
    if (chunk_index >= RNET_STATE_MAX_CHUNKS)
    {
        return;
    }
    s->state_rx_bits[chunk_index >> 3] |= (rnet_u8)(1u << (chunk_index & 7u));
}

static int state_rx_has_chunk(const RNetSession *s, rnet_u32 chunk_index)
{
    if (chunk_index >= RNET_STATE_MAX_CHUNKS)
    {
        return 0;
    }
    return (s->state_rx_bits[chunk_index >> 3] >> (chunk_index & 7u)) & 1;
}

static void state_rx_advance_contiguity(RNetSession *s)
{
    rnet_u32 chunks = (s->state_total + RNET_STATE_CHUNK_MAX - 1u) / RNET_STATE_CHUNK_MAX;
    rnet_u32 i = s->state_contiguity / RNET_STATE_CHUNK_MAX;
    while (i < chunks && state_rx_has_chunk(s, i))
    {
        rnet_u32 end = (i + 1u) * RNET_STATE_CHUNK_MAX;
        if (end > s->state_total)
        {
            end = s->state_total;
        }
        s->state_contiguity = end;
        i++;
    }
}

static void state_send_ack(RNetSession *s)
{
    rnet_u8 buf[64];
    int n;
    n = rnet_proto_encode_state_ack(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot,
                                    s->state_xfer_id, s->state_contiguity);
    if (n > 0)
    {
        send_raw(s, buf, n);
        s->state_last_ack_ms = session_now(s);
    }
}

static void state_mark_ready_if_complete(RNetSession *s)
{
    rnet_u32 crc;
    if (!s->state_active || s->state_ready || s->state_buf == NULL)
    {
        return;
    }
    if (s->state_sender)
    {
        if (s->state_peer_ack >= s->state_total)
        {
            s->state_ready = 1;
            state_pacing_remember_success(s);
        }
        return;
    }
    if (s->state_contiguity < s->state_total)
    {
        return;
    }
    crc = rnet_proto_checksum(s->state_buf, s->state_total);
    if (crc != s->state_crc)
    {
        /* Restart receive — ask host to resend from 0 by ACKing 0 after clear. */
        s->state_contiguity = 0;
        state_send_ack(s);
        return;
    }
    s->state_ready = 1;
    state_send_ack(s);
}

static void state_drive_sender(RNetSession *s)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    rnet_u64 now;
    rnet_u32 window_end;
    rnet_u32 sent_this_pump = 0;
    const int is_ice = state_transport_is_ice(s);
    const rnet_u64 kBeginRetransmitMs = is_ice ? 80ULL : 40ULL;
    rnet_u32 cwnd;
    rnet_u32 chunks_cap;
    rnet_u64 ack_timeout_ms;

    if (!s->state_active || !s->state_sender || s->state_ready || s->state_buf == NULL)
    {
        return;
    }
    if (s->state_cwnd == 0 || s->state_chunks_cap == 0)
        state_pacing_reset(s);
    cwnd = s->state_cwnd;
    chunks_cap = s->state_chunks_cap;
    ack_timeout_ms = (rnet_u64)s->state_ack_timeout_ms;
    if (ack_timeout_ms == 0)
        ack_timeout_ms = is_ice ? (rnet_u64)RNET_STATE_ICE_ACK_TO_START_MS
                                : (rnet_u64)RNET_STATE_LAN_ACK_TO_MS;

    now = session_now(s);
    /* Retransmit BEGIN until the peer has ACKed past 0 (saw BEGIN). */
    if (s->state_peer_ack == 0 &&
        (s->state_last_begin_ms == 0 || now - s->state_last_begin_ms >= kBeginRetransmitMs))
    {
        n = rnet_proto_encode_state_begin(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot,
                                          s->state_op, s->state_slot, s->state_xfer_id, s->state_total, s->state_crc);
        if (n > 0)
        {
            send_raw(s, buf, n);
        }
        s->state_last_begin_ms = now;
    }

    if (s->state_peer_ack >= s->state_total)
    {
        state_mark_ready_if_complete(s);
        return;
    }

    /* On ACK timeout, rewind send cursor to peer_ack for retransmission. */
    if (s->state_last_ack_ms != 0 && now - s->state_last_ack_ms >= ack_timeout_ms)
    {
        s->state_send_cursor = s->state_peer_ack;
        s->state_last_ack_ms = now; /* avoid spinning every pump */
        state_pacing_on_timeout(s);
        cwnd = s->state_cwnd;
        chunks_cap = s->state_chunks_cap;
        ack_timeout_ms = (rnet_u64)s->state_ack_timeout_ms;
    }

    if (s->state_send_cursor < s->state_peer_ack)
    {
        s->state_send_cursor = s->state_peer_ack;
    }
    window_end = s->state_peer_ack + cwnd;
    if (window_end > s->state_total)
    {
        window_end = s->state_total;
    }

    while (s->state_send_cursor < window_end && sent_this_pump < chunks_cap)
    {
        rnet_u32 off = s->state_send_cursor;
        rnet_u32 left = s->state_total - off;
        rnet_u16 chunk = (left > RNET_STATE_CHUNK_MAX) ? (rnet_u16)RNET_STATE_CHUNK_MAX : (rnet_u16)left;
        n = rnet_proto_encode_state_chunk(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot,
                                          s->state_xfer_id, off, s->state_buf + off, chunk);
        if (n <= 0)
        {
            break;
        }
        send_raw(s, buf, n);
        s->state_send_cursor += chunk;
        sent_this_pump++;
    }
    s->state_last_tx_ms = now;

    /* Progress log every ~500ms (or on first ACK) — useful on slow TURN paths. */
    if (s->state_peer_ack != s->state_last_progress_acked || s->state_last_progress_log_ms == 0 ||
        now - s->state_last_progress_log_ms >= 500ULL)
    {
        unsigned kib_s = 0;
        if (s->state_xfer_start_ms != 0 && now > s->state_xfer_start_ms)
        {
            rnet_u64 elapsed = now - s->state_xfer_start_ms;
            if (elapsed > 0)
                kib_s = (unsigned)((s->state_peer_ack * 1000ULL) / elapsed / 1024ULL);
        }
        fprintf(stderr,
                "rnet_state: xfer_id=%u op=%u %u/%u acked (%u KiB/s) cwnd=%u chunks=%u to=%ums%s\n",
                (unsigned)s->state_xfer_id, (unsigned)s->state_op, (unsigned)s->state_peer_ack,
                (unsigned)s->state_total, kib_s, (unsigned)s->state_cwnd, (unsigned)s->state_chunks_cap,
                (unsigned)s->state_ack_timeout_ms, is_ice ? " (ice)" : "");
        s->state_last_progress_log_ms = now;
        s->state_last_progress_acked = s->state_peer_ack;
    }

    state_mark_ready_if_complete(s);
}

static void state_on_begin(RNetSession *s, const RNetDecodedPacket *pkt)
{
    if (pkt->local_slot == s->cfg.local_slot)
    {
        return; /* ignore echo */
    }
    if (s->cfg.local_slot == 0)
    {
        return; /* host never receives BEGIN */
    }
    if (pkt->state_total_size == 0 || pkt->state_total_size > RNET_STATE_MAX)
    {
        return;
    }
    if (pkt->state_xfer_id == s->state_finished_xfer_id)
    {
        /* A delayed/retransmitted BEGIN may survive the app's LOAD apply and
         * hard resync. Re-ACK it instead of reopening a completed transfer. */
        rnet_u8 buf[64];
        int n = rnet_proto_encode_state_ack(buf, sizeof(buf), s->cfg.protocol_magic,
                                            s->cfg.session_id, s->cfg.local_slot,
                                            pkt->state_xfer_id, pkt->state_total_size);
        if (n > 0) send_raw(s, buf, n);
        return;
    }
    if (s->state_active && s->state_xfer_id == pkt->state_xfer_id && s->state_buf != NULL)
    {
        state_send_ack(s);
        return;
    }
    state_probe_clear(s); /* hash-miss path: transfer replaces probe */
    state_clear(s);
    s->state_buf = (rnet_u8 *)malloc(pkt->state_total_size);
    if (s->state_buf == NULL)
    {
        return;
    }
    memset(s->state_buf, 0, pkt->state_total_size);
    s->state_active = 1;
    s->state_sender = 0;
    s->state_stall_sim = 1;
    s->state_op = pkt->state_op;
    s->state_slot = pkt->state_slot;
    s->state_xfer_id = pkt->state_xfer_id;
    s->state_total = pkt->state_total_size;
    s->state_crc = pkt->state_payload_crc;
    s->state_contiguity = 0;
    s->state_xfer_start_ms = session_now(s);
    state_send_ack(s);
}

static void state_on_chunk(RNetSession *s, const RNetDecodedPacket *pkt)
{
    rnet_u32 end;
    if (!s->state_active || s->state_sender || s->state_buf == NULL)
    {
        return;
    }
    if (pkt->state_xfer_id != s->state_xfer_id)
    {
        return;
    }
    if (pkt->state_offset > s->state_total || pkt->state_chunk_size == 0)
    {
        return;
    }
    end = pkt->state_offset + (rnet_u32)pkt->state_chunk_size;
    if (end > s->state_total)
    {
        return;
    }
    memcpy(s->state_buf + pkt->state_offset, pkt->state_chunk, pkt->state_chunk_size);
    {
        rnet_u32 chunk_index = pkt->state_offset / RNET_STATE_CHUNK_MAX;
        state_rx_set_chunk(s, chunk_index);
        (void)end;
        state_rx_advance_contiguity(s);
    }
    {
        rnet_u64 now = session_now(s);
        /* ICE: ACK every chunk so the sender's cwnd can grow without waiting
         * on a 4ms coalesce that fights TURN RTT. LAN keeps light coalescing. */
        if (state_transport_is_ice(s) || now - s->state_last_ack_ms >= 4ULL ||
            s->state_contiguity >= s->state_total)
        {
            state_send_ack(s);
        }
    }
    state_mark_ready_if_complete(s);
}

static void state_on_ack(RNetSession *s, const RNetDecodedPacket *pkt)
{
    if (!s->state_active || !s->state_sender)
    {
        return;
    }
    if (pkt->state_xfer_id != s->state_xfer_id)
    {
        return;
    }
    if (pkt->state_ack_bytes > s->state_peer_ack)
    {
        s->state_peer_ack = pkt->state_ack_bytes;
        if (s->state_peer_ack > s->state_total)
        {
            s->state_peer_ack = s->state_total;
        }
        s->state_last_ack_ms = session_now(s);
        s->state_last_tx_ms = 0; /* send next chunk immediately */
        state_pacing_on_ack_progress(s);
    }
    state_mark_ready_if_complete(s);
}

static void state_drive_probe(RNetSession *s)
{
    rnet_u8 buf[64];
    int n;
    rnet_u64 now;

    if (!s->state_probe_active || !s->state_probe_sender || s->state_probe_reply_ready)
    {
        return;
    }
    now = session_now(s);
    /* Ready/hash probes: snappy retransmit — 40ms left a visible post-load hitch. */
    if (s->state_probe_last_tx_ms != 0 && now - s->state_probe_last_tx_ms < 8ULL)
    {
        return;
    }
    n = rnet_proto_encode_state_probe(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                      s->cfg.local_slot, s->state_probe_op, s->state_probe_slot,
                                      s->state_probe_size, s->state_probe_crc);
    if (n > 0)
    {
        send_raw(s, buf, n);
        s->state_probe_last_tx_ms = now;
    }
}

static void state_on_probe(RNetSession *s, const RNetDecodedPacket *pkt)
{
    if (pkt->local_slot == s->cfg.local_slot)
    {
        return;
    }
    if (s->cfg.local_slot == 0)
    {
        return; /* host never receives PROBE */
    }
    if (s->state_active)
    {
        return; /* transfer in flight takes precedence */
    }
    /* Retransmit of a probe we already answered — resend REPLY, do not re-arm. */
    if (s->state_probe_active && !s->state_probe_sender && !s->state_probe_pending &&
        s->state_probe_op == pkt->state_op && s->state_probe_slot == pkt->state_slot &&
        s->state_probe_size == pkt->state_total_size && s->state_probe_crc == pkt->state_payload_crc)
    {
        rnet_u8 buf[64];
        int n = rnet_proto_encode_state_probe_reply(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                                    s->cfg.local_slot, s->state_probe_op, s->state_probe_slot,
                                                    s->state_probe_match ? 1u : 0u, s->state_probe_size,
                                                    s->state_probe_crc);
        if (n > 0)
        {
            send_raw(s, buf, n);
        }
        return;
    }
    /* Fresh probe — surface to the app. size==0 (coord save) must not stall
     * admit or deferred savestate_poll never runs (deadlock). */
    s->state_probe_active = 1;
    s->state_probe_sender = 0;
    s->state_probe_pending = 1;
    s->state_probe_reply_ready = 0;
    s->state_probe_match = 0;
    s->state_probe_op = pkt->state_op;
    s->state_probe_slot = pkt->state_slot;
    s->state_probe_size = pkt->state_total_size;
    s->state_probe_crc = pkt->state_payload_crc;
    s->state_stall_sim = (pkt->state_total_size != 0) ? 1 : 0;
}

static void state_on_probe_reply(RNetSession *s, const RNetDecodedPacket *pkt)
{
    if (!s->state_probe_active || !s->state_probe_sender)
    {
        return;
    }
    if (pkt->local_slot == s->cfg.local_slot)
    {
        return;
    }
    /* Bind to the active probe generation. SAVE coord (size=0,crc=target)
     * and LOAD ready (size=0,crc=ready) share op/slot with the following
     * hash probe — a late ACK must not satisfy the next probe. */
    if (pkt->state_op != s->state_probe_op || pkt->state_slot != s->state_probe_slot ||
        pkt->state_total_size != s->state_probe_size || pkt->state_payload_crc != s->state_probe_crc)
    {
        return;
    }
    s->state_probe_match = pkt->state_probe_match ? 1 : 0;
    s->state_probe_reply_ready = 1;
}

static void maybe_bootstrap(RNetSession *s)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int len;
    rnet_u64 now = session_now(s);
    rnet_u8 all_ready = 1;
    rnet_u8 slot;

    if (s->phase == RNET_PHASE_LINKING)
    {
        if (now - s->last_hello_ms >= 100ULL)
        {
            len = rnet_proto_encode_hello(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot,
                                          s->cfg.slot_count, s->delay);
            send_raw(s, buf, len);
            s->last_hello_ms = now;
        }
    }

    if (s->phase == RNET_PHASE_READY || s->phase == RNET_PHASE_LINKING)
    {
        if (!s->local_ready)
        {
            s->local_ready = 1;
        }
        if (now - s->last_ready_ms >= 100ULL)
        {
            len = rnet_proto_encode_ready(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot);
            send_raw(s, buf, len);
            s->last_ready_ms = now;
        }
        s->peer_ready[s->cfg.local_slot] = 1;
        for (slot = 0; slot < s->cfg.slot_count; ++slot)
        {
            if (!rnet_config_slot_occupied(&s->cfg, slot))
            {
                s->peer_ready[slot] = 1; /* empty seat — no READY expected */
                continue;
            }
            if (!s->peer_ready[slot])
            {
                all_ready = 0;
                break;
            }
        }
        if (all_ready && s->is_sim_authority && !s->start_sent)
        {
            len = rnet_proto_encode_start(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, 0);
            send_raw(s, buf, len);
            s->start_sent = 1;
            s->sim_tick = 0;
            s->phase = RNET_PHASE_RUNNING;
            seed_delay_prefix(s);
            send_input_bundle(s);
        }
    }
}

static void send_input_bundle(RNetSession *s)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    RNetWireFrame frames[RNET_MAX_BUNDLE];
    int red = (int)s->cfg.bundle_redundancy;
    rnet_u32 tip;
    rnet_u32 lo;
    rnet_u32 t;
    rnet_u32 ack;
    int sent_any = 0;
    rnet_u64 now = session_now(s);

    if (s->phase != RNET_PHASE_RUNNING)
    {
        return;
    }
    /* Stall or explicit suppress: do not emit pre-resync tips during load. */
    if (s->state_stall_sim || s->input_send_suppress)
    {
        return;
    }
    tip = rnet_wire_tick_from_sim(s->sim_tick, s->delay);
    /* A new future tip is latency-sensitive and sends immediately. Repeated
     * pumps for the same tip are reliability retransmits, not new data. */
    if (s->last_input_tip_valid && s->last_input_tip == tip &&
        now - s->last_input_ms < 8ULL)
    {
        return;
    }
    if (red < 1)
    {
        red = 1;
    }
    /* Startup must carry the complete neutral delay prefix. Otherwise delays
     * larger than one INPUT packet can never admit tick zero. Do not clamp
     * this window to RNET_MAX_BUNDLE — emit multiple packets instead. */
    if (red < (int)s->delay + 1)
    {
        red = (int)s->delay + 1;
    }
    lo = (tip + 1U > (rnet_u32)red) ? (tip + 1U - (rnet_u32)red) : 0U;
    /* Peer ACK of our tips: resend from ack+1 when they fell behind the
     * tip-redundancy window (WAN/TURN loss). Cap so one pump cannot emit
     * the whole 128-slot history. */
    if (s->highest_remote_ack < tip)
    {
        rnet_u32 ack_lo = s->highest_remote_ack + 1U;
        rnet_u32 max_back = (rnet_u32)RNET_HISTORY_LENGTH / 2U;
        if (max_back < 32U)
            max_back = 32U;
        if (max_back > 64U)
            max_back = 64U;
        if (tip + 1U > max_back && ack_lo + max_back <= tip)
            ack_lo = tip + 1U - max_back;
        if (ack_lo < lo)
            lo = ack_lo;
    }
    ack = rnet_ring_highest_valid(
        &s->remote_rings[(s->cfg.local_slot + 1) % s->cfg.slot_count]);
    t = lo;
    while (t <= tip)
    {
        int count = 0;
        int len;
        while (t <= tip && count < RNET_MAX_BUNDLE)
        {
            RNetInputSample sample;
            if (rnet_ring_get(&s->local_ring, t, &sample))
            {
                frames[count].tick = sample.tick;
                frames[count].size = sample.size;
                memcpy(frames[count].bytes, sample.bytes, sample.size);
                count++;
            }
            if (t == 0xffffffffu)
            {
                break;
            }
            t++;
        }
        if (count == 0)
        {
            continue;
        }
        len = rnet_proto_encode_input(buf, sizeof(buf), s->cfg.protocol_magic,
                                      s->cfg.session_id, s->cfg.local_slot, s->input_epoch, ack,
                                      frames, count);
        if (len < 0)
        {
            break;
        }
        send_raw(s, buf, len);
        s->input_bundle_sends++;
        sent_any = 1;
    }
    if (!sent_any)
    {
        return;
    }
    s->last_input_ms = now;
    s->last_input_tip = tip;
    s->last_input_tip_valid = 1;
}

static int remotes_ready_for_play_wire(RNetSession *s, rnet_u32 play_wire)
{
    rnet_u8 slot;
    RNetInputSample tmp;

    for (slot = 0; slot < s->cfg.slot_count; ++slot)
    {
        if (slot == s->cfg.local_slot)
        {
            continue;
        }
        if (!rnet_config_slot_occupied(&s->cfg, slot))
        {
            continue; /* empty lobby seat — local neutral, not on the wire */
        }
        if (!rnet_ring_get(&s->remote_rings[slot], play_wire, &tmp))
        {
            return 0;
        }
    }
    return 1;
}

/* Gameplay ticks 0..D-1 have no prior human sample. Seed them with a
 * deterministic neutral row; fresh input sampled at sim T is stored at T+D. */
static void seed_delay_prefix(RNetSession *s)
{
    rnet_u32 t;
    if (s == NULL) return;
    for (t = 0; t < (rnet_u32)s->delay; ++t)
    {
        RNetInputSample sample;
        if (rnet_ring_get(&s->local_ring, t, &sample)) continue;
        memset(&sample, 0, sizeof(sample));
        sample.tick = t;
        sample.valid = 1;
        rnet_ring_store(&s->local_ring, &sample);
    }
}

static int collect_wire_inputs(RNetSession *s, rnet_u32 wire,
                               RNetInputSample *resolved)
{
    rnet_u8 slot;
    if (s == NULL || resolved == NULL) return 0;
    memset(resolved, 0, sizeof(RNetInputSample) * RNET_MAX_SLOTS);
    for (slot = 0; slot < s->cfg.slot_count; ++slot)
    {
        RNetInputSample sample;
        int found;
        if (!rnet_config_slot_occupied(&s->cfg, slot))
        {
            memset(&sample, 0, sizeof(sample));
            sample.tick = wire;
            sample.valid = 1;
            resolved[slot] = sample;
            continue;
        }
        found = slot == s->cfg.local_slot
            ? rnet_ring_get(&s->local_ring, wire, &sample)
            : rnet_ring_get(&s->remote_rings[slot], wire, &sample);
        if (!found) return 0;
        resolved[slot] = sample;
        resolved[slot].tick = wire;
    }
    return 1;
}

/* Resolve and advertise a wire row as soon as both peers' inputs exist. The
 * row may be D frames in the future; that lead time absorbs confirmation RTT. */
static int prepare_wire_confirm(RNetSession *s, rnet_u32 wire, int force_send)
{
    RNetInputSample resolved[RNET_MAX_SLOTS];
    rnet_u32 index = wire % RNET_HISTORY_LENGTH;
    rnet_u32 hash;
    rnet_u64 now;
    rnet_u8 slot;

    if (!collect_wire_inputs(s, wire, resolved)) return 0;
    hash = hash_resolved_inputs(wire, resolved, (int)s->cfg.slot_count);
    s->published_tick[index] = wire;
    s->published_hash[index] = hash;
    s->published_valid[index] = 1;

    for (slot = 0; slot < s->cfg.slot_count; ++slot)
    {
        if (slot == s->cfg.local_slot) continue;
        if (!rnet_config_slot_occupied(&s->cfg, slot)) continue;
        if (s->peer_history_valid[index][slot] &&
            s->peer_history_tick[index][slot] == wire &&
            s->peer_history_hash[index][slot] != hash)
        {
            s->input_desync = 1;
            s->desync_tick = wire;
            s->desync_local_hash = hash;
            s->desync_remote_hash = s->peer_history_hash[index][slot];
            return 0;
        }
    }

    now = session_now(s);
    if (force_send || s->confirm_last_sent_ms[index] == 0 ||
        now - s->confirm_last_sent_ms[index] >= 4ULL)
        send_input_confirm_tick(s, wire, hash);
    return 1;
}

static int wire_confirmations_agree(RNetSession *s, rnet_u32 wire)
{
    rnet_u32 index = wire % RNET_HISTORY_LENGTH;
    rnet_u8 slot;
    if (!s->published_valid[index] || s->published_tick[index] != wire)
        return 0;
    for (slot = 0; slot < s->cfg.slot_count; ++slot)
    {
        if (slot == s->cfg.local_slot) continue;
        if (!rnet_config_slot_occupied(&s->cfg, slot)) continue;
        if (!s->peer_history_valid[index][slot] ||
            s->peer_history_tick[index][slot] != wire)
            return 0;
        if (s->peer_history_hash[index][slot] != s->published_hash[index])
            return 0;
    }
    return 1;
}

/* Retransmit confirms for [play - trail, tip], not only [play, tip].
 * Otherwise a lost INPUT_CONFIRM for an already-admitted tick leaves the
 * slower peer in wait_confirm forever while the faster peer runs ahead
 * (spam rematch over ICE: guest@19 wait_confirm, host@26 wait_remote).
 * Prefer the cached published hash so trailing ticks still retransmit after
 * rings have advanced past the original inputs. */
static void refresh_confirm_window(RNetSession *s, rnet_u32 play_wire, rnet_u32 sample_wire)
{
    rnet_u32 trail;
    rnet_u32 lo;
    rnet_u32 w;
    rnet_u64 now;

    if (s == NULL)
        return;
    trail = (rnet_u32)s->delay + (rnet_u32)s->cfg.bundle_redundancy + 8u;
    if (trail < 16u)
        trail = 16u;
    if (trail > (rnet_u32)RNET_HISTORY_LENGTH / 2u)
        trail = (rnet_u32)RNET_HISTORY_LENGTH / 2u;
    lo = (play_wire > trail) ? (play_wire - trail) : 0u;
    if (sample_wire < lo)
        sample_wire = lo;
    now = session_now(s);
    for (w = lo; w <= sample_wire; ++w)
    {
        rnet_u32 index = w % RNET_HISTORY_LENGTH;
        if (s->published_valid[index] && s->published_tick[index] == w)
        {
            if (s->confirm_last_sent_ms[index] == 0 ||
                now - s->confirm_last_sent_ms[index] >= 4ULL)
                send_input_confirm_tick(s, w, s->published_hash[index]);
        }
        else
        {
            (void)prepare_wire_confirm(s, w, 0);
        }
        if (w == 0xffffffffu)
            break;
    }
}

RNetSession *rnet_session_create(const RNetConfig *cfg, const RNetHostVTable *host)
{
    RNetSession *s;
    rnet_u8 i;
    if ((cfg == NULL) || (host == NULL) || (host->sample_local == NULL) || (host->publish == NULL))
    {
        return NULL;
    }
    if (cfg->slot_count < 2 || cfg->slot_count > RNET_MAX_SLOTS || cfg->local_slot >= cfg->slot_count)
    {
        return NULL;
    }
    s = (RNetSession *)calloc(1, sizeof(*s));
    if (s == NULL)
    {
        return NULL;
    }
    s->cfg = *cfg;
    s->host = *host;
    s->delay = cfg->input_delay;
    s->phase = RNET_PHASE_IDLE;
    s->is_sim_authority = (cfg->local_slot == 0) ? 1 : 0;
    s->rb_peer_slot = -1;
    s->session_start_ms = rnet_os_monotonic_ms();
    s->last_peer_rx_ms = 0;
    s->peer_gone = 0;
    rnet_transport_init(&s->transport);
    rnet_ring_clear(&s->local_ring);
    for (i = 0; i < RNET_MAX_SLOTS; ++i)
    {
        rnet_ring_clear(&s->remote_rings[i]);
    }
    return s;
}

void rnet_session_destroy(RNetSession *s)
{
    if (s == NULL)
    {
        return;
    }
    state_clear(s);
    rnet_transport_shutdown(&s->transport);
    rnet_ice_agent_destroy(s->ice);
    s->ice = NULL;
    free(s);
}

int rnet_session_start_lan(RNetSession *s, const char *bind_hostport, const char *peer_hostport)
{
    if (s == NULL)
    {
        return -1;
    }
    if (rnet_transport_start_lan(&s->transport, bind_hostport, peer_hostport) != 0)
    {
        return -1;
    }
    s->phase = RNET_PHASE_LINKING;
    s->last_hello_ms = 0;
    return 0;
}

int rnet_session_start_lan_hub(RNetSession *s, const char *bind_hostport)
{
    if (s == NULL)
    {
        return -1;
    }
    /* Transport fan-out hub (lobby owner). Sim local_slot is independent —
     * seats may be reordered; guests still dial this peer's endpoint. */
    if (rnet_transport_start_lan_hub(&s->transport, bind_hostport) != 0)
    {
        return -1;
    }
    s->phase = RNET_PHASE_LINKING;
    s->last_hello_ms = 0;
    return 0;
}

int rnet_session_start_ice(RNetSession *s, const RNetIceConfig *ice)
{
#if !defined(RNET_ENABLE_ICE)
    (void)s;
    (void)ice;
    return -1;
#else
    RNetIceConfig local;
    if ((s == NULL) || (ice == NULL))
    {
        return -1;
    }
    local = *ice;
    if (s->ice != NULL)
    {
        rnet_ice_agent_destroy(s->ice);
        s->ice = NULL;
    }
    s->ice = rnet_ice_agent_create(&local, ice_emit_bridge, s);
    if (s->ice == NULL)
    {
        return -1;
    }
    rnet_transport_shutdown(&s->transport);
    rnet_transport_init(&s->transport);
    s->transport.mode = RNET_TRANSPORT_ICE;
    s->transport.ice_send = ice_send_bridge;
    s->transport.ice_recv = ice_recv_bridge;
    s->transport.ice_ctx = s->ice;
    s->ice_attempt_ms = session_now(s);
    s->ice_completed_ms = 0;
    if (rnet_ice_agent_start_gathering(s->ice) != 0)
    {
        return -1;
    }
    /* Stay IDLE until ICE completes; pump will promote to LINKING. */
    s->phase = RNET_PHASE_IDLE;
    return 0;
#endif
}

#if defined(RNET_ENABLE_ICE)
/* After STUN/host ICE fails or stalls, one automatic gather with force_relay
 * when TURN credentials are present. Opt out: RNET_ICE_NO_RELAY_FALLBACK=1.
 *
 * Timers (env overrides):
 *   RNET_ICE_RELAY_FALLBACK_MS — general stall (default 5000, was 12000)
 *   RNET_ICE_RELAY_PRIVATE_MS  — early fallback when every remote so far is
 *                                RFC1918 (default 2500); host/STUN cannot
 *                                reach CGNAT LAN candidates
 *   RNET_ICE_RELAY_DEAD_MS     — completed non-relay but no session RX
 */
static void session_maybe_ice_relay_fallback(RNetSession *s)
{
    RNetIceState st;
    rnet_u64 now;
    rnet_u64 elapsed;
    rnet_u64 stuck_ms = 5000ULL;
    rnet_u64 private_ms = 2500ULL;
    rnet_u64 dead_ms = 6000ULL;
    const char *env;
    char path[32];
    int only_private;

    if (s == NULL || s->ice == NULL || s->phase == RNET_PHASE_RUNNING)
        return;
    if (rnet_ice_agent_relay_fallback_done(s->ice) || rnet_ice_agent_is_force_relay(s->ice))
        return;
    if (!rnet_ice_agent_has_turn(s->ice))
        return;
    env = getenv("RNET_ICE_NO_RELAY_FALLBACK");
    if (env != NULL && env[0] != '\0' && env[0] != '0')
        return;
    env = getenv("RNET_ICE_RELAY_FALLBACK_MS");
    if (env != NULL && env[0] != '\0')
    {
        long v = strtol(env, NULL, 10);
        if (v >= 2000L && v <= 60000L)
            stuck_ms = (rnet_u64)v;
    }
    env = getenv("RNET_ICE_RELAY_PRIVATE_MS");
    if (env != NULL && env[0] != '\0')
    {
        long v = strtol(env, NULL, 10);
        if (v >= 1000L && v <= 30000L)
            private_ms = (rnet_u64)v;
    }
    env = getenv("RNET_ICE_RELAY_DEAD_MS");
    if (env != NULL && env[0] != '\0')
    {
        long v = strtol(env, NULL, 10);
        if (v >= 2000L && v <= 30000L)
            dead_ms = (rnet_u64)v;
    }
    if (private_ms > stuck_ms)
        private_ms = stuck_ms;

    now = session_now(s);
    elapsed = (s->ice_attempt_ms != 0ULL && now >= s->ice_attempt_ms)
                  ? (now - s->ice_attempt_ms)
                  : 0ULL;
    st = rnet_ice_agent_state(s->ice);
    only_private = rnet_ice_agent_remote_only_private(s->ice);

    if (st == RNET_ICE_STATE_FAILED)
        goto do_fallback;
    if (st != RNET_ICE_STATE_CONNECTED && st != RNET_ICE_STATE_COMPLETED)
    {
        /* CGNAT / internet: only private host remotes will never connect via
         * host/STUN — cut over to force_relay without waiting the full stall. */
        if (only_private && elapsed >= private_ms)
            goto do_fallback;
        if (elapsed >= stuck_ms)
            goto do_fallback;
        return;
    }
    /* Completed on a non-relay path but no session packets yet — flaky CGNAT. */
    if (s->last_peer_rx_ms == 0ULL && s->ice_completed_ms != 0ULL &&
        now - s->ice_completed_ms >= dead_ms)
    {
        path[0] = '\0';
        rnet_ice_agent_selected_info(s->ice, path, sizeof(path), NULL, 0, NULL, 0);
        if (path[0] != '\0' && strcmp(path, "relay") != 0)
            goto do_fallback;
    }
    return;

do_fallback:
    if (only_private)
    {
        fprintf(stderr,
                "rnet_ice: TURN fallback with RFC1918-only remotes so far — "
                "restarting force_relay so both sides can gather typ relay "
                "(early=%llums stall=%llums)\n",
                (unsigned long long)private_ms, (unsigned long long)stuck_ms);
    }
    else
    {
        fprintf(stderr,
                "rnet_ice: TURN fallback after host/STUN stall (%llums) — "
                "restarting force_relay\n",
                (unsigned long long)stuck_ms);
    }
    if (rnet_ice_agent_restart_force_relay(s->ice) != 0)
        return;
    s->transport.ice_ctx = s->ice;
    s->ice_attempt_ms = now;
    s->ice_completed_ms = 0;
    s->phase = RNET_PHASE_IDLE;
}
#endif

void rnet_session_pump(RNetSession *s)
{
    if (s == NULL)
    {
        return;
    }
    if (s->ice != NULL)
    {
#if defined(RNET_ENABLE_ICE)
        session_maybe_ice_relay_fallback(s);
#endif
        rnet_ice_agent_poll(s->ice);
        if (s->phase == RNET_PHASE_IDLE && rnet_ice_agent_state(s->ice) == RNET_ICE_STATE_COMPLETED)
        {
            s->phase = RNET_PHASE_LINKING;
            if (s->ice_completed_ms == 0ULL)
                s->ice_completed_ms = session_now(s);
        }
    }
    pump_recv(s);
    maybe_bootstrap(s);
    if (s->state_probe_active)
    {
        state_drive_probe(s);
    }
    if (s->state_active)
    {
        /* Burst recv↔send so ACK progress can refill the window inside one
         * host pump (otherwise TURN transfers are gated to one cwnd/frame). */
        int burst;
        for (burst = 0; burst < 4; burst++)
        {
            state_drive_sender(s);
            if (!s->state_active || s->state_ready)
                break;
            pump_recv(s);
        }
    }
    /* LOAD apply/ready suppresses INPUT; emit HELLO so peers keep stamping
     * last_peer_rx_ms (hash-match apply of a multi‑MB .pst is otherwise silent). */
    if (s->phase == RNET_PHASE_RUNNING && (s->input_send_suppress || s->state_stall_sim))
    {
        rnet_u64 now = session_now(s);
        if (now - s->last_hello_ms >= 250ULL)
        {
            rnet_u8 buf[RNET_MAX_PACKET];
            int len = rnet_proto_encode_hello(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                              s->cfg.local_slot, s->cfg.slot_count, s->delay);
            send_raw(s, buf, len);
            s->last_hello_ms = now;
        }
    }
    send_input_bundle(s);
    /* Retransmit pending DELAY_SYNC until both peers apply (UDP). */
    if (s->delay_pending && s->phase == RNET_PHASE_RUNNING)
    {
        rnet_u64 now = session_now(s);
        if (s->delay_pending_last_tx_ms == 0ULL ||
            now - s->delay_pending_last_tx_ms >= 50ULL)
        {
            emit_delay_sync(s, s->delay_pending_value, s->delay_pending_effective);
        }
    }
}

int rnet_session_wait_recv(RNetSession *s, int timeout_ms)
{
    if (s == NULL)
    {
        return 0;
    }
    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }
    /* LAN UDP: block in poll so the peer can run without us busy-spinning. */
    if (s->transport.mode == RNET_TRANSPORT_LAN_UDP && rnet_os_socket_valid(s->transport.sock))
    {
        int r = rnet_os_poll_recv(s->transport.sock, timeout_ms);
        return (r > 0) ? 1 : 0;
    }
    /* ICE / no sock: coarse sleep only — still better than a busy spin. */
    if (timeout_ms > 0)
    {
        rnet_os_sleep_micros((unsigned)timeout_ms * 1000U);
    }
    return 0;
}

int rnet_session_try_admit(RNetSession *s, rnet_u32 sim_tick)
{
    RNetInputSample resolved[RNET_MAX_SLOTS];
    RNetInputSample local_play;
    RNetInputSample local_future;
    rnet_u32 play_wire;
    rnet_u32 sample_wire;
    rnet_u32 hash;
    rnet_u8 slot;

    if ((s == NULL) || (s->phase != RNET_PHASE_RUNNING))
    {
        if (s)
            note_admit_stall(s, RNET_ADMIT_NOT_RUNNING);
        return 0;
    }
    if (s->state_stall_sim && (s->state_active || s->state_probe_active))
    {
        /* Stall while probe or chunked transfer is in flight. */
        note_admit_stall(s, RNET_ADMIT_STATE_XFER);
        return 0;
    }
    if (sim_tick != s->sim_tick)
    {
        /* Host must advance in lockstep with session clock. */
        note_admit_stall(s, RNET_ADMIT_SIM_MISMATCH);
        return 0;
    }
    if (s->input_desync)
    {
        note_admit_stall(s, RNET_ADMIT_DESYNC);
        return 0;
    }

    /* Simulate wire T while sampling local input for wire T+D. Once the
     * neutral prefix is consumed, peer input normally arrived D frames ago. */
    play_wire = sim_tick;
    sample_wire = rnet_wire_tick_from_sim(sim_tick, s->delay);
    if (!rnet_ring_get(&s->local_ring, sample_wire, &local_future))
    {
        memset(&local_future, 0, sizeof(local_future));
        s->host.sample_local(sim_tick, &local_future, s->host.ctx);
        local_future.tick = sample_wire;
        local_future.valid = 1;
        if (local_future.size > RNET_INPUT_MAX)
        {
            local_future.size = RNET_INPUT_MAX;
        }
        rnet_ring_store(&s->local_ring, &local_future);
    }

    send_input_bundle(s);
    refresh_confirm_window(s, play_wire, sample_wire);

    if (!rnet_ring_get(&s->local_ring, play_wire, &local_play))
    {
        if (play_wire == sample_wire)
            local_play = local_future;
        else
        {
            send_input_bundle(s);
            note_admit_stall(s, RNET_ADMIT_WAIT_LOCAL_INPUT);
            return 0;
        }
    }

    if (!remotes_ready_for_play_wire(s, play_wire))
    {
        send_input_bundle(s);
        note_admit_stall(s, RNET_ADMIT_WAIT_REMOTE_INPUT);
        return 0;
    }

    memset(resolved, 0, sizeof(resolved));
    for (slot = 0; slot < s->cfg.slot_count; ++slot)
    {
        if (slot == s->cfg.local_slot)
        {
            resolved[slot] = local_play;
            resolved[slot].tick = sim_tick;
        }
        else if (!rnet_config_slot_occupied(&s->cfg, slot))
        {
            /* Sparse lobby seat: deterministic neutral (matches delay-prefix
             * seed zeros). All peers synthesize the same bytes locally. */
            memset(&resolved[slot], 0, sizeof(resolved[slot]));
            resolved[slot].tick = sim_tick;
            resolved[slot].valid = 1;
        }
        else
        {
            RNetInputSample remote;
            if (!rnet_ring_get(&s->remote_rings[slot], play_wire, &remote))
            {
                note_admit_stall(s, RNET_ADMIT_WAIT_REMOTE_INPUT);
                return 0;
            }
            resolved[slot] = remote;
            resolved[slot].tick = sim_tick;
        }
    }

    hash = hash_resolved_inputs(sim_tick, resolved, (int)s->cfg.slot_count);

    if (!prepare_wire_confirm(s, play_wire, 0) || s->input_desync) {
        note_admit_stall(s, s->input_desync ? RNET_ADMIT_DESYNC
                                            : RNET_ADMIT_WAIT_CONFIRM);
        return 0;
    }
    if (s->published_hash[play_wire % RNET_HISTORY_LENGTH] != hash)
    {
        s->input_desync = 1;
        s->desync_tick = play_wire;
        s->desync_local_hash = hash;
        s->desync_remote_hash =
            s->published_hash[play_wire % RNET_HISTORY_LENGTH];
        note_admit_stall(s, RNET_ADMIT_DESYNC);
        return 0;
    }
    if (!wire_confirmations_agree(s, play_wire))
    {
        (void)prepare_wire_confirm(s, play_wire, 0);
        send_input_bundle(s);
        note_admit_stall(s, RNET_ADMIT_WAIT_CONFIRM);
        return 0;
    }
    s->host.publish(sim_tick, resolved, (int)s->cfg.slot_count, s->host.ctx);
    note_admit_ok(s);
    return 1;

#if 0 /* Legacy strict confirmation barrier; superseded by delay pipeline. */

    if (!s->confirm_active || s->confirm_sim_tick != sim_tick)
    {
        /* Peer INPUT_CONFIRM may arrive before we activate. Preserve those
         * same-tick sightings — wiping them races the slower peer into a
         * permanent stall once the faster peer admits and stops retransmit. */
        rnet_u8 saved_seen[RNET_MAX_SLOTS];
        rnet_u32 saved_hash[RNET_MAX_SLOTS];
        memcpy(saved_seen, s->confirm_seen, sizeof(saved_seen));
        memcpy(saved_hash, s->peer_confirm_hash, sizeof(saved_hash));

        s->confirm_active = 1;
        s->confirm_sim_tick = sim_tick;
        s->confirm_hash = hash;
        memcpy(s->confirm_resolved, resolved, sizeof(resolved));
        memset(s->confirm_seen, 0, sizeof(s->confirm_seen));
        memset(s->peer_confirm_hash, 0, sizeof(s->peer_confirm_hash));
        for (slot = 0; slot < s->cfg.slot_count; ++slot)
        {
            if (slot == s->cfg.local_slot)
            {
                continue;
            }
            if (saved_seen[slot])
            {
                s->confirm_seen[slot] = 1;
                s->peer_confirm_hash[slot] = saved_hash[slot];
                if (saved_hash[slot] != hash)
                {
                    s->input_desync = 1;
                    s->desync_tick = sim_tick;
                    s->desync_local_hash = hash;
                    s->desync_remote_hash = saved_hash[slot];
                    return 0;
                }
            }
        }
        s->confirm_seen[s->cfg.local_slot] = 1;
        s->peer_confirm_hash[s->cfg.local_slot] = hash;
        send_input_confirm(s);
        send_input_bundle(s);
        /* Peer CONFIRM may already be in saved_seen (arrived before we
         * activated). Admit immediately when everyone already agrees. */
        if (confirms_agree(s))
        {
            s->host.publish(sim_tick, s->confirm_resolved, (int)s->cfg.slot_count, s->host.ctx);
            s->confirm_active = 0;
            return 1;
        }
        return 0;
    }

    if (hash != s->confirm_hash)
    {
        s->input_desync = 1;
        s->desync_tick = sim_tick;
        s->desync_local_hash = hash;
        s->desync_remote_hash = s->confirm_hash;
        return 0;
    }

    now = session_now(s);
    if (now - s->last_confirm_ms >= 4ULL)
    {
        send_input_confirm(s);
    }

    if (!confirms_agree(s))
    {
        send_input_bundle(s);
        return 0;
    }

    s->host.publish(sim_tick, s->confirm_resolved, (int)s->cfg.slot_count, s->host.ctx);
    s->confirm_active = 0;
    return 1;
#endif
}

static void apply_pending_delay(RNetSession *s)
{
    if (s == NULL || !s->delay_pending)
    {
        return;
    }
    if (s->sim_tick < s->delay_pending_effective)
    {
        return;
    }
    s->delay = s->delay_pending_value;
    s->cfg.input_delay = s->delay_pending_value;
    s->delay_pending = 0;
}

static void emit_delay_sync(RNetSession *s, rnet_u8 new_delay, rnet_u32 effective_tick)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int len;

    if (s == NULL)
    {
        return;
    }
    len = rnet_proto_encode_delay_sync(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                       new_delay, effective_tick);
    if (len > 0)
    {
        send_raw(s, buf, len);
        s->delay_pending_last_tx_ms = session_now(s);
    }
}

void rnet_session_advance(RNetSession *s)
{
    if (s == NULL)
    {
        return;
    }
    s->sim_tick++;
    apply_pending_delay(s);
}

int rnet_session_input_desync(const RNetSession *s, rnet_u32 *tick, rnet_u32 *local_hash, rnet_u32 *remote_hash)
{
    if ((s == NULL) || !s->input_desync)
    {
        return 0;
    }
    if (tick != NULL)
    {
        *tick = s->desync_tick;
    }
    if (local_hash != NULL)
    {
        *local_hash = s->desync_local_hash;
    }
    if (remote_hash != NULL)
    {
        *remote_hash = s->desync_remote_hash;
    }
    return 1;
}

int rnet_session_send_bye(RNetSession *s)
{
    rnet_u8 buf[64];
    int n;

    if ((s == NULL) || (s->transport.mode == RNET_TRANSPORT_NONE))
    {
        return -1;
    }
    n = rnet_proto_encode_bye(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot);
    if (n <= 0)
    {
        return -1;
    }
    /* Best-effort: send a few times so a single loss doesn't leave peer hanging. */
    (void)rnet_transport_send(&s->transport, buf, (size_t)n);
    (void)rnet_transport_send(&s->transport, buf, (size_t)n);
    (void)rnet_transport_send(&s->transport, buf, (size_t)n);
    return 0;
}

int rnet_session_peer_disconnected(const RNetSession *s, rnet_u64 timeout_ms)
{
    rnet_u64 now;
    rnet_u64 last;

    if (s == NULL)
    {
        return 0;
    }
    if (s->peer_gone)
    {
        return 1;
    }
    if (timeout_ms == 0)
    {
        return 0;
    }
    now = rnet_os_monotonic_ms();
    if (s->last_peer_rx_ms == 0)
    {
        /* No peer traffic yet — only after we expected packets (linking/running).
         * Rematch session_reboot is often >15s on the slower peer; the old
         * timeout_ms*10 (~15s at 1500) false-disconnected the fast peer and
         * BYE'd both back to lobby. Keep a long link budget; callers that
         * pass timeout_ms==0 skip this path entirely (BYE-only). */
        rnet_u64 link_budget_ms;
        if (s->phase != RNET_PHASE_RUNNING && s->phase != RNET_PHASE_LINKING)
        {
            return 0;
        }
        link_budget_ms = timeout_ms * 60u;
        if (link_budget_ms < 90000u)
            link_budget_ms = 90000u;
        if (s->session_start_ms != 0 && (now - s->session_start_ms) > link_budget_ms)
        {
            return 1;
        }
        return 0;
    }
    last = s->last_peer_rx_ms;
    return (now > last && (now - last) >= timeout_ms) ? 1 : 0;
}

void rnet_session_touch_peer_liveness(RNetSession *s)
{
    if (s == NULL || s->peer_gone)
    {
        return;
    }
    s->last_peer_rx_ms = session_now(s);
}

void rnet_session_push_signal(RNetSession *s, const RNetSignal *msg)
{
    if ((s == NULL) || (msg == NULL) || (s->ice == NULL))
    {
        return;
    }
    rnet_ice_agent_push_signal(s->ice, msg);
}

rnet_u8 rnet_session_committed_delay(const RNetSession *s)
{
    return (s != NULL) ? s->delay : 0;
}

int rnet_session_request_delay_change(RNetSession *s, rnet_u8 new_delay)
{
    rnet_u32 effective;
    rnet_u8 clamped;

    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
    {
        return 0;
    }
    clamped = new_delay;
    if (clamped < 2u)
    {
        clamped = 2u;
    }
    if (clamped > 20u)
    {
        clamped = 20u;
    }
    if (clamped == s->delay && !s->delay_pending)
    {
        return 0;
    }
    /* Margin: 2*D + 8 ticks so the packet can land before both peers admit. */
    effective = s->sim_tick + ((rnet_u32)s->delay * 2u) + 8u;
    if (s->delay_pending && s->delay_pending_effective > s->sim_tick &&
        s->delay_pending_value == clamped)
    {
        /* Already scheduled — refresh the wire copy. */
        emit_delay_sync(s, clamped, s->delay_pending_effective);
        return 1;
    }
    s->delay_pending = 1;
    s->delay_pending_value = clamped;
    s->delay_pending_effective = effective;
    emit_delay_sync(s, clamped, effective);
    return 1;
}

int rnet_session_local_slot(const RNetSession *s)
{
    return (s != NULL) ? (int)s->cfg.local_slot : -1;
}

rnet_u32 rnet_session_sim_tick(const RNetSession *s)
{
    return (s != NULL) ? s->sim_tick : 0;
}

int rnet_session_is_running(const RNetSession *s)
{
    return (s != NULL && s->phase == RNET_PHASE_RUNNING) ? 1 : 0;
}

RNetIceState rnet_session_ice_state(const RNetSession *s)
{
    if ((s == NULL) || (s->ice == NULL))
    {
        return RNET_ICE_STATE_IDLE;
    }
    return rnet_ice_agent_state(s->ice);
}

void rnet_session_get_stats(const RNetSession *s, RNetSessionStats *out)
{
    rnet_u8 slot;
    rnet_u32 highest_remote = 0;
    rnet_u64 now;
    int have_remote = 0;

    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));
    if (s == NULL)
        return;

    out->sim_tick = s->sim_tick;
    out->delay = s->delay;
    out->local_slot = s->cfg.local_slot;
    out->slot_count = s->cfg.slot_count;
    out->is_running = (s->phase == RNET_PHASE_RUNNING) ? 1 : 0;
    out->peer_gone = s->peer_gone;
    out->input_desync = s->input_desync;
    out->desync_tick = s->desync_tick;
    out->desync_local_hash = s->desync_local_hash;
    out->desync_remote_hash = s->desync_remote_hash;
    out->ice_state = rnet_session_ice_state(s);
    out->last_stall = s->last_stall;
    out->consecutive_stalls = s->consecutive_stalls;
    out->admit_ok_count = s->admit_ok_count;
    out->stall_streaks = s->stall_streaks;
    out->last_admit_wait_ms = s->last_admit_wait_ms;
    out->max_admit_wait_ms = s->max_admit_wait_ms;
    out->state_busy = (s->state_active || s->state_probe_active) ? 1 : 0;
    out->state_op = s->state_active ? s->state_op
                    : (s->state_probe_active ? s->state_probe_op : 0);
    out->state_sender = s->state_active ? s->state_sender : 0;
    out->state_bytes_total = s->state_active ? s->state_total : 0;
    if (s->state_active)
        out->state_bytes_acked = s->state_sender ? s->state_peer_ack : s->state_contiguity;
    else
        out->state_bytes_acked = 0;
    out->packets_rx = s->packets_rx;
    out->input_bundle_sends = s->input_bundle_sends;

    now = session_now((RNetSession *)s);
    if (s->last_peer_rx_ms != 0)
        out->last_peer_rx_age_ms = now - s->last_peer_rx_ms;
    /* Refresh live stall wait while still blocked. */
    if (s->stall_started_ms != 0 && s->last_stall != RNET_ADMIT_OK) {
        out->last_admit_wait_ms = (rnet_u32)(now - s->stall_started_ms);
        if (out->last_admit_wait_ms > out->max_admit_wait_ms)
            out->max_admit_wait_ms = out->last_admit_wait_ms;
    }

    for (slot = 0; slot < s->cfg.slot_count; ++slot) {
        rnet_u32 tip;
        if (slot == s->cfg.local_slot)
            continue;
        if (!rnet_config_slot_occupied(&s->cfg, slot))
            continue;
        tip = rnet_ring_highest_valid(&s->remote_rings[slot]);
        if (!have_remote || tip > highest_remote)
            highest_remote = tip;
        have_remote = 1;
    }
    out->highest_remote_wire = highest_remote;
    out->remote_lead = have_remote ? (int)highest_remote - (int)s->sim_tick : 0;

#if defined(RNET_ENABLE_ICE)
    if (s->ice != NULL) {
        rnet_ice_agent_selected_info(s->ice, out->ice_path, sizeof(out->ice_path),
                                     out->ice_local, sizeof(out->ice_local),
                                     out->ice_remote, sizeof(out->ice_remote));
        if (out->ice_state == RNET_ICE_STATE_FAILED)
            snprintf(out->ice_path, sizeof(out->ice_path), "failed");
        else if (out->ice_state != RNET_ICE_STATE_CONNECTED &&
                 out->ice_state != RNET_ICE_STATE_COMPLETED &&
                 out->ice_path[0] == '\0')
            snprintf(out->ice_path, sizeof(out->ice_path), "pending");
    } else
#endif
    {
        snprintf(out->ice_path, sizeof(out->ice_path), "lan");
    }
}

int rnet_session_state_probe(RNetSession *s, rnet_u8 op, rnet_u8 slot, rnet_u32 total_size, rnet_u32 payload_crc)
{
    if ((s == NULL) || s->cfg.local_slot != 0 || s->phase != RNET_PHASE_RUNNING)
    {
        return -1;
    }
    if (s->state_active || (s->state_probe_active && s->state_probe_sender && !s->state_probe_reply_ready))
    {
        return -1;
    }
    if (op != RNET_STATE_OP_SAVE && op != RNET_STATE_OP_LOAD && op != RNET_STATE_OP_SRAM &&
        op != RNET_STATE_OP_RB_KF && op != RNET_STATE_OP_BOOT)
    {
        return -1;
    }
    if (total_size > RNET_STATE_MAX)
    {
        return -1;
    }

    state_probe_clear(s);
    s->state_probe_active = 1;
    s->state_probe_sender = 1;
    s->state_probe_reply_ready = 0;
    s->state_probe_pending = 0;
    s->state_probe_match = 0;
    s->state_probe_op = op;
    s->state_probe_slot = slot;
    s->state_probe_size = total_size;
    s->state_probe_crc = payload_crc;
    s->state_probe_last_tx_ms = 0;
    /* size==0 probes must not stall INPUT: SAVE coord and LOAD ready both need
     * the slower peer to keep admitting for savestate_poll. App-layer LOAD_READY
     * freezes sim until mutual ready + hard_resync; stalling send_input_bundle
     * here starves the late applier's tip runway (spam-load hang).
     * Hash probe (size!=0): stall until agree or transfer. */
    s->state_stall_sim = (total_size != 0) ? 1 : 0;
    state_drive_probe(s);
    return 0;
}

int rnet_session_state_probe_take_reply(RNetSession *s, int *match_out)
{
    if ((s == NULL) || !s->state_probe_active || !s->state_probe_sender || !s->state_probe_reply_ready)
    {
        return 0;
    }
    if (match_out)
    {
        *match_out = s->state_probe_match;
    }
    return 1;
}

int rnet_session_state_probe_pending(const RNetSession *s, rnet_u8 *op_out, rnet_u8 *slot_out, rnet_u32 *size_out,
                                     rnet_u32 *crc_out)
{
    if ((s == NULL) || !s->state_probe_active || !s->state_probe_pending)
    {
        return 0;
    }
    if (op_out)
    {
        *op_out = s->state_probe_op;
    }
    if (slot_out)
    {
        *slot_out = s->state_probe_slot;
    }
    if (size_out)
    {
        *size_out = s->state_probe_size;
    }
    if (crc_out)
    {
        *crc_out = s->state_probe_crc;
    }
    return 1;
}

int rnet_session_state_probe_reply(RNetSession *s, int match)
{
    rnet_u8 buf[64];
    int n;

    if ((s == NULL) || !s->state_probe_active || s->state_probe_sender || !s->state_probe_pending)
    {
        return -1;
    }
    n = rnet_proto_encode_state_probe_reply(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                            s->cfg.local_slot, s->state_probe_op, s->state_probe_slot,
                                            match ? 1u : 0u, s->state_probe_size, s->state_probe_crc);
    if (n <= 0)
    {
        return -1;
    }
    send_raw(s, buf, n);
    s->state_probe_pending = 0;
    s->state_probe_match = match ? 1 : 0;
    if (s->state_probe_size == 0)
    {
        s->state_stall_sim = 0;
        if (s->state_probe_op == RNET_STATE_OP_LOAD || s->state_probe_op == RNET_STATE_OP_BOOT)
        {
            /* Post-load / post-boot ready ACK — done; do not keep probe for retransmit. */
            state_probe_clear(s);
            return 0;
        }
        /* SAVE coord ACK: leave probe active for retransmit replies. */
        return 0;
    }
    if (match)
    {
        /* Real hash agree — host finishes probe; guest unstalls. */
        state_probe_clear(s);
    }
    /* Hash miss: keep stall until STATE_BEGIN (or a new probe). */
    return 0;
}

void rnet_session_state_probe_finish(RNetSession *s)
{
    state_probe_clear(s);
}

int rnet_session_state_begin(RNetSession *s, rnet_u8 op, rnet_u8 slot, const void *data, size_t size)
{
    rnet_u8 buf[64];
    int n;

    if ((s == NULL) || (data == NULL) || (size == 0) || (size > RNET_STATE_MAX))
    {
        return -1;
    }
    if (s->cfg.local_slot != 0 || s->phase != RNET_PHASE_RUNNING || s->state_active)
    {
        return -1;
    }
    if (op != RNET_STATE_OP_SAVE && op != RNET_STATE_OP_LOAD && op != RNET_STATE_OP_SRAM &&
        op != RNET_STATE_OP_RB_KF && op != RNET_STATE_OP_BOOT)
    {
        return -1;
    }

    /* Drop any open probe — transfer is the authority path after a hash miss. */
    state_probe_clear(s);

    s->state_buf = (rnet_u8 *)malloc(size);
    if (s->state_buf == NULL)
    {
        return -1;
    }
    memcpy(s->state_buf, data, size);
    s->state_active = 1;
    s->state_sender = 1;
    s->state_ready = 0;
    s->state_stall_sim = 1;
    s->state_op = op;
    s->state_slot = slot;
    s->state_next_xfer_id++;
    if (s->state_next_xfer_id == 0)
    {
        s->state_next_xfer_id = 1;
    }
    s->state_xfer_id = s->state_next_xfer_id;
    s->state_total = (rnet_u32)size;
    s->state_crc = rnet_proto_checksum(s->state_buf, s->state_total);
    s->state_contiguity = 0;
    s->state_peer_ack = 0;
    s->state_send_cursor = 0;
    s->state_last_tx_ms = 0;
    s->state_last_ack_ms = 0;
    s->state_last_begin_ms = 0;
    s->state_last_progress_log_ms = 0;
    s->state_last_progress_acked = 0;
    s->state_xfer_start_ms = session_now(s);
    state_pacing_reset(s);

    n = rnet_proto_encode_state_begin(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id, s->cfg.local_slot,
                                      s->state_op, s->state_slot, s->state_xfer_id, s->state_total, s->state_crc);
    if (n > 0)
    {
        send_raw(s, buf, n);
        s->state_last_begin_ms = session_now(s);
    }
    state_drive_sender(s);
    return 0;
}

int rnet_session_state_busy(const RNetSession *s)
{
    if (s == NULL)
    {
        return 0;
    }
    if (s->state_probe_active && s->state_probe_sender && !s->state_probe_reply_ready)
    {
        return 1;
    }
    if (s->state_probe_active && s->state_probe_pending)
    {
        return 1;
    }
    return (s->state_active && !s->state_ready) ? 1 : 0;
}

int rnet_session_state_take_ready(RNetSession *s, rnet_u8 *op_out, rnet_u8 *slot_out, const void **data_out,
                                  size_t *size_out)
{
    if ((s == NULL) || !s->state_active || !s->state_ready || s->state_buf == NULL)
    {
        return 0;
    }
    if (op_out)
    {
        *op_out = s->state_op;
    }
    if (slot_out)
    {
        *slot_out = s->state_slot;
    }
    if (data_out)
    {
        *data_out = s->state_buf;
    }
    if (size_out)
    {
        *size_out = s->state_total;
    }
    return 1;
}

void rnet_session_hard_resync(RNetSession *s)
{
    rnet_u8 i;
    if (s == NULL)
    {
        return;
    }
    rnet_ring_clear(&s->local_ring);
    /* Clear remotes too: leftover tip rows from a prior post-load epoch are
     * first-wins and can let one peer admit on stale wire=D inputs. Both peers
     * re-prime after mutual ready and wait for a fresh tip exchange. */
    for (i = 0; i < RNET_MAX_SLOTS; ++i)
    {
        rnet_ring_clear(&s->remote_rings[i]);
    }
    memset(s->published_valid, 0, sizeof(s->published_valid));
    memset(s->peer_history_valid, 0, sizeof(s->peer_history_valid));
    memset(s->confirm_last_sent_ms, 0, sizeof(s->confirm_last_sent_ms));
    s->input_desync = 0;
    s->desync_tick = 0;
    s->desync_local_hash = 0;
    s->desync_remote_hash = 0;
    s->highest_remote_ack = 0;
    s->last_input_tip_valid = 0;
    /* Peers may have applied a load on different sim ticks; restart together. */
    s->sim_tick = 0;
    /* Invalidate in-flight INPUT/CONFIRM from the previous era (same low ticks
     * would otherwise first-wins into this window — spam rematch + stick mash). */
    s->input_epoch = (rnet_u16)(s->input_epoch + 1u);
    /* Keep suppress until prime_delay_inputs — avoids emitting an empty tip. */
    s->input_send_suppress = 1;
    s->delay_pending = 0;
}

void rnet_session_set_input_send_suppress(RNetSession *s, int suppress)
{
    if (s == NULL)
    {
        return;
    }
    s->input_send_suppress = suppress ? 1 : 0;
}

void rnet_session_prime_delay_inputs(RNetSession *s, const rnet_u8 *bytes, rnet_u16 size)
{
    rnet_u32 tip;
    rnet_u32 t;
    if (s == NULL || bytes == NULL || size == 0 || size > RNET_INPUT_MAX)
    {
        return;
    }
    if (s->phase != RNET_PHASE_RUNNING)
    {
        return;
    }
    tip = rnet_wire_tick_from_sim(s->sim_tick, s->delay);
    for (t = s->sim_tick; t < tip; ++t)
    {
        RNetInputSample sample;
        memset(&sample, 0, sizeof(sample));
        sample.tick = t;
        sample.size = size;
        memcpy(sample.bytes, bytes, size);
        sample.valid = 1;
        rnet_ring_store(&s->local_ring, &sample);
    }
    s->last_input_ms = 0;
    s->last_input_tip_valid = 0;
    s->input_send_suppress = 0;
    /* Prime must emit even if a LOAD ready probe still has state_stall_sim
     * (host commits sync before probe_finish). */
    {
        int saved_stall = s->state_stall_sim;
        s->state_stall_sim = 0;
        send_input_bundle(s);
        s->state_stall_sim = saved_stall;
    }
}

void rnet_session_state_finish(RNetSession *s, int hard_resync)
{
    rnet_u32 finished_xfer_id;
    if (s == NULL)
    {
        return;
    }
    finished_xfer_id = s->state_xfer_id;
    if (hard_resync)
    {
        rnet_session_hard_resync(s);
    }
    state_clear(s);
    s->state_finished_xfer_id = finished_xfer_id;
}

int rnet_session_send_rb_frame_commit(RNetSession *s, rnet_u32 through_tick,
                                      rnet_u32 state_hash)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
    {
        return -1;
    }
    n = rnet_proto_encode_rb_frame_commit(buf, sizeof(buf), s->cfg.protocol_magic,
                                          s->cfg.session_id, s->cfg.local_slot,
                                          through_tick, state_hash);
    if (n <= 0)
    {
        return -1;
    }
    send_raw(s, buf, n);
    return 0;
}

void rnet_session_set_rb_peer_slot(RNetSession *s, int slot)
{
    if (s == NULL)
    {
        return;
    }
    s->rb_peer_slot = slot;
}

int rnet_session_take_rb_frame_commit(RNetSession *s, rnet_u32 *through_tick,
                                      rnet_u32 *state_hash)
{
    if (s == NULL || s->rb_fc_q_count <= 0)
    {
        return 0;
    }
    if (through_tick)
    {
        *through_tick = s->rb_fc_tick[s->rb_fc_q_tail];
    }
    if (state_hash)
    {
        *state_hash = s->rb_fc_hash[s->rb_fc_q_tail];
    }
    s->rb_fc_q_tail = (s->rb_fc_q_tail + 1) % RNET_RB_FC_QUEUE;
    s->rb_fc_q_count--;
    return 1;
}

int rnet_session_prepare_local_tip(RNetSession *s, rnet_u32 sim_tick)
{
    rnet_u32 sample_wire;
    RNetInputSample local_future;

    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
    {
        return 0;
    }
    if (sim_tick != s->sim_tick)
    {
        return 0;
    }
    sample_wire = rnet_wire_tick_from_sim(sim_tick, s->delay);
    /* Fill EVERY missing wire in [sim .. sim+D], not just the tip. In steady
     * state only sample_wire is missing (one sample/admit, unchanged). After
     * a mid-session DELAY_SYNC increase the production tip jumps from
     * sim-1+D_old to sim+D_new, leaving [sim+D_old .. sim+D_new-1] unproduced
     * — without the back-fill the peer must invent across the gap. A
     * consumer at wire = sim (real-delay rollback, lockstep play) also
     * relies on the whole prefix existing. Missing rows repeat the current
     * sample (hold-current), matching prime_delay_inputs semantics. */
    {
        rnet_u32 w;
        int have_sample = 0;
        for (w = sim_tick; w <= sample_wire; ++w)
        {
            RNetInputSample existing;
            if (rnet_ring_get(&s->local_ring, w, &existing))
            {
                continue;
            }
            if (!have_sample)
            {
                memset(&local_future, 0, sizeof(local_future));
                if (s->host.sample_local)
                {
                    s->host.sample_local(sim_tick, &local_future, s->host.ctx);
                }
                local_future.valid = 1;
                if (local_future.size > RNET_INPUT_MAX)
                {
                    local_future.size = RNET_INPUT_MAX;
                }
                have_sample = 1;
            }
            local_future.tick = w;
            rnet_ring_store(&s->local_ring, &local_future);
        }
    }
    send_input_bundle(s);
    return 1;
}

rnet_u32 rnet_session_remote_arrival_age_ms(const RNetSession *s, int slot,
                                            rnet_u32 wire_tick)
{
    rnet_u32 idx;
    rnet_u64 now;
    if (s == NULL || slot < 0 || slot >= (int)s->cfg.slot_count ||
        slot == (int)s->cfg.local_slot)
    {
        return 0xffffffffu;
    }
    idx = wire_tick % RNET_HISTORY_LENGTH;
    if (s->remote_arr_ms[slot][idx] == 0ull ||
        s->remote_arr_tick[slot][idx] != wire_tick)
    {
        return 0xffffffffu;
    }
    now = session_now((RNetSession *)s);
    if (now <= s->remote_arr_ms[slot][idx])
    {
        return 0u;
    }
    return (rnet_u32)(now - s->remote_arr_ms[slot][idx]);
}

int rnet_session_peek_input(const RNetSession *s, int slot, rnet_u32 wire_tick,
                            RNetInputSample *out)
{
    if (s == NULL || out == NULL || slot < 0 || slot >= (int)s->cfg.slot_count)
    {
        return 0;
    }
    if (slot == (int)s->cfg.local_slot)
    {
        return rnet_ring_get(&s->local_ring, wire_tick, out);
    }
    return rnet_ring_get(&s->remote_rings[slot], wire_tick, out);
}

int rnet_session_peek_remote_input(const RNetSession *s, int slot, rnet_u32 wire_tick,
                                   RNetInputSample *out)
{
    if (s == NULL || slot == (int)s->cfg.local_slot)
    {
        return 0;
    }
    if (slot < 0 || slot >= (int)s->cfg.slot_count)
    {
        return 0;
    }
    if (!rnet_config_slot_occupied(&s->cfg, (rnet_u8)slot))
    {
        if (out)
        {
            memset(out, 0, sizeof(*out));
            out->tick = wire_tick;
            out->valid = 1;
        }
        return 1;
    }
    return rnet_session_peek_input(s, slot, wire_tick, out);
}

void rnet_session_set_sim_tick(RNetSession *s, rnet_u32 sim_tick)
{
    if (s == NULL)
    {
        return;
    }
    s->sim_tick = sim_tick;
    apply_pending_delay(s);
}

void rnet_session_clear_remote_inputs(RNetSession *s)
{
    rnet_u8 i;
    if (s == NULL)
    {
        return;
    }
    for (i = 0; i < RNET_MAX_SLOTS; ++i)
    {
        if (i == s->cfg.local_slot)
            continue;
        rnet_ring_clear(&s->remote_rings[i]);
    }
    s->highest_remote_ack = 0;
}

int rnet_session_send_rb_sync(RNetSession *s, rnet_u32 epoch_id, rnet_u32 mismatch_tick,
                              rnet_u32 load_tick, rnet_u32 target_tick,
                              rnet_u8 corrected_slot, rnet_u8 op, rnet_u8 flags)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
        return -1;
    n = rnet_proto_encode_rb_sync(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                  s->cfg.local_slot, epoch_id, mismatch_tick, load_tick,
                                  target_tick, corrected_slot, op, flags);
    if (n <= 0)
        return -1;
    send_raw(s, buf, n);
    return 0;
}

int rnet_session_take_rb_sync(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *mismatch_tick,
                              rnet_u32 *load_tick, rnet_u32 *target_tick,
                              rnet_u8 *corrected_slot, rnet_u8 *op, rnet_u8 *flags)
{
    if (s == NULL || s->rb_sync_count <= 0)
        return 0;
    if (epoch_id)
        *epoch_id = s->rb_sync_q[s->rb_sync_tail].epoch_id;
    if (mismatch_tick)
        *mismatch_tick = s->rb_sync_q[s->rb_sync_tail].mismatch_tick;
    if (load_tick)
        *load_tick = s->rb_sync_q[s->rb_sync_tail].load_tick;
    if (target_tick)
        *target_tick = s->rb_sync_q[s->rb_sync_tail].target_tick;
    if (corrected_slot)
        *corrected_slot = s->rb_sync_q[s->rb_sync_tail].corrected_slot;
    if (op)
        *op = s->rb_sync_q[s->rb_sync_tail].initiator;
    if (flags)
        *flags = s->rb_sync_q[s->rb_sync_tail].flags;
    s->rb_sync_tail = (s->rb_sync_tail + 1) % RNET_RB_CTRL_QUEUE;
    s->rb_sync_count--;
    return 1;
}

int rnet_session_send_rb_seal_rows(RNetSession *s, rnet_u32 epoch_id, rnet_u32 mismatch_tick,
                                   rnet_u32 target_tick, rnet_u8 slot, rnet_u32 row_begin,
                                   const RNetRbFrame *rows, rnet_u16 row_count)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    RNetRbWireFrame wire[RNET_RB_SEAL_ROWS_CHUNK_MAX];
    rnet_u16 i, n;
    int enc;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING || rows == NULL)
        return -1;
    n = row_count;
    if (n > RNET_RB_SEAL_ROWS_CHUNK_MAX)
        n = RNET_RB_SEAL_ROWS_CHUNK_MAX;
    for (i = 0; i < n; ++i)
    {
        wire[i].buttons = rows[i].buttons;
        wire[i].stick_x = (rnet_s8)rows[i].stick_x;
        wire[i].stick_y = (rnet_s8)rows[i].stick_y;
        wire[i].source = rows[i].analog ? 1u : 0u;
        wire[i].is_predicted = rows[i].is_predicted;
        wire[i].is_valid = rows[i].is_valid;
    }
    enc = rnet_proto_encode_rb_seal_rows(buf, sizeof(buf), s->cfg.protocol_magic,
                                         s->cfg.session_id, s->cfg.local_slot, epoch_id,
                                         mismatch_tick, target_tick, slot, row_begin, wire, n);
    if (enc <= 0)
        return -1;
    send_raw(s, buf, enc);
    return 0;
}

int rnet_session_take_rb_seal_rows(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *mismatch_tick,
                                   rnet_u32 *target_tick, rnet_u8 *slot, rnet_u32 *row_begin,
                                   RNetRbFrame *rows, rnet_u16 *row_count)
{
    rnet_u16 i, n;
    if (s == NULL || s->rb_seal_count <= 0)
        return 0;
    if (epoch_id)
        *epoch_id = s->rb_seal_q[s->rb_seal_tail].epoch_id;
    if (mismatch_tick)
        *mismatch_tick = s->rb_seal_q[s->rb_seal_tail].mismatch_tick;
    if (target_tick)
        *target_tick = s->rb_seal_q[s->rb_seal_tail].target_tick;
    if (slot)
        *slot = s->rb_seal_q[s->rb_seal_tail].slot;
    if (row_begin)
        *row_begin = s->rb_seal_q[s->rb_seal_tail].row_begin;
    n = s->rb_seal_q[s->rb_seal_tail].row_count;
    if (rows && n > 0)
    {
        for (i = 0; i < n; ++i)
        {
            rows[i].tick = s->rb_seal_q[s->rb_seal_tail].row_begin + i;
            rows[i].buttons = s->rb_seal_q[s->rb_seal_tail].rows[i].buttons;
            rows[i].stick_x = (int8_t)s->rb_seal_q[s->rb_seal_tail].rows[i].stick_x;
            rows[i].stick_y = (int8_t)s->rb_seal_q[s->rb_seal_tail].rows[i].stick_y;
            rows[i].analog = s->rb_seal_q[s->rb_seal_tail].rows[i].source ? 1u : 0u;
            rows[i].is_predicted = s->rb_seal_q[s->rb_seal_tail].rows[i].is_predicted;
            rows[i].is_valid = s->rb_seal_q[s->rb_seal_tail].rows[i].is_valid;
        }
    }
    if (row_count)
        *row_count = n;
    s->rb_seal_tail = (s->rb_seal_tail + 1) % RNET_RB_CTRL_QUEUE;
    s->rb_seal_count--;
    return 1;
}

int rnet_session_send_rb_baseline(RNetSession *s, rnet_u32 epoch_id, rnet_u32 load_tick,
                                  rnet_u32 digest_master, rnet_u32 digest_a, rnet_u32 digest_b,
                                  rnet_u32 digest_c)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
        return -1;
    n = rnet_proto_encode_rb_baseline(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                      s->cfg.local_slot, epoch_id, load_tick, digest_master,
                                      digest_a, digest_b, digest_c);
    if (n <= 0)
        return -1;
    send_raw(s, buf, n);
    return 0;
}

int rnet_session_take_rb_baseline(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *load_tick,
                                  rnet_u32 *digest_master, rnet_u32 *digest_a, rnet_u32 *digest_b,
                                  rnet_u32 *digest_c)
{
    if (s == NULL || s->rb_base_count <= 0)
        return 0;
    if (epoch_id)
        *epoch_id = s->rb_base_q[s->rb_base_tail].epoch_id;
    if (load_tick)
        *load_tick = s->rb_base_q[s->rb_base_tail].load_tick;
    if (digest_master)
        *digest_master = s->rb_base_q[s->rb_base_tail].digest_master;
    if (digest_a)
        *digest_a = s->rb_base_q[s->rb_base_tail].digest_a;
    if (digest_b)
        *digest_b = s->rb_base_q[s->rb_base_tail].digest_b;
    if (digest_c)
        *digest_c = s->rb_base_q[s->rb_base_tail].digest_c;
    s->rb_base_tail = (s->rb_base_tail + 1) % RNET_RB_CTRL_QUEUE;
    s->rb_base_count--;
    return 1;
}

int rnet_session_send_rb_post(RNetSession *s, rnet_u32 epoch_id, rnet_u32 target_tick,
                              rnet_u32 digest_master, rnet_u32 input_digest, rnet_u8 match)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
        return -1;
    n = rnet_proto_encode_rb_post(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                  s->cfg.local_slot, epoch_id, target_tick, digest_master,
                                  input_digest, match);
    if (n <= 0)
        return -1;
    send_raw(s, buf, n);
    return 0;
}

int rnet_session_take_rb_post(RNetSession *s, rnet_u32 *epoch_id, rnet_u32 *target_tick,
                              rnet_u32 *digest_master, rnet_u32 *input_digest, rnet_u8 *match)
{
    if (s == NULL || s->rb_post_count <= 0)
        return 0;
    if (epoch_id)
        *epoch_id = s->rb_post_q[s->rb_post_tail].epoch_id;
    if (target_tick)
        *target_tick = s->rb_post_q[s->rb_post_tail].target_tick;
    if (digest_master)
        *digest_master = s->rb_post_q[s->rb_post_tail].digest_master;
    if (input_digest)
        *input_digest = s->rb_post_q[s->rb_post_tail].input_digest;
    if (match)
        *match = s->rb_post_q[s->rb_post_tail].match;
    s->rb_post_tail = (s->rb_post_tail + 1) % RNET_RB_CTRL_QUEUE;
    s->rb_post_count--;
    return 1;
}

int rnet_session_send_sio_multi_xfer(RNetSession *s, rnet_u8 unit_id, rnet_u32 seq,
                                     rnet_u16 send, rnet_u16 confirm_pad)
{
    rnet_u8 buf[64];
    int n;
    if (s == NULL || s->transport.mode == RNET_TRANSPORT_NONE)
        return -1;
    /* Allow during READY/RUNNING so Cable Club can start as soon as UDP is up. */
    if (s->phase != RNET_PHASE_RUNNING && s->phase != RNET_PHASE_READY)
        return -1;
    n = rnet_proto_encode_sio_multi_xfer(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                         s->cfg.local_slot, unit_id, seq, send, confirm_pad);
    if (n <= 0)
        return -1;
    /* Small redundancy: Multi barrier cannot hide behind INPUT FEC. */
    send_raw(s, buf, n);
    send_raw(s, buf, n);
    return 0;
}

int rnet_session_poll_sio_multi_xfer(RNetSession *s, rnet_u8 *unit_id, rnet_u32 *seq,
                                     rnet_u16 *send, rnet_u16 *confirm_pad)
{
    if (s == NULL || s->sio_xfer_count <= 0)
        return 0;
    if (unit_id)
        *unit_id = s->sio_xfer_q[s->sio_xfer_tail].unit_id;
    if (seq)
        *seq = s->sio_xfer_q[s->sio_xfer_tail].seq;
    if (send)
        *send = s->sio_xfer_q[s->sio_xfer_tail].send;
    if (confirm_pad)
        *confirm_pad = s->sio_xfer_q[s->sio_xfer_tail].confirm_pad;
    s->sio_xfer_tail = (s->sio_xfer_tail + 1) % RNET_SIO_XFER_QUEUE;
    s->sio_xfer_count--;
    return 1;
}

int rnet_session_send_rb_resolved(RNetSession *s, rnet_u32 resolved_through)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    int n;
    if (s == NULL || s->phase != RNET_PHASE_RUNNING)
        return -1;
    n = rnet_proto_encode_rb_resolved(buf, sizeof(buf), s->cfg.protocol_magic, s->cfg.session_id,
                                      s->cfg.local_slot, resolved_through);
    if (n <= 0)
        return -1;
    send_raw(s, buf, n);
    return 0;
}

int rnet_session_take_rb_resolved(RNetSession *s, rnet_u32 *resolved_through)
{
    if (s == NULL || s->rb_resolved_count <= 0)
        return 0;
    if (resolved_through)
        *resolved_through = s->rb_resolved_q[s->rb_resolved_tail];
    s->rb_resolved_tail = (s->rb_resolved_tail + 1) % RNET_RB_CTRL_QUEUE;
    s->rb_resolved_count--;
    return 1;
}
