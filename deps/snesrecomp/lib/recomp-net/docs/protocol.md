# Protocol

All multi-byte integers are **little-endian**. Every packet ends with a 32-bit
FNV-1a-style checksum over the preceding bytes (`rnet_proto_checksum`).

## Common header

| Field | Size | Notes |
|-------|------|-------|
| magic | u32 | Default `0x524E4554` (`RNET`) |
| type | u16 | Packet id |
| session_id | u32 | Must match config |

## Packet types

### HELLO (1)

`local_slot : u8`, `slot_count : u8`, `delay : u8`, `pad : u8`

Peers discover each other and confirm slot layout / advertised delay.

### READY (2)

`local_slot : u8`, `pad : u8×3`

Barrier before start. Session marks the sender ready.

### START (3)

`start_tick : u32`

Emitted by slot 0 when all slots are ready. Sets `sim_tick` and enters
`RUNNING`.

### INPUT (4)

`local_slot : u8`, `frame_count : u8`, `input_epoch : u16` (LE), `ack_tick : u32`,
then `frame_count` frames:

| Field | Size |
|-------|------|
| tick | u32 (wire tick) |
| size | u16 |
| bytes | `size` (≤ `RNET_INPUT_MAX`) |

Bundles retransmit recent local wire rows (`bundle_redundancy`).
`input_epoch` bumps on `hard_resync` (post-load); receivers drop other-epoch
packets so in-flight tips cannot first-wins into the new `sim_tick=0` window.

### DELAY_SYNC (5)

`new_delay : u8`, `pad : u8×3`, `effective_tick : u32`

Optional mid-session delay change. Receivers queue the change when
`effective_tick` is still in the future and commit when `sim_tick` reaches
that tick (both peers via `rnet_session_advance` / `set_sim_tick`). Applied
immediately when already past `effective_tick` or while not RUNNING.
`rnet_session_request_delay_change` schedules + emits; MotK uses this for
always-on adaptive delay bumps after sustained prediction-runway freezes.

### INPUT_CONFIRM (6)

`local_slot : u8`, `input_epoch : u16` (LE), `pad : u8`, `sim_tick : u32`,
`input_hash : u32`

Peers agree on the resolved pad set for `sim_tick` before publish/advance.
`input_hash` is `rnet_proto_checksum` over `sim_tick` (LE u32) followed by each
slot's `size` (LE u16) and `bytes`. Session latches local/remote wire rows
(first-wins) so late retransmits cannot change the hash mid-confirm.
Mismatch flags an input desync; agreement across all slots allows admission.
Same `input_epoch` rule as INPUT.

### BYE (7)

`local_slot : u8`, `pad : u8×3`

Graceful leave. Best-effort UDP (hosts may retransmit a few times on shutdown).
Peer marks the sender gone and can exit without waiting for the RX timeout.

### STATE_BEGIN (8) / STATE_CHUNK (9) / STATE_ACK (10)

Host→guest chunked blob transfer (savestate / memcard / SRAM). Cap:
`RNET_STATE_MAX` (8 MiB). Chunk payload ≤ `RNET_STATE_CHUNK_MAX` (1120;
fits in `RNET_MAX_PACKET` with header+checksum). ICE/TURN uses AIMD pacing
(start ~32 KiB / 16 chunks, max 256 KiB / 64 chunks, sticky warm-start) so
multi‑MB MotK `.pst` transfers do not crawl on Force TURN.

**BEGIN:** `local_slot`, `op`, `slot`, `pad`, `xfer_id : u32`, `total_size : u32`,
`payload_crc : u32` (`rnet_proto_checksum` over the full blob).

**CHUNK:** `local_slot`, pad×3, `xfer_id`, `offset : u32`, `size : u16`, pad u16,
`data[size]`.

**ACK:** `local_slot`, pad×3, `xfer_id`, `ack_bytes : u32` (contiguous bytes from 0).

Guest marks ready only after full contiguous receive **and** CRC match. Admit
stalls for the whole transfer.

`op`: `0=SAVE`, `1=LOAD`, `2=SRAM`.

### STATE_PROBE (11) / STATE_PROBE_REPLY (12)

Hash-agree before transfer. Host announces; guest replies; skip BEGIN/CHUNK when
identical.

**PROBE:** `local_slot`, `op`, `slot`, `pad`, `total_size : u32`, `payload_crc : u32`.

- `op=SAVE`, `total_size == 0`: coordinate local save first (guest ACKs when its
  local write is done). Does **not** stall admit (deferred saves must still
  reach a block boundary).
- `op=LOAD`, `total_size == 0`: post-load ready rendezvous (not a content hash).
  Does **not** stall INPUT (late applier still needs tip rows); the app freezes
  sim until mutual ready + `hard_resync`.
- Hash probes (`total_size != 0`): stall until agree or transfer.

**PROBE_REPLY:** `local_slot`, `op`, `slot`, `match : u8`, `total_size : u32`,
`payload_crc : u32`. The size/crc **echo the probe being answered** so a late
SAVE-coord or LOAD-ready ACK cannot satisfy a subsequent hash probe that shares
the same `op`/`slot`.

On hash miss the host starts STATE_BEGIN.

After a LOAD restore, both peers ACK a ready probe, then each calls
`rnet_session_hard_resync` (clear local **and** remote rings, `sim_tick → 0`)
and `rnet_session_prime_delay_inputs` once at mutual ready — not at apply
time. Both stay in the app load barrier until `try_admit` succeeds (fresh
tip + INPUT_CONFIRM). Ready-probe retransmit interval is 8 ms.

## Wire vs sim

Hosts reason in **sim ticks**. Inputs on the wire are indexed by
`wire = sim + D`. Admission for sim `T` requires remote rows at wire `T + D`,
then INPUT_CONFIRM hash agreement on the resolved set.
