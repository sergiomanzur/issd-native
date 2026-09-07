# ISSD Native - Bring-Up Status Report

**Project:** ISSD Native (Static Recompilation of International Superstar Soccer Deluxe for Windows x86-64)  
**Date:** 2026-09-06  
**Milestone:** M0 / M1 / M2 First Native Execution Baseline

---

## 1. Repositories and Revisions Evaluated

| Repository | Pinned Commit / Revision | Role / Description |
| --- | --- | --- |
| `mstan/snesrecomp` | `c2f2421b78e3444b80de508740d1bd6ccb944632` | Core static recompiler (Rust analyzer + Python C emitter) and SNES hardware runtime. |
| `Yoshifanatic1/ISSD-disassembly` | `d5b0e975c25c266d1a92692a62cb18462807c679` | Full 65816 assembly disassembly of ISSD USA, RAM map, routine macros, and data tables. |
| `EstebanFuentealba/ISSD-web-editor` | `703c2d1e10f20308c0b09ae09358272b0d03a22b` | Reference specifications for player stats, team kits, formations, and attributes. |

---

## 2. License Notes

- **`snesrecomp`**: PolyForm Noncommercial License 1.0.0 for the core project, with separately attributed third-party components. See `deps/snesrecomp/LICENSE` and `deps/snesrecomp/THIRD_PARTY_ATTRIBUTION.md`.
- **`ISSD-disassembly`**: Reverse-engineering source; used purely for symbol address cross-referencing and memory map discovery.
- **ISSD Game Assets**: Strict compliance policy in effect; copyrighted assets and original ROM binary are excluded from Git and kept strictly on the local user machine.

---

## 3. Verified ROM Revision

- **ROM Name:** International Superstar Soccer Deluxe (USA)
- **Internal Title:** `SUPERSTAR SOCCER 2`
- **File Format:** Headerless `.sfc` (2,097,152 bytes / 16 Mbits)
- **MD5 Hash:** `345ddedcd63412b9373dabb67c11fc05` (Verified Clean USA No-Intro dump)
- **Validation Script:** `tools/verify_rom.py`

---

## 4. ROM Mapping Details

- **Cartridge Type:** LoROM / FastROM (CartType 1)
- **ROM Size:** 2 MB (64 banks: `$80` to `$BF`, LoROM index `$00` to `$3F`)
- **Native Vectors:**
  - Reset: `$80:FF90` -> `$80:8000`
  - NMI: `$80:FF9B` -> `$80:80E0`
  - IRQ/BRK: `$80:FF97` -> `$80:81A4`
- **WRAM Layout:**
  - Direct Page / Stack: `$00:0000` - `$00:1FFF` (mirrored in `$7E:0000` - `$7E:1FFF`)
  - Extended WRAM: `$7E:2000` - `$7F:FFFF` (Palettes at `$7E:2C00`, DMA tables at `$7E:3200`, etc.)

---

## 5. SNESRecomp Generation Status

- **Configuration:** 64 bank config files generated under `recomp/config/` (`bank00.cfg` to `bank3f.cfg`).
- **Discovered Functions:** 2,999 analysis roots.
- **Analyzed CFG:** 4,896 exact variants, 8,016 control-flow edges.
- **Emitted Translation Units:** 74 C source files + global dispatch table `dispatch_v2.c` in `recomp/generated/`.

---

## 6. Statically Resolved Code Coverage

- **AOT-Eligible Exact Variants:** 2,053 functions (~42% fully resolved to native machine code without any fallback).
- **LLE Fallback Tier:** 2,843 functions (routed through the safe SNESRecomp interpreter fallback tier with full M/X tracking and register synchronization).
- **Static Coverage Expansion:** Can be increased in subsequent phases by annotating complex jump tables with `indirect_dispatch` directives.

---

## 7. Fallback / Interpreter Locations

- Indirect jump dispatch tables in menu and player AI routines (banks `$80`, `$83`, `$84`, `$85`, `$86`, `$8A`, `$8B`, `$A4`).
- Unresolved jump table sites currently execute seamlessly through the interpreter fallback tier (`tier_down_stubs` enabled in `bank00.cfg`).

---

## 8. Known Indirect-Jump / Dispatch Problems

- 285 indirect jump sites identified in the disassembly.
- The `tier_down_stubs` mechanism in SNESRecomp handles un-annotated indirect dispatches by falling back to interpreter execution rather than aborting.
- Next step: Map out the primary jump tables in `tools/generate_issd_configs.py` using `indirect_dispatch <site> <count> idx:<reg> tables:<addrs>`.

---

## 9. Build Status

- **Compiler:** Clang 22.1.8 / MSVC x64 + Ninja + CMake 3.20+
- **Host Binary:** `build/ISSDNative.exe` (7.99 MB native Windows x86-64 executable)
- **Compilation Time:** ~35 seconds parallel build.
- **Link Status:** Clean link with SDL2, winmm, ws2_32.

---

## 10. Runtime & Boot Status

- **Initialization:** `SnesInit()` and `snes_loadRom()` execute successfully.
- **Reset Vector:** Executes `CODE_808000` reset routine, clears Direct Page and WRAM, sets up PPU registers and DMA channels.
- **Execution Loop:** Headless and windowed execution loops run at stable 60 Hz. The ROM-backed automated match has completed 12,000 consecutive frames with a balanced `$01AF` idle stack and every NMI completed; dedicated tests cover interpreter-owned interrupt tails and non-local returns through generated tail wrappers.

---

## 11. Graphics Status

- **PPU Emulation:** Mode 1 / Mode 2 BG layers and OAM sprite evaluation functional via `g_ppu`.
- **Framebuffer Output:** 256x224 32-bit ARGB framebuffer rasterized per scanline and streamed to SDL texture.
- **Screenshot Exporter:** Automated BMP capture (`--screenshot <file>`) working.

---

## 12. Audio Status

- **APU / S-DSP Emulation:** S-DSP stereo audio generated at 32,000 Hz / 44,100 Hz output.
- **Thread Synchronization:** `RtlApuLock` / `RtlApuUnlock` thread-safe mutex implemented in host.
- **SDL Audio:** Device stream callback connected to `RtlRenderAudio`.

---

## 13. Input Status

- **Controller 1:** Keyboard mapped (Arrow keys / WASD, Z/J for B, X/K for A, A/U for Y, I for X, Q for L, E for R, Enter for Start, Space for Select).
- **Controller 2:** 12-bit joypad mask structure ready for local 2-player support.

---

## 14. Major Blockers

- **None for Phase 1 bring-up baseline.** The executable compiles, boots, and executes the recompiled code with the SNES hardware runtime.

---

## 15. Next Concrete Engineering Tasks

1. **Jump Table Annotation:** Add `indirect_dispatch` rules for the main game loop and menu dispatches to increase AOT coverage from 42% towards 90%+.
2. **Interactive Menu & Match Testing:** Test title screen animations, menu navigation, team selection, match loading, kickoff, and player control in windowed mode.
3. **Bridge Expansion:** Expand `issd_bridge.c` to expose high-level match state (ball coordinates, player coordinates, match clock, fouls, score).
4. **Differential Validation Framework:** Set up automated deterministic input tests comparing native execution against reference emulator states.
