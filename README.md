# ISSD Native

A native recompilation and modernization of **International Superstar Soccer Deluxe (USA)** for the Super Nintendo. The original 65816 code is translated into C, with native replacements for asset decompression, memory streaming, and audio command handling. The runner retains SNES hardware models for video, audio, and other peripherals.

The goal is to preserve the original gameplay while adding modern controls, presentation options, saves, and editable mod packs. This is a **beta project working toward 1.0**; recompilation coverage does not establish complete gameplay or hardware fidelity.

The current release is **[v0.3.0-beta.1](https://github.com/sergiomanzur/issd-native/releases/tag/v0.3.0-beta.1)**, a GitHub prerelease for Windows and Android. It adds configurable per-player controls and touch layouts, exhibition rematches/drills and favorites, verified original-game bug fixes, and improved graphics/readability including widened penalty scenery. Four-player local play, campaign recovery, cartridge password interoperability and optional AI tweaks remain available. See [CHANGELOG.md](CHANGELOG.md) and [the release notes](docs/releases/v0.3.0-beta.1.md).

The [supported-feature inventory](docs/SUPPORTED_FEATURES.md) records actual behavior and inactive settings; [acceptance checks](docs/ACCEPTANCE_TESTS.md) separate automated evidence from pending complete-campaign and device testing. Original formation/substitution menus have [input-driven acceptance checks](docs/TEAM_CHANGES_ACCEPTANCE.md) for first-team commits, request cancellation and advancing save/reload with AI off/on. The project remains a beta; netplay is a [future design](docs/NETPLAY_DESIGN.md).

## Current platforms

| Platform | Current state |
| --- | --- |
| Windows x86-64 | Release packages and a native CMake build. Current multiplayer, campaign saves, and password bridge built and tested here. |
| Android 8.0+ | APK releases, ROM/mod import, touch controls, and overlay navigation. Build targets are `arm64-v8a` and `x86_64`. The new multiplayer changes still need an Android device playtest. |
| Linux x86-64 | CMake preset using system SDL2. Current source built under Ubuntu 24.04, with a headless save/load smoke check. |
| SteamOS / Steam Deck | Build target using the Steam Linux Runtime 3.0 “sniper” SDK, a launcher script, and Deck-specific first-run settings. |
| macOS | No validated port or release package at present. |

Current Windows, Linux, and Android builds succeed; gameplay and controller regression validation was performed on Windows. The SteamOS-specific SDK package was not rebuilt in this verification. Android releases currently use the debug signing configuration for sideload testing.

## What is implemented

- **Original game modes:** Open Game/exhibition, International Cup, World Series, scenarios, training, and penalty shootouts. Full end-to-end certification of every mode is still pending.
- **Native runtime paths:** Konami five-mode LZSS/RLE decompression, VRAM/WRAM streaming, static indirect dispatch, and audio command fast paths. SPC700 execution and S-DSP sound generation remain in the runner.
- **Local multiplayer:** Up to four independent SDL2 gamepads, SNES multitap support, stable player slots, connection notifications, and automatic pause when a gamepad disconnects during live play.
- **Controls:** Saved P1–P4 Classic/FIFA/PES/custom gamepad profiles, in-game remapping and separate stick/trigger deadzones; remappable P1 keyboard inputs; individual Android touch positions and sizes. See [controls](docs/CONTROLS.md).
- **Overlay menu:** Presentation, audio, controls, saves, and mod selection, accessible from keyboard, gamepad, and Android touch.
- **Exhibition shortcuts:** Instant rematch, independent drill checkpoints, a persistent favorite setup, and Original/Classic/Casual/custom rules using existing cartridge options. See [match shortcuts](docs/MATCH_SHORTCUTS.md).
- **Campaign saves:** Explicit Continue, safe campaign checkpoints, two backup generations, and compatibility checks against applied gameplay data. See [the campaign saves guide](docs/CAMPAIGN_SAVES.md).
- **Password bridge:** Native entry/export for all six original Cup/World Series password formats, original checksum validation and restoration, and autosave after import. Requires unmodified retail gameplay. See [the password guide](docs/PASSWORD_BRIDGE.md).
- **Save states:** Quicksave/quickload and eight numbered slots with integrity and gameplay compatibility checks. New snapshots preserve native CPU/timing and multitap state. Older v4–v8 raw snapshots require a compatibility warning and explicit confirmation.
- **Presentation:** Window-size presets, actual resolution reporting, independent integer scaling, 4:3/square-pixel/Authentic 320 and wider views, nearest/linear/Sharp/CRT filters, sharp scalable overlays, Original/Sharp/Enhanced presets with a sample preview/reset, and optional ball/player/radar readability with label size and radar placement/opacity controls. See [graphics settings](docs/GRAPHICS_SETTINGS.md).
- **Modding:** Ordered roster/formation/kit/stadium packs, striped and away kits, custom flags, team plates, squad photos, and BMP background tile replacements. ISSD Mod Studio provides a visual editor.
- **Gameplay tweaks:** Stackable, persistent goalkeeper shot tracking and formation/substitution-aware player positioning, controlled from the new **Gameplay Tweaks** overlay page. Both default off and apply live. See [the gameplay tweaks guide](docs/GAMEPLAY_TWEAKS.md).
- **Original bug fixes:** One persistent master toggle for six verified corrections, with original behavior when Off and save compatibility checks. Reported glitches without a verified cause remain unresolved. See [the bug-fix guide](docs/ORIGINAL_BUG_FIXES.md).
- **Developer tools:** Headless simulation, screenshots, RAM/state dumps, frame-based input scripts, ROM verification, and regression tests.

Gameplay advances at 60 Hz. Higher presentation FPS settings do not add interpolated gameplay frames. Resolution scaling enlarges the rendered image; it does not redraw the original assets in HD. Intermediate scaling applies to CRT, Sharp and HD tile composition; actual output dimensions are reported separately.

Widescreen extends the view using game metatiles and supplemental sprites. Verified culled player states continue their authored ROM animations without prior cycle learning; learned locomotion provides a fallback. Geometry and graphics stay paired without changing simulation. Wider presets remain experimental: unsupported offscreen actions can still hold. The goal-facing penalty view also extends its grass/crowd scenery for 16:10, 16:9 and 21:9 while retaining the original goal, players and HUD. Use 4:3 for the baseline presentation. Audio fidelity and broader end-to-end gameplay validation remain targets.

## Getting started

The game ROM is not included. Provide your own dumped cartridge image matching this supported revision:

| Property | Expected value |
| --- | --- |
| Game | International Superstar Soccer Deluxe (USA) |
| Internal title | `SUPERSTAR SOCCER 2` |
| Format | Headerless `.sfc`, 2,097,152 bytes |
| MD5 | `345ddedcd63412b9373dabb67c11fc05` |
| SHA-256 | `d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9` |

Verify a dump with:

```powershell
python tools/verify_rom.py "path/to/International Superstar Soccer Deluxe (USA).sfc"
```

On Windows, extract the release package and run `ISSDNative.exe`, or launch your source build:

```powershell
.\build\ISSDNative.exe --rom "path/to/International Superstar Soccer Deluxe (USA).sfc" --mods-dir mods
```

Keep `aot_boot_deny.txt` beside the executable; it supplies the runtime's required boot overrides. Windows also needs the matching `SDL2.dll` beside the executable or on `PATH`. Include `mods/` to use the supplied packs.

If no valid ROM is configured, the desktop launcher opens a file picker. Linux requires `zenity` or `kdialog` for that picker; `--rom` works without it. Android uses the system picker to import the ROM and mods.

Settings are saved in `issd_native.cfg`. By default this is under `%APPDATA%/ISSDNative` on Windows, `$XDG_CONFIG_HOME/ISSDNative` or `~/.config/ISSDNative` on Linux, and app storage on Android. An existing `issd_native.cfg` or `issd_config.json` in the working directory takes precedence. Use `--config <path>` to select a specific file. A minimal explicit configuration is:

```ini
rom_path=path/to/International Superstar Soccer Deluxe (USA).sfc
mods_dir=mods
```

Saves use `%LOCALAPPDATA%/ISSDNative/saves` on Windows, `$XDG_DATA_HOME/issd-native/saves` or `~/.local/share/issd-native/saves` on Linux, and app-internal storage on Android. Use `--save-dir <directory>` for portable saves. Existing working-directory `saves/` files remain available through explicit manual load with a legacy compatibility warning.

## Local multiplayer and controls

Connect up to four SDL2-compatible gamepads before starting or while the game is running. They occupy **P1–P4 in connection order**, with an on-screen notification identifying each slot. Keyboard and Android touch also control P1; four independent gamepad players require four gamepads.

In **Open Game**, choose the cartridge's multiplayer player-count option. Original configurations include **P1+P3 versus P2+P4** and **all four players versus CPU**.

Disconnecting a pad clears its input without moving the remaining players to other slots. A live match pauses in the overlay. Reconnect, then resume when ready; new connections fill the first vacant slot. With several vacancies, slots fill in ascending order. A fifth gamepad is ignored.

Any assigned pad can open the overlay with **Guide/Home** or **Back+Start**, then navigate with the D-pad, A, and B. Held gamepad controls must be released before affecting gameplay after closing the overlay or regaining window focus. See [the multiplayer guide](docs/LOCAL_MULTIPLAYER.md) for protocol, script, and validation details.

Open **Controls / Profiles** in the overlay to edit P1–P4 presets, bindings and
deadzones, or select **P1 Keyboard** and **Touch Layout**. Changes persist and
apply immediately. Overlay navigation stays fixed so gameplay remapping cannot
remove access to settings. Profiles follow player slots, not controller identity.

Default keyboard bindings send these SNES inputs to P1. Remapping an action
replaces its legacy aliases too:

| Keys | Input |
| --- | --- |
| Arrow keys or W/A/S/D | Move |
| Z or J | B |
| X or K | A |
| C or U | Y |
| V or I | X |
| Q / E | L / R |
| Enter | Start |
| Space or Right Shift | Select |

| Shortcut | Action |
| --- | --- |
| Escape / F1 | Toggle overlay |
| F2 | Cycle and save P1 gamepad preset |
| F3 | Cycle aspect ratio |
| F4 | Cycle internal scale when applicable to the filter |
| F5 / F6 | Quicksave / quickload |
| Shift+1–8 / Ctrl+1–8 | Save / load numbered slot |
| F7 | Cycle presentation FPS limit |
| F8 | Cycle nearest, linear, and CRT filtering |
| F11 | Toggle fullscreen |

## Mods and Mod Studio

The `mods/` directory includes Liga MX/Expansión, World Cup 2026-themed, Chivas, legends, and formation example packs. These are editable replacement packs within the original game's data constraints.

Open the overlay's **Mods** page to select packs. Roster packs patch an in-memory copy of the ROM; the original dump is unchanged. Enabled packs form an ordered stack, with later packs taking precedence and overlaps reported. Save and restart to apply roster changes; background tile packs can be toggled live.

Run the visual editor from source with Python 3.10+ and Pillow:

```powershell
python -m pip install pillow
python tools/mod_studio_launch.py
```

See [Mod Studio](tools/mod_studio/README.md) for building its standalone Windows executable and importing cartridge data. See [the modding guide](docs/MODDING.md) for pack schemas, limits, validation, and HD tile capture/replacement.

## Building from source

The generated C sources and SNESRecomp runner are checked into this repository; a normal build does not require regenerating the recompilation. Desktop builds require a C11/C++17 compiler, SDL2 development files, Ninja, and CMake 3.20+ (3.21+ for the supplied presets).

```powershell
git clone https://github.com/sergiomanzur/issd-native.git
cd issd-native
```

### Windows

Use an x64 Visual Studio developer shell or a configured Clang toolchain. Point CMake at your SDL2 development installation:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/path/to/SDL2"
cmake --build build --parallel
```

The result is `build/ISSDNative.exe`. CMake copies `aot_boot_deny.txt`; depending on the SDL2 package layout, you may need to copy its x64 `SDL2.dll` into `build/` yourself.

### Linux

Install a C/C++ toolchain, CMake, Ninja, pkg-config, and SDL2 development files (for example, `build-essential cmake ninja-build pkg-config libsdl2-dev` on Debian/Ubuntu), then:

```bash
cmake --preset linux
cmake --build --preset linux
./build-linux/ISSDNative --rom "/path/to/game.sfc" --mods-dir mods
```

### SteamOS and Android

The [multi-platform build script](build-all.sh) can stage Windows, Linux, SteamOS, and Android artifacts into `dist/<target>/`. On the Windows development host it uses Git Bash, WSL for Linux, and Docker for the SteamOS sniper SDK:

```bash
bash build-all.sh --only steamos
bash build-all.sh --only android
```

The SteamOS package includes [steamos-run.sh](scripts/steamos-run.sh). Android needs JDK 17, the Android SDK (API 34), NDK `26.2.11394342`, CMake `3.22.1`, and Gradle or the Gradle wrapper. Set `ANDROID_SDK_ROOT` to the SDK location. [build-android.sh](scripts/build-android.sh) fetches the pinned SDL2 sources and assembles the release APK; [the app configuration](android/app/build.gradle) defines its ABIs and signing.

After building Windows and Android for the version in `VERSION`, run
`python tools/assemble_release.py` to create beta packages and SHA-256 checksums
under `dist/releases/`. The packager uses an explicit file list and verifies the
Android version before packaging.

## Testing and diagnostics

Run this project's tests explicitly so pytest does not also collect vendored SNESRecomp tests:

```powershell
python -m pytest tests -q
python -m pytest tests/test_local_multiplayer.py -q
```

Tests need Python and pytest, with compiler/SDL2 requirements for native harnesses. Some also require Pillow. ROM-backed checks require your supported ROM and a built executable; see the individual test files for setup.

The latest full project-suite run (2026-10-01) produced **227 passed, 2 failed, 2 skipped, and 7 subtests passed** in 971.41 seconds. Both failures are the existing mod appearance-nibble expectation and shared-kit warnings. Graphics checks cover presets/preview/reset, label/radar controls, real SDL software rendering from 320x240 through 4K, and renderer-reset recovery. Five real-ROM viewports passed at frames 600 and 1200 with identical guest RAM and exact original center pixels. Windows, Linux and Android `arm64-v8a`/`x86_64` builds succeeded; physical display/touch acceptance remains pending. See [graphics settings](docs/GRAPHICS_SETTINGS.md) and [acceptance records](docs/ACCEPTANCE_TESTS.md).

Final graphics verification against the updated Windows executable returned **17 passed in 68.07 seconds**, including a live graphical match replay and paused HD capture geometry.

The offscreen-animation follow-up passed **18 focused checks** against the updated
Windows executable. Verified culled states now use the original ROM animation
tables, including cold starts and terminal holds. The reproducible
[match graphics checks](docs/GRAPHICS_MATCH_ACCEPTANCE.md) track original pause/replay,
period transitions, extra time and shootout entry. Its extended run passed
**3 tests in 213.68 seconds**, comparing 11 scenes at three widths with identical
full game RAM and native center pixels. Remaining gaps are documented.

The penalty-camera follow-up passed **11 focused checks** and the expanded
**3-test retail run in 279.40 seconds** (16 scenes, 53 viewport runs and 1,197
captures), including actual kicks and subsequent turns at 16:10/16:9/21:9.
Windows, Linux and Android builds succeeded.

The two remaining baseline failures are an appearance-nibble expectation (`test_mod_rom_patching`) and shipped-pack kit-sharing warnings (`test_shipped_packs_validate`). The earlier menu harness linking and snapshot replay failures are fixed. Full tournament, shootout, team-change and device acceptance evidence is tracked in [ACCEPTANCE_TESTS.md](docs/ACCEPTANCE_TESTS.md). Physical four-controller matches, Android controls, physical-cartridge password comparison, and gameplay-tweak balance still need playtesting. This is not yet a fully passing 1.0 test baseline.

Useful runtime options:

| Option | Purpose |
| --- | --- |
| `--rom <path>` | Select the ROM; a positional ROM path is also accepted |
| `--config <path>` | Select the configuration file |
| `--save-dir <path>` | Select an isolated or portable save root |
| `--continue` | Explicitly restore the latest compatible campaign checkpoint |
| `--allow-legacy-save` | Confirm raw legacy loading for headless fixtures |
| `--mods-dir <path>` | Select the mods folder |
| `--headless [frames]` | Run without a window; defaults to 120 frames |
| `--frames <count>` | Stop after the requested simulation frame count |
| `--auto-start <frame>` | Start automated menu navigation into a match |
| `--screenshot <file.bmp>` | Save the final rendered frame |
| `--graphics-report <file.json>` | Record actual presentation dimensions, renderer resets and CPU/total rendering times |
| `--dump-state <file>` | Write a diagnostic state dump |
| `--script <file>` | Apply frame-based inputs, optionally addressed to P1–P4 |
| `--import-password-symbols <file>` | Import raw palette-index bytes on the settled cartridge Password screen |
| `--import-password-at <frame>` | Schedule developer password submission (default frame 1) |
| `--export-password-symbols <file>` | Export raw palette-index bytes from the final settled campaign checkpoint |

For example:

```powershell
.\build\ISSDNative.exe --rom "path/to/game.sfc" --headless 1800 --auto-start 30 --screenshot match.bmp
```

## Remaining work for 1.0

1. Establish a fully passing regression baseline, including mod appearance and shipped-pack validation.
2. Playtest two-, three-, and four-player matches on physical controllers, including reconnect/focus changes and Android handhelds.
3. Verify full tournament progression, scenarios, training, penalties, long sessions, and audio against the original game.
4. Resolve widescreen edge behavior and clearly separate faithful presentation from experimental enhancements.
5. Validate remapping/profile ergonomics on physical controllers and touch devices, and finish release packaging across supported platforms.

Rollback netplay, interpolated presentation, expanded asset replacement, and additional gameplay mods remain future work. The Classic/Enhanced labels and settings such as skip intro or fast menus should not be treated as proof that distinct gameplay behavior is implemented.

## Technical documentation and licenses

- [System architecture](docs/SYSTEM_ARCHITECTURE.md), [routine map](docs/ROUTINE_MAP.md), and [reverse engineering](docs/REVERSE_ENGINEERING.md)
- [Rendering](docs/RENDERING_AND_GFX.md), [audio](docs/AUDIO_SYSTEM.md), and [RAM map](docs/RAM_MAP.md)
- [Known differences](docs/KNOWN_DIFFERENCES.md) and [enhancement design notes](docs/ENHANCEMENTS.md) — some entries describe planned behavior; use this README and the current source for implementation status
- [Changelog](CHANGELOG.md), [modding](docs/MODDING.md), and [local multiplayer](docs/LOCAL_MULTIPLAYER.md)

Component licenses:

- [SNESRecomp](deps/snesrecomp/LICENSE): PolyForm Noncommercial License 1.0.0.
- [ISSD disassembly reference](deps/ISSD-disassembly/LICENSE): GPL-3.0.
- [ISSD web editor reference](deps/ISSD-web-editor/LICENSE.md): MIT.

International Superstar Soccer Deluxe and associated marks belong to their respective rights holders. This is an independent reverse-engineering, preservation, and modernization project. Release packages do not include the game ROM.
