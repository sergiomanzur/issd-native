# ISSD Native

ISSD Native is an AI-driven native recompilation project for **International Superstar Soccer Deluxe** on SNES, targeting modern Windows PCs first.

The goal is to modernize the game while preserving the original gameplay as the compatibility baseline. The current architecture uses SNESRecomp-style static recompilation of the original 65816 program into native C/C++ code, with a SNES-compatible runtime for hardware behavior that still depends on the PPU, APU, DMA, cartridge mapping, and interpreter fallback paths.

This is not an emulator wrapper. The long-term direction is a native PC application with Classic Mode for faithful behavior and Enhanced Mode for modern presentation, improved timing, better controls, audio improvements, modding, widescreen, HD assets, replays, and eventually online play.

## Current Status

- Windows x86-64 is the primary target.
- The game boots, reaches menus, starts matches, and gameplay is currently stable in the validated local build.
- The black-screen gameplay freeze found during live match testing has been fixed in the runtime bridge by protecting interpreter/AOT stack ownership and by keeping a sensitive ISSD routine in exact interpreter execution.
- Audio, widescreen, and generated-code reproducibility are still active development areas.

See [docs/BRINGUP_STATUS.md](docs/BRINGUP_STATUS.md), [docs/KNOWN_DIFFERENCES.md](docs/KNOWN_DIFFERENCES.md), and [docs/DECISIONS.md](docs/DECISIONS.md) for engineering status and known limitations.

## Legal And Copyright Notice

This repository does **not** include the original International Superstar Soccer Deluxe ROM, cartridge dump, extracted commercial assets, firmware blobs, save files, or copyrighted game media.

To build or run against the original game data, you must provide your own legally obtained cartridge dump. Do not upload ROMs, BIOS/firmware files, extracted commercial assets, or game media to this repository.

International Superstar Soccer Deluxe, its code, graphics, audio, names, marks, and other original game content belong to their respective rights holders. This project is an independent preservation, interoperability, research, and modernization effort and is not affiliated with, endorsed by, sponsored by, or approved by Konami or any other rights holder.

The code in this repository is subject to the licenses of its components. In particular:

- `deps/snesrecomp` includes SNESRecomp code licensed under the PolyForm Noncommercial License 1.0.0, plus separately attributed third-party components documented in `deps/snesrecomp/THIRD_PARTY_ATTRIBUTION.md`.
- `deps/ISSD-disassembly` is GPL-3.0 licensed reverse-engineering reference material.
- `deps/ISSD-web-editor` is MIT licensed reference tooling.

Check each dependency's license file before redistribution, packaging, or commercial use. This README is not legal advice.

## Required Cartridge Dump

The supported game revision is:

- Game: `International Superstar Soccer Deluxe (USA)`
- Internal title: `SUPERSTAR SOCCER 2`
- Cartridge mapping: LoROM / FastROM
- ROM size: 2 MiB / 16 Mbit / 2,097,152 bytes
- Format: clean headerless `.sfc`
- MD5: `345ddedcd63412b9373dabb67c11fc05`
- SHA-1: `67ee7452d5b6e4e5e406fbaec72828b812fcf452`
- SHA-256: `cbe787a7ba22b07f8ee7be0a6bf95fa1e6fbe6c466487ffdf8fbc5f778d97151`

Verify a local dump with:

```powershell
python tools\verify_rom.py "C:\path\to\International Superstar Soccer Deluxe (USA).sfc"
```

## Build Requirements

Recommended Windows toolchain:

- Visual Studio 2022 with MSVC x64 tools
- CMake 3.20 or newer
- Ninja
- Python 3
- SDL2 development package
- Git

The current root CMake file defaults `CMAKE_PREFIX_PATH` to the local SDL2 path used during development:

```cmake
C:/Users/sergi/scoop/apps/sdl2/current
```

If SDL2 is installed somewhere else, pass your SDL2 prefix when configuring.

## Building A New Build

From a Visual Studio x64 developer shell or a PowerShell session with the compiler available:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\path\to\sdl2"
cmake --build build --config Release
```

The executable is produced at:

```text
build\ISSDNative.exe
```

Run it from the build directory or next to `SDL2.dll`.

Current source-only rebuilds compile successfully, but full generated-code reproducibility is still being stabilized. The validated local gameplay build used for the latest black-screen fix is preserved in the local build output; future work should close the remaining generated-code boot/runtime gap before treating fresh rebuilds as release candidates.

## Running

Current development builds are intended to run locally with your own verified ROM. Runtime ROM discovery is still evolving, so use the launcher or command-line behavior implemented in the current executable and keep the ROM outside Git.

Useful developer validation:

```powershell
python tests\test_gameplay_stability.py build\ISSDNative.exe
```

That test drives the game into a match and checks that gameplay remains active, the NMI path completes, and the idle stack stays balanced.

## Repository Layout

- `ISSDNative/` - hand-written native host integration, ISSD bridge code, config, save/mod/menu scaffolding, and widescreen hooks.
- `recomp/config/` - SNESRecomp configuration for ISSD banks and generated symbol declarations.
- `recomp/generated/` - generated C from the recompilation pipeline.
- `deps/snesrecomp/` - vendored SNESRecomp runtime, tools, tests, and local changes.
- `deps/ISSD-disassembly/` - ISSD disassembly reference material.
- `deps/ISSD-web-editor/` - ISSD data-format reference tooling.
- `docs/` - bring-up reports, RAM/routine maps, known differences, decisions, modding notes, and enhancement plans.
- `tools/` - local analysis and ROM verification utilities.
- `tests/` - runtime, audio, widescreen, frame pacing, and gameplay stability tests.

## Development Rules

- Do not commit ROMs, cartridge dumps, firmware files, extracted commercial assets, screenshots, local saves, generated logs, or build outputs.
- Keep hand-written source and generated recompilation output clearly separated.
- Prefer configuration, recompiler fixes, hooks, or documented regeneration steps over manual edits to generated source.
- Preserve Classic Mode behavior as the compatibility baseline before adding Enhanced Mode changes.
- Document important reverse-engineering discoveries in `docs/`.

## No Warranty

This software is provided as-is, without warranty of any kind. Use it only with game data and third-party components you are legally allowed to use.
