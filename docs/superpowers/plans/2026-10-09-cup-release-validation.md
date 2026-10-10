# Untouched Cup and Release Validation Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development for the independent Cup and platform build work. Preserve other workers' edits.

**Goal:** Verify a fresh controller-only winning Cup and current Windows/Linux/Android builds, complete tests and clean-install behavior.

**Architecture:** Keep Cup input automation in an isolated diagnostic executable with unchanged gameplay/runtime objects and no guest-memory writes. Build platforms in owned output directories from current source; test packaged copies with private ROM/config/save roots.

**Tech Stack:** C, Python/pytest, CMake/Ninja/Clang, WSL Linux, Android Gradle/NDK.

**Spec:** User request to complete steps 1 and 3; docs/CONTINUOUS_CAMPAIGN_ACCEPTANCE.md and docs/ACCEPTANCE_TESTS.md define existing evidence and gaps.

## Global Constraints

- Never edit Cup clocks, scores, stages, entrants, callbacks or winner; no passwords, snapshot loads or gameplay tweaks in the final continuous run.
- Diagnostic WRAM reads and ordinary controller input are allowed; production game rules stay original.
- Preserve all existing uncommitted edits, private ROM and personal saves.
- Retain source/build hashes, commands, logs, checkpoint envelopes and screenshots in owned ignored build directories.
- Do not bump versions, publish, tag or replace existing release archives.
- If a check cannot pass, report its actual failure rather than certify a partial run.

## Review Focus

- Cup controller semantics, possession and stamina must come from original code and actual runtime evidence.
- A winning certificate must prove fresh setup, every scheduled match and an original champion, not elimination or seeded progression.
- Continue must restore tables/settings/teams from native envelopes without resaving on load.
- New builds must incorporate the current audio, shootout and replay corrections.
- Clean packages must run without repository-relative assets and exclude ROMs, local configs and personal saves.

## Task 1: Untouched winning Cup

- [x] Inspect original control mapping and improve diagnostic input policy only.
- [x] Prove normal inputs can win an actual match without edits (nomash09 qualifier 7–5 and subsequent fulltime wins).
- [x] Run fresh Cup continuously through all nine matches and original championship.
- [x] Retain all native checkpoint generations and verify production Continue from owned copies.
- [x] Record exact scope and any unresolved gap in docs/CUP_WINNING_ACCEPTANCE.md.

## Task 2: Platform builds

- [x] Configure and build a fresh Windows Release output using existing compiler/SDL settings.
- [x] Build fresh Linux and Android arm64-v8a/x86_64 outputs in independent directories.
- [x] Record output hashes, versions and runtime/APK checks; do not claim physical-device acceptance.

## Task 3: Full checks and clean install

- [x] Install the verified Windows output at the canonical test executable path once stable.
- [x] Run the full project suite including new goal replay tests.
- [x] Stage fresh Windows/Linux/Android packages in owned directories; verify explicit contents and dependencies.
- [x] Extract/run clean Windows copies with private ROM, fresh settings and saves; verify boot, real gameplay, replay and persistence.
- [x] Review evidence, update acceptance documentation and report results and remaining limits.
