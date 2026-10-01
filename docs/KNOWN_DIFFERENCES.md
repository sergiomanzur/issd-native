# ISSD Native — Known Differences & Behavioral Baseline

This document records selected architectural differences and measured behavior
against the original USA cartridge. It is not an exhaustive fidelity audit.
The current [supported feature inventory](SUPPORTED_FEATURES.md) and
[acceptance checklist](ACCEPTANCE_TESTS.md) separate working behavior, proposals
and unexecuted game/device checks.

---

## 1. Architectural Differences

| Component | Original SNES Hardware | ISSD Native | Notes |
| :--- | :--- | :--- | :--- |
| **CPU** | Ricoh 5A22 (65816 @ 3.58 MHz / 2.68 MHz) | Translated host routines, interpreter fallback and native replacements | Not a proven cycle-accurate timing recreation |
| **PPU** | Custom S-PPU1 / S-PPU2 (256x224, Mode 1/2/7) | Software rasterizer + SDL2 texture rendering | Integer/aspect viewport and CPU CRT effect; no configurable shader suite |
| **APU / Sound** | Sony SPC700 @ 1.024 MHz + S-DSP | Native SPC700/S-DSP models and host linear resampler | Bounded occupancy correction; measured limitations below |
| **Storage / Saves** | Password System only (no cartridge battery backup) | Validated binary snapshot envelopes and original password bridge | Manual slots and verified campaign checkpoints; config is separate |
| **Controls** | 2/4-player SNES Controller Ports | SDL2 GameController (XInput, DirectInput, DualShock, Switch) + Keyboard | Arbitrary remapping, analog stick deadzones, per-player configuration |

---

## 2. Timing & Frame Pacing

### One baseline engine
The legacy `engine_mode` enum is persisted but does not switch runtime timing.
The overlay identifies the engine as fixed. Earlier claims of cycle-accurate
Classic mode, speedrun preservation or an Enhanced mode that eliminates slowdown
were design assumptions, not delivered selectable implementations.

Simulation is scheduled at 60 Hz. Host performance can still cause slow frames.
Higher requested presentation rates repeat guest frames; they do not interpolate
poses or establish cartridge-equivalent timing under all load conditions.

---

## 3. Input Latency & Polling

- **Original SNES:** Joypad registers `$4218-$421F` read during automatic Joypad Read in VBlank (~line 225-227).
- **ISSD Native:** SDL2 events and sampled controls feed native joypad state. No end-to-end input-to-display measurement establishes sub-millisecond latency or a universal improvement over original hardware.

---

## 4. Audio Quality & Resampling

- **Native sound:** The SPC700 and S-DSP execute the cartridge's sound program. The runtime models 534 native stereo sample pairs per completed 60 Hz guest frame (32,040 pairs/second). Host callbacks consume PCM; they do not advance the guest sound clock.
- **Device conversion:** Linear interpolation converts native PCM to the obtained SDL device rate, normally 44.1 or 48 kHz. An occupancy servo adjusts consumption by at most ±0.5% to absorb small clock differences. This is not a band-limited resampler and cannot prevent starvation when simulation runs too slowly.
- **Starvation continuity:** A 64-output-frame fade continues across callback boundaries. Recovery joins the last emitted level even when samples resume before the fade reaches silence, avoiding an abrupt jump to zero.
- **Overflow continuity:** Accelerated boot and sound-upload handshakes can fill the 8,192-pair output FIFO and drop source samples. After an actual drop, only newly appended host PCM is joined to the last accepted pair over 64 native pairs (about 2 ms). The join occurs at the queued gap, leaving older queued audio, SPC/DSP execution, and raw trace samples unchanged. It suppresses a waveform seam; it does not restore missing audio or establish cartridge-perfect timing.
- **Measurements:** On this Windows machine, a muted real audio device at 48 kHz/1,024 pairs, 1,200 frames and auto-start at frame 180 produced 119 underflows with the original default 4× CPU scaling path and SDL's software renderer. The same original executable with 1× presentation produced zero underflows. An intermediate presentation fix reduced the software stress result to 43; these are diagnostic observations, not final accelerated-renderer guarantees. Sound-upload transitions also recorded roughly 7,000–9,000 audible native pairs dropped; the continuity fix intentionally leaves those raw counters truthful.
- **Current verification:** The final build completed 1,200-frame muted real-device runs with zero underflows in default software, minimal software, and default accelerated presentation. Accelerated runs still recorded roughly 7,700 audible native pairs dropped during bursty sound uploads; those gaps use the overflow join above rather than a discontinuous PCM edge.
- **Regression checks:** `python tests/test_audio_output.py` compiles the production resampler and FIFO enqueue/access code without a ROM. It checks continued starvation, early recovery, unchanged normal PCM, and the delayed FIFO overflow boundary. The failing fixtures exposed jumps of `14848 → 0` and `16000 → -16000` before their respective fixes. Host continuity metadata is outside the serialized DSP region; its serialized size remains 33,664 bytes on this x64 build.
- **Reproduction:** `python tests/test_audio_realtime.py --device --config default --frames 1200` opens the actual default audio device with volume zero, uses invisible software video, and saves logs/config in a fresh temporary directory. Repeat with `--config minimal` or `--config worst`; `--video accelerated` explicitly opens a native window. Without `--device`, the dummy audio backend is used and its timer is not a hardware clock reference. The user's configuration, mods, saves, and screenshots are not touched.

---

## 5. Compatibility & Regression Invariants

Regression checks exercise original physics paths, snapshot replay and bounded
optional AI target policies. They do not establish 100% gameplay fidelity or
equivalence for every collision, referee decision, mode and long tournament.
Gameplay tweaks intentionally alter selected CPU targets when enabled; both are
off by default. Deterministic replay checks cover their tested snapshot/input
sequences. Untouched campaigns, extra-time/shootout flow and physical device
acceptance remain explicit checks rather than inferred guarantees.
