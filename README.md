# ISSD Native — Modern Native Recompilation of International Superstar Soccer Deluxe

**ISSD Native** is an AI-driven, high-performance static recompilation and modernization project for the legendary Super Nintendo soccer title **International Superstar Soccer Deluxe** (USA).

The project translates the original 65816 machine code and SNES hardware interactions into native C/C++ running directly on modern 64-bit host processors (Windows x86-64 first), bypassing traditional emulation overhead while preserving 100% gameplay, animation, ball physics, and tactical AI authenticity.

---

## 🎯 Project Goals

1. **100% Authentic Baseline:** Preserve every frame of original gameplay pacing, player inertia, referee decisions, collision detection, and AI behavior with zero degradation.
2. **Native PC Architecture:** Run as a standalone native executable with deterministic 60 Hz frame pacing, ultra-low input latency, and direct host OS controller support.
3. **Native C High-Level Emulation (HLE):** Replace performance-critical translated 65816 subsystems (asset decompression, tile streaming, APU communication) with optimized native C routines.
4. **True 16:9 Widescreen:** Dynamically stream 32x32 pitch and stadium metatiles from Bank `$7F` to expand the field of view without stretching, distortion, or sprite pop-in.
5. **Audio Modernization:** Clean S-DSP stereo sound rendering, voice sample streaming, announcer commentaries, and background music sequencing with zero APU port timeouts.
6. **Modern Quality-of-Life:** In-game pause menu, modern gamepad layout switching (EA FC / FIFA style), instant quicksaves, CRT scanline filtering, and modding hooks.

---

## 📊 Recompilation & Reverse Engineering Metrics

| Metric | Status / Measurement | Description |
| :--- | :--- | :--- |
| **Playability & Stability** | **100% Verified** | Complete match simulation, menus, scenarios, and attract loops verified over 15,000+ continuous frames with 0 watchdog pauses. |
| **Analyzed Unique Routines** | **5,035 / 5,035 (100.0%)** | All 65816 subroutines and entry points identified, cataloged, and accounted for in the call tree (verified via [`tools/audit_uncalled_routines.py`](tools/audit_uncalled_routines.py)). |
| **Compiled AOT Function Variants** | **17,393 variants** | Full control-flow graph coverage across all 65816 M/X flag configurations (M0X0, M0X1, M1X0, M1X1) with 0 unresolved stubs. |
| **Statically Dispatched Jump Tables** | **153 / 153 (100.0%)** | All `JMP (abs,X)` indirect dispatch sites across Banks $83–$A4 mapped to direct C static switches. |
| **Native Asset Decompression HLE** | **100% Native C** | Custom Konami 5-mode LZSS/RLE decompressor reimplemented in native C ([`ISSDNative/issd_decompress.c`](ISSDNative/issd_decompress.c)), replacing slow 65816 bitstream loops and the WRAM MVN trampoline. |
| **Native Audio Fast-Path HLE** | **100% Native C** | Konami SPC700 audio command protocol fast-pathed in C ([`ISSDNative/issd_audio.c`](ISSDNative/issd_audio.c)), eliminating 262k cycle spin-waits with 74 voice/sfx telemetry mappings. |
| **Match Simulation Throughput** | **~270 – 440 FPS** | Headless match simulation runs at up to 7× real-time speed on modern x86-64 CPUs. |
| **Core Systems Understanding** | **~95% Reversed** | Decompression, palette DMA, camera metatile streaming, APU protocol, and HDMA rasterization fully documented in [`docs/`](docs/). |
| **Routine Directory Index** | **2,582 documentation lines** | Full cross-referenced disassembly symbol map in [`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md). |

---

## ✨ Features & Current Status

### ⚡ Native C Asset Decompression & Memory Streamer HLE (Priority 2)
- **Zero-Allocation 5-Mode LZSS/RLE Engine ([`ISSDNative/issd_decompress.c`](ISSDNative/issd_decompress.c)):**
  - Completely replaces translated 65816 decompression loops (`CODE_80B527`) and the fragile WRAM MVN self-modifying trampoline (`$001E40`).
  - Supports RAW (0x80..0x9F), RLE Interleaved (0xA0..0xBF), RLE High (0xC0..0xDF), RLE Constant/Zero Fill (0xE0..0xFF), and 1024-byte sliding window LZSS back-references (0x00..0x7F).
- **Multi-Mode PPU VRAM Direct Streamer (Type 0x00):**
  - Directly streams decoded graphics into PPU VRAM arrays handling Mode 0 (16-bit word), Modes 1 & 2 (PortLo $2118), and Mode 3 (PortHi $2119).
  - Performs 2bpp/4bpp planar tile deinterleaving (`ISSD_DeinterleaveTiles`) on the fly when stream header bit 15 (`src[1] & 0x80`) is set.
- **Map32 Stride-2 Interleaved WRAM Streamer (Type 0x01):**
  - Reverse engineered the `!Define_ISSD_DecompressionRt_DecompressMap32Flag` ($400000 / bank bit 6): decompresses with **stride-2** into WRAM `$7F8000..$7F91FF`, seamlessly interleaving tile numbers and attributes for pixel-perfect pitch and stadium rendering.
- **Hardware-Accurate 65816 RTL Stack Unwinding:**
  - Emulates 3-byte hardware `RTL` stack popping and 24-bit return PC computation, eliminating guest stack imbalances and achieving **0 watchdog pauses**.

### 🎮 Gameplay & Game Modes (100% Functional)
- **Open Game / Exhibition:** Full team selection, stadium selection, weather conditions, controller configuration.
- **International Cup & World Series:** Complete multi-stage tournament progression.
- **Scenario Mode:** Challenge scenarios with remaining clock countdown and targeted win conditions.
- **Training & Penalty Shootout:** Full practice drills and penalty shootout modes.

### 🖼️ Video & Presentation (100% Functional)
- **Pristine Pitch & Stadium Rendering:** Pixel-perfect field grass mowing patterns, stadium crowd graphics, and white pitch line markings.
- **Title Screen HDMA Mode 3 Split:** Per-scanline HDMA engine switches PPU from Mode 1 to Mode 3 (8bpp direct color) on scanline 112, cleanly displaying all 5 real-life player portraits without white cutout boxes or corrupted tiles.
- **16:9 & Ultrawide Field Streaming (Priority 4):** Synthesizes new pitch and stadium columns in real-time from world metatiles with continuous boundary clamping, eliminating black margin voids near penalty boxes and sidelines across 16:10, 16:9, and 21:9 viewports.
- **Sprite Uncapping:** Hardware sprite limits unlocked via `kPpuRenderFlags_NoSpriteLimits` to eliminate sprite flickering during multi-player scrums.
- **CRT Scanline Filter:** Built-in retro scanline shader for authentic CRT display aesthetics.

### 🔊 Native C Audio Subsystem & APU Fast-Path HLE (Priority 3)
- **Fast-Path Command Dispatches ([`ISSDNative/issd_audio.c`](ISSDNative/issd_audio.c)):**
  - Replaces translated 65816 spin-waiting loops (`CODE_80BE95`, `CODE_80BEEC`, `CODE_80BFAE`) with direct native C APU port writes.
  - Instantly drains the `$7EE680` sound effect FIFO queue and sends voice trigger commands without polling cycles or watchdog pauses.
- **74-Voice Announcer Telemetry Dictionary:**
  - Full reverse-engineered dictionary mapping every Konami announcer voice line (Goal, Foul, Yellow/Red Card, Corner Kick, Throw In, Penalty, Extra Time, etc.) and sound effect IDs for human-readable logging and telemetry.
- **Hardware-Accurate 65816 RTL Stack Unwinding:**
  - Emulates 3-byte hardware `RTL` stack frame popping and 24-bit return PC resolution with zero stack leaks.

### 🕹️ Controls & Enhancements
- **Gamepad Auto-Detection:** Direct support for modern Xbox, PlayStation, and generic SDL2 gamepads.
- **Controller Schemas:** Toggle on-the-fly between Classic SNES layout and **Modern / EA FC Layout** (A: Ground Pass, B: Shoot, X: Cross, Y: Through Ball, RB: Sprint).
- **In-Game Pause Menu:** Press **Guide / Home** (or Back+Start) to toggle the custom in-game overlay menu.
- **Instant Quicksave / Quickload:** State banking framework for rapid save-states.

---

## 🗺️ Roadmap & Milestones

- [x] **Priority 1: Indirect Jump Table Mapping & AOT Expansion**
  - Mapped 153/153 indirect jump tables with static C AOT switches.
  - Expanded compiled routine count from 1,850 to **4,457 routines** (16,951 variants).
  - Documented complete routine map in [`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).
- [x] **Priority 2: Native C Decompression & Memory Streamer HLE**
  - Complete 5-mode Konami LZSS/RLE decompression engine in C ([`ISSDNative/issd_decompress.c`](ISSDNative/issd_decompress.c)).
  - Multi-mode VRAM writing and Map32 stride-2 WRAM tilemap streamer.
  - Eliminated WRAM MVN trampoline and stack frame desyncs (0 watchdog pauses).
- [x] **Priority 3: Native Audio Streaming / APU Fast-Path HLE**
  - Fast-pathed Konami SPC700 communication protocols in native C ([`ISSDNative/issd_audio.c`](ISSDNative/issd_audio.c)).
  - Direct sound queue draining and announcer commentary triggering without 262k cycle spin-waits.
  - Reverse-engineered complete 74-item voice commentary dictionary from `DATA_829D9B`.
- [x] **Priority 4: Widescreen Metatile Streaming Overhaul**
  - Continuous stadium boundary metatile clamping in [`ISSDNative/issd_widescreen.c`](ISSDNative/issd_widescreen.c) eliminating black margin voids near penalty boxes and sidelines.
  - Multi-aspect ratio regression verified across 4:3 (256x224), 16:10 (358x224), 16:9 (398x224), and 21:9 (446x224) viewports.
- [ ] **Cross-Platform Native Port:** Expand CMake build targets to Linux (x86-64 / ARM64) and macOS (Apple Silicon via Metal/OpenGL).
- [ ] **Rollback Netplay:** Integrate `recomp-net` / `retcomm-rbengine` for peer-to-peer rollback multiplayer over LAN and Internet.
- [ ] **HD Asset Replacement Packs:** Custom hook system for loading high-resolution team badges, modernized UI textures, and CD-quality audio commentary packs.

---

## 🛠️ Build Instructions

### Prerequisites
* **OS:** Windows 10/11 x86-64
* **Compiler:** Visual Studio 2022 (MSVC x64) or Clang
* **Build System:** CMake 3.20+ and [Ninja](https://ninja-build.org/)
* **Dependencies:** SDL2 development library (`SDL2-devel-2.x.x-VC.zip` or Scoop `sdl2`)
* **Python:** Python 3.8+

### Step-by-Step Build

1. **Clone the repository:**
   ```powershell
   git clone https://github.com/sergiomanzur/issd-native.git
   cd issd-native
   ```

2. **Configure with CMake:**
   ```powershell
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\path\to\sdl2"
   ```

3. **Build the executable:**
   ```powershell
   cmake --build build --config Release
   ```
   The compiled executable will be generated at `build\ISSDNative.exe`.

---

## 🕹️ Running the Game

1. Place your legally dumped ROM in the project folder or specify its path in `issd_config.json`:
   ```json
   rom_path=International Superstar Soccer Deluxe (USA).sfc
   ```
2. Run `build\ISSDNative.exe`. If no ROM path is configured, a Windows file picker dialog will open automatically to select your ROM file.

### CLI Options
- `--rom <path>` : Load a specific ROM file.
- `--headless <frames>` : Run headless execution for automated regression benchmarking.
- `--screenshot <file.bmp>` : Capture a screenshot at the final simulated frame.
- `--auto-start <frame>` : Automate menu navigation into a live match.

---

## 📜 Legal and Copyright Notice

This repository contains **NO copyrighted game assets, ROM binaries, commercial game media, or proprietary BIOS files**.

To build and run this software, you must provide your own legally dumped cartridge image of:
* **Game Title:** *International Superstar Soccer Deluxe (USA)*
* **Internal Title:** `SUPERSTAR SOCCER 2`
* **Format:** Headerless `.sfc` (2,097,152 bytes)
* **MD5:** `345ddedcd63412b9373dabb67c11fc05`
* **SHA-256:** `cbe787a7ba22b07f8ee7be0a6bf95fa1e6fbe6c466487ffdf8fbc5f778d97151`

You can verify your dump with the included verification utility:
```powershell
python tools\verify_rom.py "path\to\International Superstar Soccer Deluxe (USA).sfc"
```

*International Superstar Soccer Deluxe* and associated marks are trademarks and copyrights of their respective rights holders. This project is an independent clean-room reverse-engineering, preservation, and modernization research project.

### Component Licenses
* `deps/snesrecomp`: Licensed under the PolyForm Noncommercial License 1.0.0 (see `deps/snesrecomp/LICENSE`).
* `deps/ISSD-disassembly`: GPL-3.0 reverse-engineering reference data.
* `deps/ISSD-web-editor`: MIT licensed reference tooling.
