# Tactical Camera Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Deliver two genuine tactical zoom-outs alongside Classic for the 1.0 development version.

**Architecture:** Add a host-only direct world renderer sharing stadium tiles and actor decoding with widescreen. Render an expanded viewport at 4/5 or 2/3 scale, composite the HUD separately, and keep the guest simulation and original scanout unchanged.

**Tech Stack:** C11, existing SNES PPU and SDL2, CMake, pytest-driven C fixtures, native headless acceptance runs.

**Spec:** `docs/superpowers/specs/2026-10-10-tactical-camera-design.md`

## Global Constraints

- Classic is the default for existing configurations.
- Changing the camera must not change guest execution, input, audio timing, collision, activation, replay payloads, or match outcomes.
- All presentation transactions must restore original PPU VRAM/OAM/register state before guest code resumes.
- Keep the output aspect ratio independent of camera scale.
- Do not change the public beta, bump the release version, or publish cartridge-derived fixtures as part of this work.
- Android runtime performance is reported as unverified unless tested on an available device.

## Review Focus

- A viewport wider than 512 pixels must not alias its opposite edge through the SNES map ring.
- Actors above/below native scanout must not wrap by 256 pixels or appear from stale auxiliary records.
- A mode switch during replay pause/rewind must preserve the selected recording generation.
- Custom stadium graphics and boundary metatiles must remain selected at every expanded edge.
- Camera configuration corruption must resolve to Classic without changing unrelated preferences.

## Execution and baseline

Execute inline in this session after plan review. Preserve pending replay/card changes on `codex/campaign-saves`; do not reset or stash them. Verify isolation using the worktree skill before starting product code. Record task commits, commands, results, and justified deviations in this plan's execution ledger. Capture a clean Classic baseline before changing shared decoding. Existing release packages remain immutable.

### Task 1: Camera geometry and configuration

**Files:** Create `ISSDNative/issd_camera.h`, `ISSDNative/issd_camera.c`, `tests/test_camera.c`, `tests/test_camera.py`. Modify `ISSDNative/issd_config.h`, `ISSDNative/issd_config.c`, `tests/test_config_persistence.c`, `cmake/issd_sources.cmake`.

**Interfaces:** Produce `IssdCameraMode` (Classic=0, Tactical=1, TacticalWide=2), `IssdCameraView { int x,y,w,h; }`, `IssdCameraView issd_camera_view(int width,int height,IssdCameraMode mode)`, and `int issd_camera_project(int coordinate,int origin,int numerator,int denominator)`. Store `camera_mode` in host config. For a logical output W/H, world size is ceil(W*den/num), ceil(H*den/num); origin is centered relative to native coordinates, including existing horizontal extra. Project signed coordinates with mathematical floor rather than C truncation. Classic uses 1/1, Tactical 4/5, Wide 2/3.

- [ ] Write geometry and config regression tests before code. Core cases:
  ```c
  IssdCameraView v=issd_camera_view(398,224,ISSD_CAMERA_TACTICAL);
  assert(v.w==498 && v.h==280 && v.x==-121 && v.y==-28);
  v=issd_camera_view(398,224,ISSD_CAMERA_TACTICAL_WIDE);
  assert(v.w==597 && v.h==336 && v.x==-171 && v.y==-56);
  assert(issd_camera_project(-1,0,4,5)==-1);
  ```
  Test 256/320/358/398/504 widths, odd dimensions, invalid modes, and config missing/negative/too-large camera values. Persist both valid modes across a real file round trip.
- [ ] Run `python -m pytest tests/test_camera.py tests/test_config_persistence.py -q`; expect failure because the new camera API/config behavior is absent.
- [ ] Implement only rational geometry and validated config storage; add `issd_camera.c` to the shared port source list. Reject invalid dimensions with a zero view. Parse out-of-range camera values as Classic, not clamp to Wide. Save `camera_mode` as an integer key.
- [ ] Rerun the same tests; expect pass. Run existing video/config preset tests found in `tests` to preserve preset behavior.
- [ ] Commit the task's files only, with `feat: add tactical camera geometry and persistent mode`.

### Task 2: Direct stadium world sampling

**Files:** Modify `ISSDNative/issd_widescreen.c`, `ISSDNative/issd_widescreen.h`, `ISSDNative/issd_camera.c`, `ISSDNative/issd_camera.h`. Create `tests/test_camera_world.c`, `tests/test_camera_world.py`.

**Interfaces:** Consume Task 1 viewport. Produce read-only `bool issd_widescreen_world_tile(const uint8_t *ram,unsigned layer,int x,int y,uint16_t *tile)` sharing `world_bounds/world_tile`; produce `bool issd_camera_render_world(const Ppu *ppu,const uint8_t *ram,const IssdCameraView *view,uint32_t *pixels,size_t stride)` for the expanded field surface. Return false for invalid layout/data. No global camera state in the sampler.

- [ ] Build synthetic asymmetric metatile, palette, and graphics fixtures. Assert distinct samples at world x=0 and x=512, vertically outside native rows, flipped tiles, transparent priority tiles, and authored boundary continuations. Snapshot PPU and WRAM before every render and compare afterward:
  ```c
  assert(issd_camera_render_world(ppu,ram,&view,pixels,stride));
  assert(memcmp(ram,ram_before,0x20000)==0);
  assert(memcmp(ppu->vram,vram_before,sizeof ppu->vram)==0);
  ```
- [ ] Run `python -m pytest tests/test_camera_world.py -q`; expect missing API failure.
- [ ] Extract existing metatile lookup without changing classic lookup results. Decode tile pixels from the active PPU graphics/palette, respecting character bases, tile flips, BG priority, transparency, brightness and applicable main/subscreen color math. Use the current world page recovered from WRAM plus PPU scroll, as `fill_pitch` does; never mask tactical world positions into the 512-pixel ring. Match active custom-art overlays through the existing artwork path.
- [ ] Run new world tests plus `tests/test_widescreen_native.py`, `tests/test_stadium_replay_art_acceptance.py`; expect pass and unchanged Classic output. Record any unsupported raster effect as an explicit eligibility restriction.
- [ ] Commit `feat: render expanded tactical stadium coverage`.

### Task 3: Signed actor geometry and replay coverage

**Files:** Modify camera/widescreen modules; modify `ISSDNative/issd_snapshot.c`, `.h` only if additional recorded actor data is required. Create `tests/test_camera_actors.c`, `.py`; extend `tests/test_replay_history.c` and snapshot compatibility fixtures.

**Interfaces:** Produce `IssdCameraPiece { int x,y,size; uint16_t tile; uint8_t attributes; }` and `bool issd_widescreen_camera_pieces(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,size_t rom_size,const IssdCameraView *view,IssdCameraPiece *pieces,size_t capacity,size_t *count)`. Consume stable presented RAM and the existing selected replay companion generation. Capacity exhaustion returns false and uses the frame fallback; never silently drop actors.

- [ ] Test packed and unpacked poses spanning all four edges, y=-40/y=280, x beyond 512, mirrored pieces, heights, ordering, ball/shadows, valid officials, and stale auxiliary records. Include replay selection before cursor advance, payload overwrite, pause/rewind, and old save history absence. Assertions use actual rendered pixels and complete signed piece coordinates, not OAM wrapping.
- [ ] Run `python -m pytest tests/test_camera_actors.py tests/test_replay_history.py tests/test_snapshot_transactional.py -q`; expect new actor API failure.
- [ ] Share geometry decoding and resolved animation descriptors with `fill_objects`. Isolate private graphics uploads in the established presentation transaction. Decode actor coordinates before OAM packing, and rasterize signed pieces directly. Audit original replay ball/official/goal coverage against fresh native recordings; add versioned companion fields only for actors whose recording is demonstrably missing. Preserve old save acceptance and transactional load rejection. Record any snapshot format change and its exact compatibility tests in the ledger.
- [ ] Rerun the task tests plus goal replay, running-animation and existing widescreen guard tests; expect pass. Confirm genuine rendering beyond vertical admission without treating stale offscreen auxiliary records as live.
- [ ] Commit `feat: render tactical actors with replay history`.

### Task 4: Frame integration, HUD, and graphics controls

**Files:** Modify `ISSDNative/main.c`, `ISSDNative/issd_menu.c`, `ISSDNative/issd_menu.h`, camera modules. Create `tests/test_camera_native.py`; extend existing graphics menu acceptance tests located with `rg --files tests`.

**Interfaces:** Consume Task 2/3 rendering APIs. Produce `bool issd_camera_compose(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,size_t rom_size,IssdCameraMode mode,uint32_t *output,int width,int height)`; returns false for Classic or unsupported scenes, leaving the original output intact. Caller invokes it while the private presentation transaction is valid, after native scanout and before transaction restoration. It renders into private surfaces and publishes the result only on complete success.

- [ ] Add native baseline/tactical comparisons asserting equal full guest WRAM and original replay payloads after identical inputs. Assert enlarged visible pitch, stable HUD pixel dimensions, unmodified menu/card output, and config persistence through menu interaction. Test changes while paused and scene transitions.
- [ ] Run `python -m pytest tests/test_camera_native.py -q`; expect missing camera behavior failures.
- [ ] Integrate composition inside `IssdDrawPpuFrame`, using the presented generation already owned by widescreen. Extract scoreboard/timer/radar/labels/replay controls using validated layer/object identity; scale only the field. Apply camera actor transforms to field markers/names/shadows. Keep framebuffer output dimensions unchanged so SDL filters, screenshots, touch controls and output resolution remain compatible. Add Camera row before graphics Back row, update row counts/navigation/rendering, and retain Android access through the same graphics page. Cache size changes only at frame boundaries. Unsupported layouts retain original scanout.
- [ ] Run native camera/menu/card/replay tests and record mode changes, pause, rewind, menus, shootouts and substitutions explicitly. Expect identical guest state and stable HUD. A scene not verified remains a documented fallback, not claimed complete.
- [ ] Commit `feat: expose tactical cameras with independent HUD composition`.

### Task 5: Acceptance, portability, performance, and 1.0 documentation

**Files:** Extend `tests/test_camera_native.py`; update `docs/CAMERA_INVESTIGATION.md`, `docs/GRAPHICS_SETTINGS.md`, `CHANGELOG.md`; create `docs/TACTICAL_CAMERA_1_0_VALIDATION.md`.

**Interfaces:** Consume all camera APIs and existing native acceptance harnesses. No release publication interface.

- [ ] Add natural goal and full-match captures at both zoom levels and 16:10/16:9/21:9. Verify all edges, camera world-page crossings, boundary corners, custom geometry/artwork, save/load replay, shootouts and substitutions. Keep cartridge pixels and fixture saves under ignored build directories.
- [ ] Run `python -m pytest tests/test_camera.py tests/test_camera_world.py tests/test_camera_actors.py tests/test_camera_native.py -q`; expect all pass. Then run the project suite `python -m pytest -q`, redirecting logs to this plan's ignored workspace, and report every skip/failure rather than claiming blanket coverage.
- [ ] Build Windows with `cmake --build build/release-validation/windows --parallel 6`, Linux via the existing WSL Ubuntu-24.04 release-validation build, and Android via the installed Gradle wrapper/runtime and existing release-validation init script. Expect successful exit from all three; no GitHub upload.
- [ ] Measure repeated-frame median/p95/max rendering time and peak camera allocation for Classic/Tactical/Wide on the same native run. Document host/device and limits; test Android runtime if a device is available, otherwise list it as outstanding. Avoid inferring runtime readiness from APK compilation.
- [ ] Update user docs with camera choices, scene fallbacks, history limitations, and measured validation. Mark this as a 1.0 development feature with remaining release gates, without bumping VERSION or rewriting published beta evidence.
- [ ] Commit `docs: validate tactical cameras for 1.0`; request one independent whole-change review under the code-review skill, resolve material findings with failing tests first, and provide local build links and actual test results.
