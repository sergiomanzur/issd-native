# ISSD Native — Known Differences & Behavioral Baseline

This document tracks all intentional, architectural, and behavioral differences between the original SNES USA cartridge release of **International Superstar Soccer Deluxe** and **ISSD Native (Windows x86-64)**.

---

## 1. Architectural Differences

| Component | Original SNES Hardware | ISSD Native | Notes |
| :--- | :--- | :--- | :--- |
| **CPU** | Ricoh 5A22 (65816 @ 3.58 MHz / 2.68 MHz) | Native x86-64 Machine Code (Static Recompilation) | High-performance direct host execution; no 3.58 MHz CPU bottlenecks |
| **PPU** | Custom S-PPU1 / S-PPU2 (256x224, Mode 1/2/7) | Software scanline rasterizer + SDL2 Texture rendering | Integer scaling, aspect ratio correction, optional scanline shaders |
| **APU / Sound** | Sony SPC700 @ 1.024 MHz + S-DSP | Cycle-synchronized SPC700 emulation + S-DSP resampler | Crystal-rate sync via `RtlSetAudioOutputRate` to host sample rate (44.1k/48k) |
| **Storage / Saves** | Password System only (no cartridge battery backup) | Native persistent JSON/binary save files + Password encoder/decoder | Multiple save slots, autosaves, tournament progress persistence |
| **Controls** | 2/4-player SNES Controller Ports | SDL2 GameController (XInput, DirectInput, DualShock, Switch) + Keyboard | Arbitrary remapping, analog stick deadzones, per-player configuration |

---

## 2. Timing & Frame Pacing

### Classic Mode vs. Enhanced Mode
- **Classic Mode:**
  - Accurately reproduces original 65816 CPU load and frame times.
  - Matches original hardware behavior for speedruns and competitive preservation.
- **Enhanced Mode (Default):**
  - Eliminates accidental CPU cycle starvation slowdowns during crowded penalty box action (multiple player sprites, ball physics, radar updates).
  - Maintains a constant, rock-solid **60.0 Hz logical simulation clock**.
  - Presentation rendering is decoupled, enabling higher refresh rates (120 Hz, 144 Hz, 240 Hz) without affecting simulation determinism.

---

## 3. Input Latency & Polling

- **Original SNES:** Joypad registers `$4218-$421F` read during automatic Joypad Read in VBlank (~line 225-227).
- **ISSD Native:** Direct sub-millisecond SDL2 event polling polled each frame boundary, reducing input latency compared to physical hardware with CRT/scaler adapters.

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

To ensure 100% gameplay fidelity:
- Player collision bounding boxes, dribble vectors, shot arcs, deflection physics, and referee AI decisions execute identically to the original game.
- Deterministic RNG seeding is preserved for replayability.
