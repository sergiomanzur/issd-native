# Gameplay, Broadcast Launcher, and AppImage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Deliver optional gameplay improvements, roster ratings, an original portable launcher, and Deck packaging.

**Architecture:** Small native C policy modules attach to verified original cartridge execution points, with equivalent interpreter hooks. A shared SDL launcher uses the current configuration and platform import APIs. AppDir packaging reuses the SteamOS build and preserves writable user state.

**Tech Stack:** C11, SDL2, CMake/Ninja, Python/pytest, POSIX shell, AppImage tooling.

**Spec:** ../specs/2026-10-21-gameplay-launcher-appimage-design.md

## Global Constraints

- Original gameplay remains the default.
- Windows, Linux, SteamOS/Steam Deck, and Android are the supported-platform scope.
- Preserve the original cartridge duration enum; override values: Original, 1, 2, 3, 5, 7, 10, 15, 20 minutes.
- At most ten seconds of simulation time for final-attack protection.
- Retain existing overlay, saves, mods and control profiles; no online-play import.
- No ROM distribution or deletion/replacement of user mods.

## Review Focus

- Both shooters and legitimate keeper actions must work after penalty correction.
- Replay/load must reproduce final-attack extension and retain compatibility isolation.
- Campaigns must use duration override without affecting challenges or ongoing matches.
- Launcher must handle cancelled imports, hotplug, touch, and held-button handoff.
- AppRun must preserve user mods and resolve relative CLI paths before changing cwd.

### Task 1: Reproduced penalty fix and deterministic gameplay policies

**Files:** Create `ISSDNative/issd_match_options.{c,h}`, `tests/test_match_options.{c,py}`; modify `main.c`, `issd_config.{c,h}`, `issd_menu.c`, `cmake/issd_sources.cmake`.

**Interfaces:** `bool issd_match_option_hook(CpuState *, uint32_t pc)` in main uses pure RAM/register policies from match_options. `int issd_duration_minutes(int index)` preserves the old enum; config gains separate `match_minutes`, `gameplay_short_goal_kicks`, `gameplay_final_attack`, `team_strength`.

- [ ] Trace actual penalty input, original kick and timer code; record root cause and exact hook addresses.
- [ ] Write failing opcode/policy tests for both penalty owners, off/on, short pass vs ordinary kicks, bounded whistle behavior, duration initialization and unrelated modes.
- [ ] Run `python -m pytest tests/test_match_options.py -q` and observe missing-feature failures.
- [ ] Implement narrowly scoped policies and native/interpreter hooks; include all gameplay settings in save/password compatibility.
- [ ] Repeat new tests plus config, bugfix context, match shortcuts, gameplay integration and snapshot tests; build Windows.

### Task 2: Applied-roster strength summaries

**Files:** Create `ISSDNative/issd_strength.{c,h}`, `tests/test_strength.{c,py}`; modify `issd_menu.c`, `main.c`, shared sources.

**Interfaces:** `bool issd_strength_calculate(const uint8_t *rom, size_t size, int team, IssdTeamStrength *out)` returns attack/midfield/defence/overall/stars using decoded applied cartridge data. Overlay consumes the existing selected-team screen gate.

- [ ] Write failing tests for quantized stock, replacement and added rosters, goalkeeper treatment, malformed pointers and all-star resolution.
- [ ] Implement ratings from existing roster layouts, documenting formula and thresholds; render only on verified team selection with toggle.
- [ ] Run `python -m pytest tests/test_strength.py -q` plus relevant existing team identity/mod tests.

### Task 3: Original cross-platform broadcast launcher

**Files:** Create `ISSDNative/issd_launcher.{c,h}`, launcher tests; modify `main.c`, shared sources and Android integration where required; add attribution.

**Interfaces:** `int issd_launcher_run(IssdConfig *, char *rom_path, size_t size)` returns play/quit/error and consumes existing platform pickers. `bool issd_launcher_rom_ok(...)` validates supported retail image. Code-native layout uses current bitmap font.

- [ ] Write failing ROM validation/navigation tests and automation entry points for screenshot/input acceptance.
- [ ] Implement Play/ROM/Presentation/Controls/Gameplay/Mods/Quit, existing config persistence, keyboard/mouse/gamepad/touch navigation, hotplug and release guard. Add in-app directory browser for Deck and Android SAF import polling.
- [ ] Use navy/cream/green/amber broadcast panels, responsive safe viewport, no fork artwork or online dependencies.
- [ ] Integrate before ROM load on interactive startup; preserve `--no-launcher`, headless, CLI and overlay behavior. Avoid Android double-window ownership.
- [ ] Build Windows/Linux/Android, exercise rejected/cancelled ROM and menu paths, inspect launcher captures at Deck and phone sizes.

### Task 4: Steam Deck AppImage

**Files:** Create `tools/make_appimage.sh`, `packaging/appimage/AppRun`, desktop/icon assets and packaging tests; modify release/SteamOS documentation and build scripts.

**Interfaces:** Configurable `BUILD_DIR`, `APPIMAGE_TOOL`, `APPIMAGE_RUNTIME`, output directory; stage AppDir separately from actual squashfs creation for testability.

- [ ] Write failing shell integration test using test-owned AppDir and fake executable: writable state, spaced paths, unchanged custom mods, unchanged saves, no ROM payload.
- [ ] Implement dependency discovery, safe staging, native assets/licenses and AppRun XDG resolution; preserve supplied relative paths.
- [ ] Build Linux in compatible environment, inspect glibc/dependencies, generate AppImage with available toolchain, run extracted AppRun/headless and no-FUSE startup.
- [ ] Update guides and source attribution; run focused cross-subsystem regressions.

### Task 5: Delivery review

- [ ] Record commands and concrete results in execution ledger; distinguish device acceptance from build checks.
- [ ] Fresh reviewer checks whole change against spec; address important findings with regression tests.
- [ ] Run final builds/tests, inspect diff/status, and deliver local artifacts/branch without pushing or publishing.
