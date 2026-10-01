# Campaign saves implementation plan

> **For agentic workers:** Use superpowers:executing-plans for integration and superpowers:dispatching-parallel-agents for the independent storage, snapshot, and cartridge investigations. Track verification here as tasks complete.

**Goal:** Implement the approved Continue/autosave/recovery feature and a cartridge-verified password bridge.

**Architecture:** Keep the runner's snapshots as payloads inside checked save envelopes. Save management owns storage and compatibility; cartridge checkpoint predicates own campaign transitions; the application and overlay integrate their results. Snapshot loading becomes transactional before any new Continue path uses it.

**Tech stack:** C11, SDL2, existing SHA-256 implementation, CMake, Python/pytest native harnesses.

**Spec:** `docs/superpowers/specs/2026-10-01-campaign-saves-design.md`

## Global constraints

- Preserve the pending multiplayer/README work already in this checkout.
- Support runner snapshot versions 4–8 and preserve explicit legacy manual loading.
- Never mutate gameplay on rejected loads or overwrite another mod context's campaign.
- Two prior valid autosave generations; atomic publication; bounded single-file envelope.
- Applied ROM/gameplay data determines compatibility; pending mods and presentation settings do not.
- Native tests use isolated save directories; real ROMs stay ignored and are never committed.

## Review focus

- Corrupt primary plus a valid backup must recover without rotating corrupt data into a backup.
- Pending mod selection must not change the running context's fingerprint.
- A failed raw snapshot load must restore CPU, peripherals, and ISSD host resume state.
- A sanitized failed frame must not be eligible for autosave.
- A password accepted natively must round-trip through the actual cartridge algorithm.

## Task 1: Checked storage and recovery

**Files:** `ISSDNative/issd_save.[ch]`, focused new storage helper if required, `tests/test_campaign_saves.[c|py]`.

**Interfaces:** Preserve existing slot/backends APIs. Add `bool issd_save_set_directory(const char *)`, `void issd_save_set_context(const uint8_t *, size_t, const uint8_t *, size_t, uint32_t)`, `bool issd_save_campaign(const char *)`, `bool issd_save_continue(void)`, `bool issd_save_continue_info(char *, size_t)`, `const char *issd_save_error(void)`, `bool issd_save_is_legacy(int)`, and `bool issd_load_from_slot_confirmed(int, bool)`.

- [x] Write a native harness with backend fixtures that preserve a recognizable payload. Assert three saved generations, corruption recovery, no mutation on bad length/digest/context, separate context histories, and interrupted writes.

```c
assert(issd_save_set_directory("test-saves"));
issd_save_set_context(base, sizeof base, patched, sizeof patched, 0);
assert(issd_save_campaign("Cup setup"));
assert(issd_save_continue_info(info, sizeof info));
assert(issd_save_continue());
```

- [x] Run `python -m pytest tests/test_campaign_saves.py -q`; confirm the missing APIs/behavior fail before implementation.
- [x] Implement explicit envelope encoding, bounded reads, SHA-256 over metadata/payload, context directories, durable temp writes and atomic replacement, validated generation rotation, legacy detection/confirmation, and stable user save roots.
- [x] Re-run the harness and existing native save-video test. Inspect all failures; retain production fallback behavior when no context/backend is installed for the isolated harness.

## Task 2: Transactional snapshots and replay

**Files:** `deps/snesrecomp/runner/src/common_rtl.[ch]`, serializer sources only if necessary, focused snapshot tests.

**Interfaces:** Existing `RtlSaveSnapshot`, `RtlLoadSnapshot`, and memory equivalents retain their signatures. The loaders reject failure without state mutation.

- [x] Add a native/ROM-backed regression that truncates a snapshot after valid magic/version, attempts a load, and compares pre/post machine state.
- [x] Run the new check and existing `tests/test_savestate_replay.py`; capture failures.
- [x] Separate validation from commit or protect loading with an in-memory rollback. Reconcile game execution contexts only on success; restore state on commit failure. Protect the audio state using existing lock boundaries.
- [x] Diagnose the existing replay mismatch using frame-boundary save/load comparisons; keep the image assertions meaningful.
- [x] Run native snapshot tests, raw v7/v8 fixtures, and `python -m pytest tests/test_savestate_replay.py tests/test_local_multiplayer.py -q` against updated Windows builds.

## Task 3: Cartridge campaign and password flow

**Files:** `ISSDNative/issd_campaign.[ch]`, password module if verified, `tests/test_campaign_transitions.[c|py]`, ignored local cartridge fixtures.

**Interfaces:** `void issd_campaign_reset(void)` and `const char *issd_campaign_tick(const uint8_t *ram, bool healthy)` return a checkpoint label exactly once for a verified committed transition; null means no checkpoint.

- [x] Trace Cup/World Series setup, progression/results, and password generation/restoration using generated routines and the local supported ROM. Record exact addresses/predicates and fixture observations.
- [x] Add tests proving correct transitions save once and half-time/replays/demos/unhealthy frames never do.

```c
issd_campaign_reset();
assert(issd_campaign_tick(ram, false) == NULL);
assert(issd_campaign_tick(ram, true) == NULL);
/* A verified stable eligible second frame yields one label only. */
```

- [x] Implement the verified state predicates with two healthy completed observations, then test healthy/no-duplicate/reset behavior.
- [x] Implement genuine password encode/decode only after the cartridge field packing, alphabet, and validity checks have been confirmed. Reject modified campaign contexts and invalid imports without mutation.
- [x] Verify both campaign types with cartridge/native round trips and persistence after password import.

## Task 4: Application and overlay integration

**Files:** `ISSDNative/main.c`, `ISSDNative/issd_menu.[ch]`, `cmake/issd_sources.cmake`, relevant test harness linkage.

- [x] Add integration tests for explicit CLI save directory, Continue success/failure, applied context changes, and menu selection/legacy confirmation.
- [x] Add `--save-dir`, `--continue`, and explicit legacy-load opt-in for headless fixtures. Initialize the save context from base/effective ROM data at boot/reapply and when effective gameplay overrides change.
- [x] Add Continue to the overlay with summary/recovery errors, show/select it on graphical startup only if available, and preserve existing Resume/manual slot actions. Clear held input after successful load; clarify quit labels.
- [x] Capture frame health before NMI sanitization. Observe campaign state after completed drawing/IRQ work at a consistent boundary; invoke one autosave per verified checkpoint. Reset observers after loads/reset.
- [x] Include new modules in every platform's shared source list. Build both existing Windows targets and run focused campaign/multiplayer/replay checks.

## Task 5: Review, documentation, and complete validation

**Files:** README, changelog, campaign save documentation, this plan.

- [x] Run `python -m pytest tests -q --tb=short`; separate newly introduced failures from the five previously reproduced baseline failures.
- [x] Review corrupt files, same-name edited mods, recovery, legacy loading, frame health, and cartridge password restrictions against the spec.
- [x] Update documentation with actual controls, save locations, migration/backup behavior, implementation status, and device-test limitations.
- [x] Run `git diff --check`; review scoped changes without reverting existing work. Record exact passing/failing checks and any genuinely unresolved requirements.

## Execution record

User approved the spec and explicitly requested implementation. Execute in this chat without another approval prompt. Preserve current pending work on a feature branch; do not push or publish.


## Final verification (2026-10-01)

Both approved stages are implemented in the current source. The native overlay
provides Continue and the 64-symbol Password page. Storage uses checked,
context-specific envelopes and two prior valid generations. Original cartridge
routines commit campaign progress and restore accepted imports; native observers
save only verified healthy stable transitions.

- Full project suite: `python -m pytest tests -q --tb=short` returned
  **146 passed, 2 failed, 1 skipped, 7 subtests passed** in 377.21 seconds.
  The only failures are the previously reproduced `test_mod_rom_patching`
  appearance-nibble expectation and `test_shipped_packs_validate` kit-sharing
  warnings. The two old menu harness linking failures and advancing replay
  mismatch are repaired.
- Password codec: **43 passing** cases across all six original formats,
  multiple teams/progress/seeds, four actual result/completion goldens, invalid
  input preservation, and retail context restrictions. Production bridge:
  **3 passing** actual export/import/autosave/Continue round trips. UI:
  **5 passing** palette, navigation, text, callbacks, and rendering checks.
- Campaign fixtures execute actual Cup/World setup and result transitions;
  Cup group/knockout/final and World round-35 completion persist and Continue
  without duplicate saves. Fixtures shorten private match clocks and seed
  disclosed prior progress; original code calculates standings and champion.
  See `docs/CAMPAIGN_CARTRIDGE_FLOW.md` for the exact evidence.
- Recovery, invalid envelopes, incompatible applied packs, same-name edits,
  equivalent pack formatting, legacy confirmation, failed publication,
  simultaneous writers, and transactional deserialization pass. Frame health
  is measured before stack/NMI repairs. Paused metadata queries are cached.
- Fresh builds pass for `build/` and `build-fixes/`, Linux under WSL Ubuntu
  24.04 with SDL2, and Android release `arm64-v8a` and `x86_64` with JDK17,
  Gradle8.9 and the installed SDK/NDK. All artifacts are newer than the native
  source. Linux passed a 120-frame save-at60/load-at80 smoke check via a script
  with isolated configuration/storage. Android APK assembled successfully.
- Reviews caught and resolved stale active-ROM data during Android in-process
  mod reapplication, stalled password submit callbacks, import-arm loss,
  duplicate setup observation, and repeated paused snapshot validation.
  Applied bytes now synchronize into the runner cartridge before clearing
  execution caches and recomputing compatibility identity.
- `git diff --check` is clean. The supported ROM remains ignored. No ROM,
  generated captures, user saves, or credentials are included in source edits.

Physical controller/touch playtests and comparison with cartridge hardware
remain unperformed. The SteamOS sniper SDK package was not rebuilt; macOS has
no validated port. These are validation/release gaps, not claims of completed
1.0 certification. Existing multiplayer edits are preserved. No push or
publication was performed.
