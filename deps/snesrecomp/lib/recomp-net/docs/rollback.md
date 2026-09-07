# Rollback mode (feat/rollback)

This branch extends recomp-net beyond delay-sync into shared rollback
architecture. Delay-sync `RNetSession` remains the shipped v0.1 path; rollback
lands here in layers so hosts opt in without breaking MotK / snes / psx titles.

## Layers

| Layer | Status | Location |
|-------|--------|----------|
| Portable input contract | Landed | [`include/recomp_net/input_contract.h`](../include/recomp_net/input_contract.h), [`src/input/rnet_input_contract.c`](../src/input/rnet_input_contract.c) |
| Rollback episode orchestration | Landed | [`include/recomp_net/rollback.h`](../include/recomp_net/rollback.h), [`src/rollback/rnet_rollback.c`](../src/rollback/rnet_rollback.c) |
| Rollback wire protocol | Landed | `RNET_PKT_RB_*` (opcodes 20–25) in [`src/protocol/rnet_protocol.{h,c}`](../src/protocol/rnet_protocol.h) |

## Rollback wire protocol

Additive, non-colliding opcode range for rollback-mode sessions. Delay-sync
hosts never emit or parse these. Seal rows are fixed 7-byte frames
(`RNetRbWireFrame`: buttons 2 + sticks 2 + source + predicted + valid) with the
tick derived from `row_begin + index`.

| Opcode | Packet | Payload |
|--------|--------|---------|
| 20 | `RB_SYNC` | correction tuple `(epoch, mismatch, load, target, slot, op, flags)` |
| 21 | `RB_SEAL_ROWS` | peer-authority sealed rows chunk (tuple + rows) |
| 22 | `RB_BASELINE` | post-load digests (master + 3 partitions) for the baseline gate |
| 23 | `RB_POST` | post-replay digests + match flag (commit / deepen / abort) |
| 24 | `RB_FRAME_COMMIT` | state/master-hash watermark agreement token |
| 25 | `RB_RESOLVED` | resolved-through / shared frontier advertise |

`RB_SYNC` carries an op code (the byte historically named `initiator`) and a
flags byte (`recomp_net/session.h`):

- `OP_BEGIN` — open or tip-extend an episode. Flags are
  initiator-authoritative episode attributes the follower adopts verbatim:
  `FLAG_LIGHT_TIP` (episode class; keeps baseline burst counts symmetric) and
  `FLAG_REREPLAY` (tip-extend requires re-replay from load — the follower
  mirrors the initiator's decision instead of re-deriving from its own tip).
- `OP_NACK` — follower refuses/cannot follow. The target field carries the
  follower's confirmed frontier so the initiator demotes its watermark to a
  mutually provable tick instead of guessing `load-1`.
- `OP_ABORT` — sender tore its episode down. The mismatch field carries the
  shared `RNET_RB_ABORT_CLASS_*` cooldown class, the load field the sender's
  realign tick; the receiver mirrors teardown + cooldown so both peers re-arm
  on the same schedule.

Hosts should partition epoch ids by initiator slot
(`epoch = counter << k | slot`) so concurrent dual initiation never collides
and a deterministic tie-break (lower initiator slot wins; the loser yields and
follows) can derive any episode's initiator from its id.

Hosts map their existing wire onto these when aligning transports (BattleShip's
soak-hardened `SYNETPEER_*` format stays authoritative for live matches); new
recomp-net rollback hosts may use `RNET_PKT_RB_*` natively. Transport/ICE
unification is a follow-up — the delay-sync ICE path is unchanged.

## Rollback episode orchestration

`RNetRbSession` owns the episode FSM (`Live → SealInputs → AwaitingBaseline →
Replay → Verify → Commit|Abort`), the correction tuple, the sealed input table,
and the resolved-through (shared frontier) watermark. The host owns snapshots,
the deterministic sim step, state digests, and the wire transport.

### Tip-extend / edge coalesce

`rnet_rb_extend_target` grows `target_tick` and appends seal rows so a late
wire edge (typical digital press then release) stays in the **same** episode
instead of a second seal/baseline handshake. Verify→extend drops to Replay;
TipHold→extend **stays TipHold** (host invent-caps Live past tip and
schedules a short rereplay only when sim already invented past the prior
tip). `rnet_rb_resign_slot_range` refreshes sealed rows after the host
promotes wire into history. `apply_peer_seal_rows` accepts
`target >= corr.target` and auto-extends; it also refuses to let a
**predicted** peer row overwrite a non-predicted sealed row (mask bit is
still credited) so tip-extend invent-idle exports cannot clobber a
wire-promoted resign. `rnet_rb_suggest_target` adds
`cfg.tip_seal_slack` past live tip when opening an episode.
`resolved_through` survives `session_reset`.

### Light tip

When `(target - load) <= cfg.light_tip_max_depth` (0 → the library default
`RNET_RB_LIGHT_TIP_MAX_DEPTH`, 16) and `load` is at/after `resolved_through`,
`begin_episode` sets `RNET_RB_CORR_LIGHT_TIP`. Hosts may skip the ready-ACK
RTT (digests still compared) and shrink baseline bursts.

Hosts that widen `cfg.tip_runway` for TipHold coalescing (see above) should
also raise `cfg.light_tip_max_depth` to match — a coalesced episode's
eventual depth (load..target after one or more `tip_extend` calls) can
approach `tip_runway`, and if that exceeds the light-tip ceiling the episode
silently falls back to the full ready-ACK round trip even though nothing
actually went wrong. A MotK soak with `tip_runway=24` and the untouched
library default (`16`) lost the light-tip fast path on ~19% of episodes for
exactly this reason before `light_tip_max_depth` was added and set to match.
`rnet_rb_is_light_tip_candidate_ex(load, target, resolved_through, max_depth)`
lets a host precompute the flag with an explicit ceiling before a session
exists (or before `corr` is populated); `rnet_rb_is_light_tip_candidate` is a
thin wrapper using the plain library default for hosts that don't override
`light_tip_max_depth`.

`rnet_rb_demote_resolved_through(s, tick)` pulls the shared frontier down
when a follow-NACK refuses a unilateral tip. `rnet_rb_set_peer_convergence`
only advances, and `session_reset` keeps `resolved_through` — without an
explicit demote the refused tip stays as the library watermark and the next
light-tip / host HC advance reopens it. Hosts that demote their own agreed
watermark on NACK should demote the library watermark in the same step.

Required `RNetRollbackVTable` callbacks: `save_state` / `load_state` /
`advance_sim` / `get_input_row` (+ `state_digest`, `hash_confirm_through`).
Host stick gates ride through `stick_gates` and feed
`rnet_rb_decide_stick_replace`.

Minimal host loop during an episode:

```c
rnet_rb_begin_episode(s, &corr);                 /* mismatch identified */
rnet_rb_seal_inputs(s, corr.mismatch_tick, corr.target_tick, corr.slot);
/* exchange peer seal rows via host transport: rnet_rb_export_seal_rows_chunk /
 * rnet_rb_apply_peer_seal_rows until rnet_rb_all_peer_seal_rows_complete */
rnet_rb_set_phase(s, nRNetRbPhaseAwaitingBaseline);
vt.load_state(vt.ctx, corr.load_tick);
for (t = corr.load_tick; t <= corr.target_tick; ++t)
    vt.advance_sim(vt.ctx, t);                   /* reads sealed rows */
/* compare vt.state_digest against peer; then: */
rnet_rb_on_post_match(s);                        /* or rnet_rb_on_post_diverge */
```

The library is transport-agnostic in Phase 2 — BattleShip's `netpeer.c` calls
these entry points from its existing packet ingress. Protocol/ICE opcodes are
Phase 3.

## Portable input contract

Pure decision core (no engine includes) for "published row vs late authoritative
row → rewind or promote?". All game-specific behavior enters through
`RNetInputContractParams` (numeric thresholds) and `RNetInputContractHostGates`
(optional host callbacks queried lazily in decision order).

Master entry point:

```c
RNetInputContractDecision d = rnet_input_contract_stick_replace_decide(
    &published, &wire, completed_sim, &params, &gates);
if (rnet_input_contract_decision_is_rewind(d)) { /* queue rollback */ }
```

Host-binding guidance (master-hash + savestate recomp host): bind only
`hash_confirm_promote` (= "peer master hash agreed through tick") and leave the
rest NULL (portable defaults).

## Invariants (soak-derived; do not relax without a new soak)

- Predicted rows never get a bare deadband promote — only `hash_confirm` or a
  host protect.
- Release always rewinds on both completed-sim and runway paths.
- Dash-gate X disagree blocks all same-intent promotes.
- `hash_confirm_promote` fails closed when NULL.

## Host guarantees for rollback mode

- Deterministic `advance_sim` for a given input set.
- Snapshot save/load at any sim tick the library requests.
- State digests suitable for agreement comparison across peers.
- Single-thread session ownership (same as delay-sync).

## Not in this library

- Automatch / lobby server (see `recomp-net-server`).
- Game-specific pad layouts, snapshots, or determinism fixes.
- Delay-sync behavior changes (existing `RNetSession` is untouched).
