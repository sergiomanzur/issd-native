#include "recomp_net/rollback.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void expect_true(int cond, const char *msg)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_failures++;
    }
}

/* --- minimal host stub: deterministic input history + digest --- */

typedef struct TestHost
{
    uint32_t digest_at[256];
    uint8_t loaded_tick_valid;
    uint32_t loaded_tick;
} TestHost;

static int host_save_state(void *ctx, uint32_t tick)
{
    (void)ctx;
    (void)tick;
    return 0;
}

static int host_load_state(void *ctx, uint32_t tick)
{
    TestHost *h = (TestHost *)ctx;
    h->loaded_tick = tick;
    h->loaded_tick_valid = 1u;
    return 0;
}

static int host_advance_sim(void *ctx, uint32_t tick)
{
    (void)ctx;
    (void)tick;
    return 0;
}

static uint32_t host_state_digest(void *ctx, uint32_t tick, uint32_t partition)
{
    TestHost *h = (TestHost *)ctx;
    (void)partition;
    return (tick < 256u) ? h->digest_at[tick] : 0u;
}

static uint8_t host_hash_confirm_through(void *ctx, uint32_t tick)
{
    (void)ctx;
    (void)tick;
    return 0u;
}

static uint8_t host_get_input_row(void *ctx, int32_t slot, uint32_t tick, RNetRbFrame *out)
{
    (void)ctx;
    out->tick = tick;
    out->buttons = (uint16_t)(0x100u + (uint16_t)slot);
    out->stick_x = (int8_t)(10 + slot);
    out->stick_y = 0;
    out->is_predicted = 0u;
    out->is_valid = 1u;
    return 1u;
}

int main(void)
{
    RNetRbConfig cfg;
    RNetRollbackVTable vt;
    RNetRbSession *s;
    RNetRbCorrection corr;
    RNetRbFrame rows[8];
    RNetRbFrame got;
    uint32_t count;
    TestHost host;
    RNetRbEvent ev;

    memset(&host, 0, sizeof(host));
    memset(&cfg, 0, sizeof(cfg));
    cfg.local_slot = 0u;
    cfg.delay = 3u;
    cfg.slot_count = 2u; /* 2P — must not wait on unused slots 2..7 */

    memset(&vt, 0, sizeof(vt));
    vt.ctx = &host;
    vt.save_state = host_save_state;
    vt.load_state = host_load_state;
    vt.advance_sim = host_advance_sim;
    vt.state_digest = host_state_digest;
    vt.hash_confirm_through = host_hash_confirm_through;
    vt.get_input_row = host_get_input_row;

    s = rnet_rb_create(&cfg, &vt);
    expect_true(s != NULL, "create session");
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseLive, "starts live");
    expect_true(!rnet_rb_is_active(s), "live not active");

    /* Begin episode */
    memset(&corr, 0, sizeof(corr));
    corr.epoch_id = 7u;
    corr.mismatch_tick = 50u;
    corr.load_tick = 48u;
    corr.target_tick = 56u;
    corr.slot = 1;
    corr.initiator = 1u;
    rnet_rb_begin_episode(s, &corr);
    expect_true(rnet_rb_is_active(s), "episode active after begin");
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseSealInputs, "phase seal_inputs");
    expect_true(rnet_rb_get_mismatch_tick(s) == 50u, "mismatch tick");
    expect_true(rnet_rb_get_target_tick(s) == 56u, "target tick");
    expect_true(rnet_rb_get_corrected_slot(s) == 1, "corrected slot");

    /* Seal local rows (slot 0 = local authority). */
    rnet_rb_seal_inputs(s, corr.mismatch_tick, corr.target_tick, corr.slot);
    expect_true(rnet_rb_inputs_sealed(s), "inputs sealed");
    expect_true(rnet_rb_get_seal_span(s) == 7u, "seal span = 50..56 inclusive");
    expect_true(rnet_rb_tick_in_sealed_span(s, 53u), "tick 53 in span");
    expect_true(!rnet_rb_tick_in_sealed_span(s, 57u), "tick 57 outside span");
    expect_true(rnet_rb_get_sealed_frame(s, 0, 52u, &got), "local sealed row valid");
    expect_true(got.buttons == 0x100u, "local row buttons from history");
    expect_true(!rnet_rb_all_peer_seal_rows_complete(s), "peer rows not complete yet");

    /* Export local chunk for slot 0. */
    expect_true(rnet_rb_export_seal_rows_chunk(s, 0, 0u, 8u, rows, &count), "export local chunk");
    expect_true(count == 7u, "export full span");

    /* Apply peer rows for slot 1 (peer authority) across the span. */
    {
        uint32_t i;
        for (i = 0u; i < 7u; ++i)
        {
            rows[i].tick = corr.mismatch_tick + i;
            rows[i].buttons = 0x200u;
            rows[i].is_valid = 1u;
        }
    }
    /* Wrong epoch rejected. */
    expect_true(!rnet_rb_apply_peer_seal_rows(s, 99u, corr.mismatch_tick, corr.target_tick, 1, 0u,
                                              rows, 7u),
                "wrong epoch rejected");
    /* Invalid rows must not advance the peer-seal mask. */
    {
        RNetRbFrame bad[7];
        uint32_t i;
        for (i = 0u; i < 7u; ++i) {
            bad[i] = rows[i];
            bad[i].is_valid = 0u;
        }
        expect_true(rnet_rb_apply_peer_seal_rows(s, corr.epoch_id, corr.mismatch_tick,
                                                 corr.target_tick, 1, 0u, bad, 7u),
                    "invalid peer rows accepted as packet");
        expect_true(!rnet_rb_peer_seal_rows_complete(s, 1),
                    "invalid rows do not complete seal");
    }
    expect_true(rnet_rb_apply_peer_seal_rows(s, corr.epoch_id, corr.mismatch_tick, corr.target_tick,
                                             1, 0u, rows, 7u),
                "peer rows applied");
    expect_true(rnet_rb_peer_seal_rows_complete(s, 1), "slot 1 complete");
    /* With slot_count=2, peer slot 1 alone is enough — unused seats are ignored. */
    expect_true(rnet_rb_all_peer_seal_rows_complete(s), "all active peer seats sealed");
    expect_true(!rnet_rb_peer_seal_rows_complete(s, 2), "slot 2 incomplete (no rows)");
    expect_true(rnet_rb_get_sealed_frame(s, 1, 54u, &got), "peer sealed row retrievable");
    expect_true(got.buttons == 0x200u, "peer row buttons");

    /* FSM drive: baseline -> replay -> verify -> commit. */
    rnet_rb_set_phase(s, nRNetRbPhaseAwaitingBaseline);
    expect_true(rnet_rb_is_resimulating(s), "awaiting baseline counts as resim");
    rnet_rb_set_phase(s, nRNetRbPhaseReplay);
    expect_true(rnet_rb_is_resimulating(s), "replay resim");
    rnet_rb_set_phase(s, nRNetRbPhaseVerify);
    rnet_rb_on_post_match(s);
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseCommit, "commit on match");
    expect_true(rnet_rb_resolved_through(s) == corr.target_tick, "frontier advanced to target");

    /* Events. */
    memset(&ev, 0, sizeof(ev));
    ev.type = nRNetRbEventPeerSymmetric;
    ev.epoch_id = 7u;
    rnet_rb_enqueue_event(s, &ev);
    expect_true(rnet_rb_has_pending_events(s), "event queued");
    memset(&ev, 0, sizeof(ev));
    expect_true(rnet_rb_drain_next_event(s, &ev), "drain event");
    expect_true(ev.type == nRNetRbEventPeerSymmetric, "event type preserved");
    expect_true(!rnet_rb_has_pending_events(s), "queue drained");

    rnet_rb_session_reset(s);
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseLive, "reset to live");
    expect_true(!rnet_rb_inputs_sealed(s), "reset clears seal");

    /* --- Tip-extend + light-tip --- */
    cfg.tip_runway = RNET_RB_TIP_RUNWAY_DEFAULT;
    cfg.tip_seal_slack = RNET_RB_TIP_SEAL_SLACK_DEFAULT;
    rnet_rb_destroy(s);
    s = rnet_rb_create(&cfg, &vt);
    expect_true(s != NULL, "recreate with tip_runway");
    expect_true(rnet_rb_suggest_target(s, 100u, 102u) == 104u,
                "suggest_target = max(sim,mismatch)+tip_seal_slack");
    expect_true(rnet_rb_is_light_tip_candidate(100u, 104u, 100u),
                "light tip when load at frontier and depth<=16");
    expect_true(!rnet_rb_is_light_tip_candidate(80u, 104u, 100u),
                "not light tip when load behind frontier");
    expect_true(rnet_rb_get_light_tip_max_depth(s) == RNET_RB_LIGHT_TIP_MAX_DEPTH,
                "light_tip_max_depth defaults to the library constant when cfg leaves it 0");
    /* Demote after follow-NACK must pull the session watermark down —
     * set_peer_convergence only advances, and session_reset keeps the old
     * value (would otherwise re-poison light-tip / HC on the next pump). */
    rnet_rb_set_peer_convergence(s, 200u);
    expect_true(rnet_rb_resolved_through(s) == 200u, "convergence advances");
    rnet_rb_demote_resolved_through(s, 184u);
    expect_true(rnet_rb_resolved_through(s) == 184u, "demote pulls watermark down");
    rnet_rb_demote_resolved_through(s, 190u);
    expect_true(rnet_rb_resolved_through(s) == 184u,
                "demote is a no-op when tick >= current");
    rnet_rb_session_reset(s);
    expect_true(rnet_rb_resolved_through(s) == 184u,
                "session_reset preserves the demoted watermark");
    /* Restore a tip-aligned frontier for the begin/light-tip checks below. */
    rnet_rb_demote_resolved_through(s, 100u);
    expect_true(rnet_rb_resolved_through(s) == 100u, "demote to tip for later checks");

    /* A host that widens tip_runway for TipHold coalescing (e.g. MotK's 24)
     * must widen light_tip_max_depth to match, or coalesced episodes past
     * the library default of 16 silently lose the light-tip fast path. */
    expect_true(!rnet_rb_is_light_tip_candidate(100u, 120u, 100u),
                "plain wrapper still uses the library default (16) — depth 20 is not light");
    expect_true(!rnet_rb_is_light_tip_candidate_ex(100u, 120u, 100u, 16u),
                "explicit depth=16 ceiling also rejects depth 20");
    expect_true(rnet_rb_is_light_tip_candidate_ex(100u, 120u, 100u, 24u),
                "explicit depth=24 ceiling (matching a widened tip_runway) accepts depth 20");
    {
        RNetRbConfig wide_cfg = cfg;
        RNetRbSession *wide_s;
        wide_cfg.tip_runway = 24u;
        wide_cfg.light_tip_max_depth = 24u;
        wide_s = rnet_rb_create(&wide_cfg, &vt);
        expect_true(wide_s != NULL, "create session with widened light_tip_max_depth");
        expect_true(rnet_rb_get_light_tip_max_depth(wide_s) == 24u,
                    "session reports the configured (non-default) depth ceiling");
        /* Tip-aligned branch needs resolved_through set (fresh sessions start
         * at 0, which only allows depth<=2 regardless of the ceiling). */
        rnet_rb_set_peer_convergence(wide_s, 100u);
        memset(&corr, 0, sizeof(corr));
        corr.epoch_id = 1u;
        corr.mismatch_tick = 120u;
        corr.load_tick = 100u;
        corr.target_tick = 120u; /* depth 20: light under 24, not under default 16 */
        corr.slot = 1;
        corr.initiator = 1u;
        rnet_rb_begin_episode(wide_s, &corr);
        expect_true((rnet_rb_get_corr_flags(wide_s) & RNET_RB_CORR_LIGHT_TIP) != 0u,
                    "begin_episode honors the session's widened light_tip_max_depth");
        rnet_rb_destroy(wide_s);
    }

    /* Follower begins adopt wire flags verbatim: light-tip is initiator-
     * authoritative, never re-derived from the follower's own watermark. */
    {
        RNetRbSession *f = rnet_rb_create(&cfg, &vt);
        expect_true(f != NULL, "create follower session");
        rnet_rb_set_peer_convergence(f, 100u);
        memset(&corr, 0, sizeof(corr));
        corr.epoch_id = 9u;
        corr.mismatch_tick = 100u;
        corr.load_tick = 100u;
        corr.target_tick = 102u; /* would be a local light-tip candidate */
        corr.slot = 1;
        corr.initiator = 0u;
        corr.from_peer_notify = 1u; /* wire flags carried no LIGHT_TIP */
        rnet_rb_begin_episode(f, &corr);
        expect_true((rnet_rb_get_corr_flags(f) & RNET_RB_CORR_LIGHT_TIP) == 0u,
                    "follower does not re-derive light-tip locally");
        expect_true(rnet_rb_recommend_light_tip(f) == 0u,
                    "recommend_light_tip reports corr.flags only");
        rnet_rb_destroy(f);
    }

    memset(&corr, 0, sizeof(corr));
    corr.epoch_id = 8u;
    corr.mismatch_tick = 100u;
    corr.load_tick = 100u;
    corr.target_tick = 102u;
    corr.slot = 1;
    corr.initiator = 1u;
    rnet_rb_begin_episode(s, &corr);
    expect_true((rnet_rb_get_corr_flags(s) & RNET_RB_CORR_LIGHT_TIP) != 0u,
                "begin sets LIGHT_TIP flag");
    rnet_rb_seal_inputs(s, corr.load_tick, corr.target_tick, corr.slot);
    expect_true(rnet_rb_get_seal_span(s) == 3u, "seal 100..102");

    /* Peer completes original span. */
    {
        uint32_t i;
        for (i = 0u; i < 3u; ++i)
        {
            rows[i].tick = 100u + i;
            rows[i].buttons = 0x200u;
            rows[i].is_valid = 1u;
        }
    }
    expect_true(rnet_rb_apply_peer_seal_rows(s, 8u, 100u, 102u, 1, 0u, rows, 3u),
                "peer rows for original tip");
    expect_true(rnet_rb_all_peer_seal_rows_complete(s), "sealed before extend");

    rnet_rb_set_phase(s, nRNetRbPhaseReplay);
    expect_true(rnet_rb_extend_target(s, 105u), "extend target 102→105");
    expect_true(rnet_rb_get_target_tick(s) == 105u, "target grown");
    expect_true(rnet_rb_get_seal_span(s) == 6u, "span 100..105");
    expect_true(rnet_rb_tick_in_sealed_span(s, 105u), "new tip in span");
    expect_true(!rnet_rb_all_peer_seal_rows_complete(s),
                "extend clears peer completeness for new offsets");

    /* Peer tip-extend via apply with higher target + delta rows. */
    {
        uint32_t i;
        for (i = 0u; i < 3u; ++i)
        {
            rows[i].tick = 103u + i;
            rows[i].buttons = 0x201u;
            rows[i].is_valid = 1u;
        }
    }
    expect_true(rnet_rb_apply_peer_seal_rows(s, 8u, 100u, 105u, 1, 3u, rows, 3u),
                "peer delta rows auto-extend path");
    expect_true(rnet_rb_all_peer_seal_rows_complete(s), "complete after delta");
    expect_true(rnet_rb_get_sealed_frame(s, 1, 104u, &got) && got.buttons == 0x201u,
                "extended peer row");

    /* Predicted peer invent must not clobber an authoritative seal. */
    {
        RNetRbFrame auth;
        RNetRbFrame invent;
        memset(&auth, 0, sizeof(auth));
        auth.tick = 104u;
        auth.buttons = 0xFF7Fu;
        auth.is_valid = 1u;
        auth.is_predicted = 0u;
        expect_true(rnet_rb_apply_peer_seal_rows(s, 8u, 100u, 105u, 1, 4u, &auth, 1u),
                    "auth row at 104");
        memset(&invent, 0, sizeof(invent));
        invent.tick = 104u;
        invent.buttons = 0xFFFFu;
        invent.is_valid = 1u;
        invent.is_predicted = 1u;
        expect_true(rnet_rb_apply_peer_seal_rows(s, 8u, 100u, 105u, 1, 4u, &invent, 1u),
                    "predicted invent apply accepted (mask credit)");
        expect_true(rnet_rb_get_sealed_frame(s, 1, 104u, &got) && got.buttons == 0xFF7Fu &&
                        got.is_predicted == 0u,
                    "predicted invent did not clobber auth seal");
    }

    /* Resign after promote (same target, update buttons). */
    expect_true(rnet_rb_resign_slot_range(s, 1, 104u, 104u), "resign one tick");

    /* Verify → extend drops back to Replay. */
    rnet_rb_set_phase(s, nRNetRbPhaseVerify);
    expect_true(rnet_rb_extend_target(s, 106u), "extend from Verify");
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseReplay, "Verify→Replay on extend");

    /* TipHold → extend stays TipHold (host invent-caps Live; rereplay only
     * when sim already past prior tip). */
    rnet_rb_set_phase(s, nRNetRbPhaseVerify);
    expect_true(rnet_rb_enter_tip_hold(s), "enter TipHold for stay-phase check");
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseTipHold, "TipHold after enter");
    expect_true(rnet_rb_extend_target(s, 108u), "extend while TipHold");
    expect_true(rnet_rb_get_target_tick(s) == 108u, "TipHold tip grown");
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseTipHold,
                "TipHold stays TipHold on extend");
    expect_true(rnet_rb_extend_target(s, 108u), "no-op extend at tip");
    expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseTipHold,
                "TipHold stays TipHold on no-op extend");

    rnet_rb_destroy(s);

    /* --- Peer-seal bitmask span cap (TipHold coalesce wall) ---
     * MotK soak: tip-extend storm grows seal from load until span>64, then
     * extend fails and the host must tip-hold-commit + open a fresh episode. */
    {
        uint32_t load = 100u;
        uint32_t tip = load + RNET_RB_PEER_SEAL_MASK_BITS - 1u; /* span == 64 */
        uint32_t past = tip + 1u;                                 /* span == 65 */

        s = rnet_rb_create(&cfg, &vt);
        expect_true(s != NULL, "recreate for span-cap");
        memset(&corr, 0, sizeof(corr));
        corr.epoch_id = 9u;
        corr.mismatch_tick = load;
        corr.load_tick = load;
        corr.target_tick = load + 2u;
        corr.slot = 1;
        corr.initiator = 1u;
        rnet_rb_begin_episode(s, &corr);
        rnet_rb_seal_inputs(s, corr.load_tick, corr.target_tick, corr.slot);
        expect_true(rnet_rb_can_extend_target(s, tip), "can extend to span=64");
        expect_true(rnet_rb_extend_target(s, tip), "extend to peer-seal max");
        expect_true(rnet_rb_get_seal_span(s) == RNET_RB_PEER_SEAL_MASK_BITS,
                    "span saturates at 64");
        expect_true(!rnet_rb_can_extend_target(s, past), "cannot extend past mask");
        expect_true(!rnet_rb_extend_target(s, past), "extend past mask fails");
        expect_true(rnet_rb_get_target_tick(s) == tip, "target unchanged on fail");

        /* TipHold path: same refusal (host finalizes tip-hold on 0). */
        rnet_rb_set_phase(s, nRNetRbPhaseVerify);
        expect_true(rnet_rb_enter_tip_hold(s), "enter TipHold at span=64");
        expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseTipHold, "TipHold phase");
        expect_true(!rnet_rb_can_extend_target(s, past), "TipHold cannot grow past 64");
        expect_true(!rnet_rb_extend_target(s, past), "TipHold extend past 64 fails");
        expect_true(rnet_rb_get_phase(s) == nRNetRbPhaseTipHold,
                    "failed extend leaves TipHold (host commits)");
    }

    rnet_rb_destroy(s);

    if (g_failures == 0)
    {
        printf("rollback_episode_test: ok\n");
        return 0;
    }
    fprintf(stderr, "rollback_episode_test: %d failure(s)\n", g_failures);
    return 1;
}
