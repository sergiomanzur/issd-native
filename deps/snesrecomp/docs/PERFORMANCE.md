# SNES performance burn-down

This worktree exists to create enough uncapped headroom for lower-power hosts,
including the original Xbox, without weakening the faithful SNES hardware
model. Performance is measured as uncapped frames per second, not as the
ability to sit at a paced 60 FPS.

The approach is adapted from the useful NDS work:

- measure complete, repeatable workloads rather than boot time;
- attribute time before changing architecture;
- compile diagnostic hot-path work out of production;
- move invariant work from pixels to tiles or spans;
- preserve a faithful fallback when adding a specialized fast path;
- use interleaved, order-balanced A/B runs and retain rejected experiments;
- require state, framebuffer, audio, attract-mode, gameplay, and fuzz gates.

## Current baseline

Super Mario World and Mega Man X already provide
`--benchmark <frames> <rom>`. It disables pacing, VSync, audio, autosave, and
the launcher and emits a machine-readable `SNESRECOMP_BENCHMARK` record. These
two games are the initial control workloads. Before accepting broad changes,
the benchmark surface should be shared with more titles.

The first audit also found several costs that are not part of emulating SNES
hardware:

- `RtlRunFrame` hashes all 128 KiB of WRAM every frame for the always-on
  determinism ring.
- `ppudma_frame_snapshot` scans 256 CGRAM entries and 32,768 VRAM words every
  frame, in addition to retaining the PPU and DMA rings.
- the indirect-dispatch path writes an always-on 1,024-entry diagnostic ring;
- the audio path records every native DSP output sample into a 16 MiB PCM
  history and records DSP writes and host-consume events into an 8 MiB event
  history.

These are high-confidence first measurements, not automatic deletions. Some
aggregate counters may be needed for audio pacing or reports even when the
large histories are disabled. Split functional state and cheap production
health counters from forensic history before measuring the production-off
path.

The PPU is about 97 KiB because it implements multiple hardware modes, color
math, windows, mosaic, large tiles, sprites, overlays, and the shared
widescreen capability. Its size is not by itself evidence of waste. Mode 1's
ordinary 2bpp and 4bpp paths already operate a tile at a time. Several less
common paths remain scalar per pixel, including 8bpp, large tiles, offset-
per-tile modes, mosaic, and Mode 7. Optimize the measured modes in place; do
not collapse specialized paths into a single abstraction unless performance
and exactness both survive.

## Measurement contract

1. Use Release builds from the same compiler and source revision.
2. Build at BelowNormal priority and with one build job on this machine.
3. Warm both binaries, then run at least five order-balanced A/B pairs.
4. Report paired deltas and raw medians. Reject contaminated samples.
5. Use a fixed frame interval that reaches stable attract or gameplay, not
   only logos and boot.
6. Record native and enabled-widescreen results separately.
7. Add timing buckets for guest CPU, PPU, APU/DSP, coprocessor, host
   presentation, and non-hardware diagnostics. Timing must be optional or
   compiled out for the final A/B.
8. Pin framebuffer/state hashes and dispatch/interpreter miss counts for every
   compared run. Audio-enabled correctness runs are separate from the
   audio-disabled throughput benchmark.

A candidate is retained when the order-balanced median is positive outside
run noise and the full correctness gates pass. Code-size changes are reported
because instruction-cache pressure matters on the Xbox.

## Retained results

### Production frame fingerprints

The first retained change compiles the full-WRAM fingerprint hash and its
8,192-entry ring out of trace-off production builds. Trace builds enable the
history by default, co-simulation forces it on, and an explicit
`SNESRECOMP_ENABLE_FRAME_FINGERPRINTS=ON` remains available independently.

Controls used MinGW GCC 15.2, SDL 3.4.12, Release `-O3`, native-width output,
audio disabled, and 3,000 fully simulated/rendered frames. Framework baseline
code was `868df63`; title revisions were SMW `98edb9e` and MMX `5324bf5`.
Five order-balanced pairs produced:

- Super Mario World: paired deltas `+2.719%, +19.139%, +18.447%, +12.624%,
  +6.677%`; paired median **+12.624%**. Raw medians were 278.870 FPS baseline
  and 322.397 FPS candidate.
- Mega Man X: paired deltas `+7.502%, +8.486%, +5.407%, +5.256%, +7.340%`;
  paired median **+7.340%**. Raw medians were 397.349 FPS baseline and
  421.703 FPS candidate.

Both titles produced byte-identical 128 KiB WRAM dumps at frame 2,999:
SMW SHA-256 `e395adef1c70fc59ab916f32c6d4e1ca9e4b420e16a5b2390b0bc661b45c8299`
(`crc32_wram=9c4e595c`) and MMX SHA-256
`31b29ca41c1108c97e4c491ad8bd5db65ede97924b9426530a3bda9dca5e632c`
(`crc32_wram=e0f6a7e2`). GNU `size` reports 65,536 fewer BSS bytes and 128
fewer text bytes in each candidate. A fresh trace configuration selected
fingerprints ON and successfully compiled both `common_rtl.c` and the real
`debug_server.c` at BelowNormal priority with one job.

### Production PPU/DMA forensic history

Trace-off production builds now omit the 4,096-frame PPU snapshot ring, the
8,192-event DMA ring, and the per-frame scan that counts non-zero values in
all 256 CGRAM entries and 32,768 VRAM words. The public recording API remains
present as no-op stubs so callers do not need configuration-dependent source.
Trace builds enable the full implementation by default, co-simulation forces
it on, and `SNESRECOMP_ENABLE_PPU_DMA_HISTORY=ON` enables it independently.

This smaller cost was near the noise floor of the current desktop harness.
Five order-balanced pairs produced:

- Super Mario World, 3,000 frames: paired deltas `+0.047%, +5.364%, +21.420%,
  -0.015%, +1.945%`; paired median **+1.945%**. Raw medians were 326.297 FPS
  baseline and 326.249 FPS candidate.
- Mega Man X, 6,000 frames: paired deltas `+8.765%, -7.804%, +1.554%,
  -5.071%, +5.336%`; paired median **+1.554%**. Raw medians were 505.134 FPS
  baseline and 494.419 FPS candidate.

The paired medians are positive, but the spread and contradictory raw medians
mean these measurements establish throughput neutrality rather than a precise
speedup. The production footprint result is unambiguous: GNU `size` reports
294,976 fewer BSS bytes, about 4 KiB less text, and 32 fewer data bytes in
each title. Both titles again produced byte-identical WRAM at frame 2,999
using the hashes above. A trace-on build selected the real history
implementation and compiled `ppu_dma_trace.c` successfully at BelowNormal
priority with one job.

### Production indirect-dispatch history

Production now retains the whole-run exact-AOT-hit and interpreter-miss
counters while omitting the 1,024-entry detailed indirect-dispatch ring.
Trace builds retain the ring by default, co-simulation forces it on, and
`SNESRECOMP_ENABLE_DISPATCH_HISTORY=ON` remains available independently.
Five order-balanced 3,000-frame pairs produced:

- Super Mario World: paired deltas `-24.894%, -0.130%, +10.993%, +1.595%,
  -4.095%`; paired median **-0.130%**. Raw medians were 319.768 FPS baseline
  and 319.353 FPS candidate, establishing that this cost is immaterial there.
- Mega Man X: paired deltas `-1.252%, +9.132%, +45.777%, +8.226%, +5.428%`;
  paired median **+8.226%**. Raw medians were 354.883 FPS baseline and
  374.147 FPS candidate. The third pair was visibly host-contaminated, but
  removing it still leaves three of four pairs positive and a positive median.

GNU `size` reports 24,576 fewer BSS bytes and about 1.7 KiB less text per
title. Both candidates produced byte-identical WRAM at frame 2,999 using the
same hashes as the preceding gates. A trace-on build selected and compiled
the real ring implementation.

## Rejected experiments

### Audio PCM/event/snapshot history culling

An experiment preserved functional pacing, sample-clock, occupancy, drop,
underflow, consume-quantum, and port-overwrite counters while compiling out
the 16 MiB PCM ring, 8 MiB event ring, snapshot ring, and per-sample snapshot
clock check. It was rejected before commit: five order-balanced 3,000-frame
SMW pairs had deltas `-5.073%, -6.266%, -6.497%, +13.572%, -1.379%` (median
**-5.073%**), and the first two MMX pairs were **-16.607%** and **-29.740%**.
That satisfies the two-negative-experiment stop condition. The large static
footprint saving is not sufficient reason to accept a repeatable throughput
regression; audio history remains unchanged pending attribution of why its
removal affects runtime layout or pacing.

## Burn-down

### P0 — harness and attribution

- [ ] Put the finite uncapped benchmark loop behind a shared runner API and
  wire it into public title worktrees rather than duplicating title logic.
- [ ] Add optional phase timing and production diagnostic counters to the
  benchmark JSON.
- [ ] Establish native-width SMW and MMX controls, then add one workload for
  each materially different hardware route: offset-per-tile, Mode 7/DSP-1,
  Super FX, Cx4, and SA-1.
- [ ] Store the exact commands, compiler, title/framework revisions, frame
  interval, ROM identity, and raw A/B records.

### P1 — production observability culling

- [x] Measure disabling the full-WRAM frame fingerprint while preserving an
  explicit diagnostic/cosim option.
- [x] Split the PPU/DMA forensic rings from cheap production counters. In
  particular, do not scan all VRAM and CGRAM every frame when forensic capture
  is absent.
- [ ] Split the audio PCM/event histories from the counters required for
  pacing, underrun reporting, and user-visible diagnostics.
- [x] Compile the dispatch-event history out of production while retaining
  hit/miss aggregates if they measure cheaply and remain useful.
- [x] Report executable and static/BSS size as well as throughput.

Each item is a separate commit and A/B. This phase should precede renderer or
CPU changes so later profiles describe emulation rather than diagnostics.

### P2 — PPU hot paths

- [ ] Attribute PPU time by mode and subpath before editing it.
- [ ] Apply the NES result where it actually fits: fetch tilemap entries and
  planar row data once per tile/span, then emit pixels. First candidates are
  scalar 8bpp and 16x16 large-tile paths.
- [ ] In offset-per-tile modes, keep offset lookup at authentic segment
  boundaries but hoist the selected tile and planar row out of the inner pixel
  loop.
- [ ] Measure native composition separately from widescreen shadow/policy and
  overlay work. Skip inactive enhancement policy early without changing the
  enabled path.
- [ ] Only investigate Mode 7, mosaic, sprite evaluation, color math, or a
  decoded-tile cache when attribution makes one material.
- [ ] Keep mode-specific paths. A DRY refactor is accepted only when generated
  code size, speed, and all mode-specific framebuffer gates are no worse.

This is an optimization of the existing emulated PPU, not a title-specific
replacement. Enhancements remain optional clients of the shared PPU and must
not alter native guest-visible state.

### P3 — CPU, bus, and generated dispatch

- [ ] Profile direct WRAM/ROM accesses, hardware-register routing, generated
  calls, binary-search dispatch, interpreter fallback, watchdog hooks, and
  cycle charging separately.
- [ ] Try header-inline fast paths only for mappings whose semantics are
  completely identical, retaining the shared slow path for MMIO, mutable code,
  tracing, and unusual cartridge mappings.
- [ ] Measure direct or cached dispatch only where profiles show repeated
  lookup cost. Preserve live M/X variant selection, bank mirroring, RAM guards,
  non-local returns, and interpreter fallback.
- [ ] Audit generated code for production-only trace/shadow-stack work, but do
  not remove guest stack semantics or host-return bookkeeping needed for
  correct control flow.
- [ ] Track executable growth and reject wins likely to regress the Xbox
  instruction cache.

### P4 — APU, DSP, DMA, and coprocessors

- [ ] Benchmark with audio disabled for host throughput and with audio enabled
  for end-to-end budget and correctness.
- [ ] Attribute SPC700 execution, DSP synthesis, resampling/callback work,
  catch-up synchronization, and trace history independently.
- [ ] Profile DMA replay and avoid duplicate routing only if the two hardware
  models remain state-identical.
- [ ] Treat DSP-1, Super FX, Cx4, and SA-1 independently. Keep each LLE core as
  the faithful floor; add a host fast path or HLE only after a representative
  title proves the core is material and an exact differential gate exists.

### P5 — release and regression sweep

- [ ] Enumerate title repositories with a public remote at validation time.
  A local title without a public repository is skippable.
- [ ] Build each public title in its own linked worktree, at BelowNormal
  priority and one job.
- [ ] Run its complete attract/demo loop where available, then a basic
  deterministic input fuzz that covers gameplay transitions. Use peripheral-
  specific fuzz for Super Scope/Mouse titles if public titles require it.
- [ ] Compare candidate and same-compiler baseline framebuffer hashes, WRAM
  fingerprints or equivalent state hashes, audio sample/hash gates, and
  dispatch/interpreter misses. Investigate every new miss.
- [ ] Include focused hardware coverage: ordinary Mode 1, windows/color math,
  offset-per-tile, Mode 7 plus DSP-1, Super FX, Cx4, SA-1, widescreen enabled,
  and native-width fallback.
- [ ] Run the framework unit, PPU, CPU differential, runtime-dispatch,
  coprocessor, and existing attract-demo regression suites.
- [ ] Document unavailable ROMs or other external blockers instead of silently
  treating them as passes.

## Stop conditions

Stop a line of work when two well-formed experiments are neutral or negative,
when its measured bucket is too small to repay the complexity, or when it
requires weakening exactness. Keep the faithful implementation and the
experiment record. After the high-confidence diagnostic and tile/span work,
re-profile before deciding whether CPU dispatch, audio, or a coprocessor
deserves the next round.
