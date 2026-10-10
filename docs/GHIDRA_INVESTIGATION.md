# Ghidra investigation for 1.0

Follow-up: the reproduced audio transfer-loss case was fixed after this
investigation; see [Audio timing fix](AUDIO_TIMING_FIX.md) for changes and validation.

Investigation performed 2026-10-08 against the current working tree. Production
behavior was not changed. Existing local edits were preserved. Reproduction
tools are in `tools/ghidra/`; ROM-derived data, Ghidra databases, snapshots and
captures stay in ignored `build/ghidra-investigation/`.

## Setup and scope

Ghidra 12.1.3 imported the verified USA ROM using the community Ghidra-SNES
loader and `65816:LE:24:snes`. Android Studio's Java 25 runtime supplied the
required Java version. The extension was loaded from a build-local directory;
it was not installed globally. See the tooling README for exact commands,
extension hash, compatibility adjustments and export validation.

The exports verify focused original instruction/data ranges. They are not a
whole-ROM analysis or recovered C. Existing assembly labels identify routines;
CPU width assumptions are explicitly supplied. SPC700 findings below use the
existing SPC assembly, not a Ghidra SPC decompilation.

## 1. Unsupported offscreen animation

**Finding: the examined unsupported states should not be added to the generic
timed-animation whitelist.** Their tables contain direct directional poses;
their handlers combine pose selection with gameplay decisions.

The original eight-byte dispatch rows at `81BD58` resolve state `80` through
`84C189` (row `81BDD8`) and state `50` through `84BF92` (row `81BDA8`).
`84C189` calls gameplay helpers and branches toward `84BFDC` or `848C27`.
The latter selects table `82DA46` and reaches `84E6AC`. That renderer indexes
a direct pose descriptor by direction. In contrast, `84E68F` interprets timed
animation sequences, cursor entries, duration and terminal markers.

The fresh Cup snapshot supplies a live witness for `84BF92`: four culled and
two visible actors in state `50` have direction zero and pose `B8C2`, matching
the first entry of direct table `82DA56`. The handler can update direction and
render through that table, while its other branches use flags/timers and
gameplay transitions. It does not establish an independent offscreen timed
sequence that presentation code can safely advance.

Evidence: `focused-rom-evidence.txt`, `static-animation-witness.json`, the
existing `ISSDNative/issd_animation.c` whitelist and original assembly.
Recommendation: retain the conservative hold for these handlers. This does
not certify every unsupported state or every direction/branch.

## 2. Audio mismatch

**Finding: native audio production can overflow the output FIFO and discard
signal-bearing PCM. This remains a concrete release investigation priority.**

After the campaign and natural-input processes exited, an isolated repeat of
the 1,200-frame default-device capture completed in 22.906 seconds with return
code zero: **921,159 produced pairs, 270,091 dropped pairs, 6,360 signal-bearing
dropped pairs, 644,304 consumed pairs and one underflow**. The device opened
and the frame target was reached. This confirms the overflow observation does
not depend on those concurrent campaign simulations. The signal loss amounts
to about 0.198 seconds of native PCM, distributed across the run, rather than
one continuous gap. No independent perceptual or original-PCM comparison was
made. See `audio-isolated.log` and `audio-probe/isolated-{stats,summary}.json`.

The default-device, muted, software-video 1,200-frame run opens the device and
reaches its target. A diagnostic executable uses the production objects except
for an exit exporter appended to a build-local copy of `audio_trace.c`.
It reports 922,395 produced native stereo pairs, 269,018 dropped pairs,
6,652 drops above the runtime signal threshold, 646,197 consumed pairs and
15 underflows. These are loaded-machine observations: campaign simulations
were also running. An uninstrumented production run independently recorded
signal-bearing drops, with zero underflows at its last one-second snapshot.

The retained event interval is 62,959–587,246; the fixed-size ring overwrote
earlier events. Within the retained interval, large drop bursts occur beside
sound-transfer traffic, including preceding guest frames 196, 488 and 648.
This establishes temporal association, not proof of a specific routine causing
every burst. A separate 180-frame boot-only capture retained all events and
dropped 163,278 pairs, all below the signal threshold, with zero underflows.
Boot silence and match-entry signal loss must therefore be distinguished.

The static trace identifies the relevant producer path:

- `80BF05`: queues BGM/SFX commands in the original FIFO.
- `80BF76`, `80BEEC`, `80C08F`, `80C115`: initiate sound/sample transfers,
  write data words through ports 2/3 and exchange tokens through port 1.
- `80C325` / `80C331`: original port echo polling loops.
- Generated polling replacements call `RtlApuWriteWaitEcho`, which advances
  SPC/DSP while waiting. The native DSP FIFO holds 8,192 pairs and drops new
  PCM when full; the seam ramp cannot restore discarded samples.

The boot log also records the port-0 `EF` timeout at SPC PC `0E34`, followed by
the runtime's fabricated echo fallback. The SPC assembly includes an initial
timer-driven delay before command processing, making startup readiness versus
the bounded wait a specific hypothesis to test. It is not yet a proven cause
of the later signal loss.

Next fix should first reproduce transfers on an otherwise idle machine and
separate guest-time advancement, transfer acknowledgement and device pacing.
Compare an independent original-game PCM/timing capture before claiming audio
fidelity. Increasing FIFO size or suppressing counters alone would not prove
the timing problem fixed. Current evidence is loss accounting and timing,
not a perceptual listening comparison.

## 3. Campaign and match transitions

The original result/transition paths were traced through `85B404`, `85D32E`,
`8B948B`–`8B950B` and the ceremony input dispatcher at `8BC8C0`.
Result callbacks resume after the original task-yield call at `858670`.
The Cup path updates standings and advances the round before selecting the
next match; World Series includes its final-round continuation branch.
Embedded dispatcher words were exported as data rather than instructions.

All nine Cup and 35 World Series result rounds completed, including their final
completion paths and new-process Continue checks. The campaign/game acceptance,
integration and transition suite passed **19 tests in 1,837.03 seconds**.
It also exercises an original drawn final/extra time, checkpoint compatibility
and setup/result transitions. No transition failure was reproduced.

The untouched-clock match reached halftime, second half, full time and a
committed result (campaign generation 1 → 2) within 83,101 simulated frames.
The sampled full-time score was 0–26. Subsequent original input reached
mode `0F` (the next match introduction). No WRAM clocks, scores, callbacks or tournament state were
patched. Evidence is `natural-match/progression.json` and `outcome.json`.

The campaign acceptance harness preserves original entrants, opponents and
progression and exercises a new-process Continue after each result. It edits
test-owned clocks and scores to shorten matches. The separate natural-input
probe does not edit either, but saves/reloads between 1,000-frame chunks.
Neither is a complete uninterrupted human campaign certification.

## Release impact

Focused animation, audio-output, handshake and native APU-reset checks passed
**5 tests and 7 subtests**. The production build succeeded. Python diagnostic
scripts passed compilation checks, and the Ghidra export completed with its
validated end marker. Production executable SHA-256:
`bcf2c3a40be8649feec1c80e3d12600db1a11f3590982ae8625bd48b715e09b6`.

Ghidra has been useful for deciding where a presentation patch would be unsafe
and for tying runtime audio symptoms to original transfer code. The evidence
supports prioritizing audio timing over expanding animation support for the
examined handlers. Tournament progression results apply only to the tested
flows; controller/device acceptance and uninterrupted campaigns remain separate
1.0 checks.

## Flow follow-up (2026-10-09)

Original shootout completion exposed a missing native autosave when the settled
ceremony returns directly to the title instead of visiting the ordinary final
Cup table. The narrowly guarded observer fix now passes ordinary and sudden-death
completion, title exit and Continue. Original substitution flows need no
production change: both sides, three-replacement admission, fourth rejection,
halftime and extra-time management pass. See [shootout evidence](SHOOTOUT_ACCEPTANCE.md),
[substitution evidence](SUBSTITUTION_FLOW_ACCEPTANCE.md) and
[continuous campaign evidence](CONTINUOUS_CAMPAIGN_ACCEPTANCE.md).
