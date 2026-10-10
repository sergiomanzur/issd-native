# Changelog

## Unreleased

- Add persistent Classic / Tactical 80% / Tactical Wide 67% camera choices for
  1.0 development, using real expanded stadium coverage, signed actors,
  replay player history and independently sized HUD elements. Preserve original
  scanout for unsupported scenes and native colors during artwork color effects.

- Keep black campaign/tournament cards centered in True widescreen; extend
  their backdrop without repeating reflected lettering into the margins.
- Preserve real edge-player positions and resolved poses alongside original
  replay records. Restore omitted actors only from matching recorded history;
  retain this history in saves while accepting older snapshot extensions.
- Document camera/FOV feasibility and the fixed 2D projection limits.

## 0.5.0-beta.1 — 2026-10-10

- Correct expanded stadium selection in both menu wrap directions, retain
  custom logical stadium IDs through original settings serialization, and
  copy the byte-indexed name graphics table without reading weather data.
  Legacy added slots reuse the original eight match layouts.

- Add bounded independent stadium profiles: shortened engine pitches, authored
  native maps/characters/palettes and stadium-local HD sets. Preserve geometry
  save identity and refresh changed or removed cosmetic resources safely.
  Two same-template profiles complete uninterrupted natural matches.
- Expand Mod Studio with geometry/artwork editing, compiled camera previews,
  image import, undo/redo, portable export and isolated asynchronous Test/Stop.
  Include two original-art stadium examples and a rebuilt standalone editor.
  See `docs/INDEPENDENT_STADIUM_ACCEPTANCE.md` for scope and evidence.

- Exclude unrecorded players from widened goal-replay margins instead of drawing
  their stale live-match positions and poses. Recorded replay edge sprites and
  live-play culling remain intact; natural-goal playback, pause and rewind pass
  native-center/state comparisons at 4:3, 16:10, 16:9 and 21:9.
- Save completed Cup shootouts at the original settled ceremony, including
  sudden death, before the cartridge returns directly to the title screen.
  Verify original substitution limits, both controller sides, halftime and
  extra-time replacement flows with advancing snapshot replay.
- Verify an uninterrupted, naturally timed 35-match World Series and production
  Continue from every result and completion; retain diagnostic build manifests
  and native checkpoints. Natural Cup qualification/elimination also passes;
  an untouched winning Cup remains an acceptance gap.
- Pace realtime sound transfers against FIFO consumption with the audio mutex
  released, preventing the reproduced loss of title/commentary PCM. Bound CPU
  timestamps to the current guest frame and allow the SPC startup delay to
  finish with a genuine acknowledgement. Stalled devices incur one bounded
  wait until consumption resumes; headless and fast-forward stay unblocked.
- Extend blue menu wallpaper after stadium initialization, including halftime
  management, formations, squad substitutions and returned menus. Clip panels,
  text and sprites to the original center instead of repeating them at the sides.
- Continue authored stadium top/bottom boundary tiles into widened margins,
  avoiding blank map padding near the goal area.
- Add Restart Match and Back to Main Menu directly below Resume in the overlay.
  Match restart retains its captured kickoff; returning to the menu preserves
  existing campaign saves. Restart requires a compatible kickoff checkpoint;
  returning also supports fresh Continue through the original startup flow.
- Preserve HD tile scanout data through render transactions, protect player
  pixels, and apply HD textures across widened margins with correct wallpaper
  repeat/mirror behavior. Back to Main Menu also works after a fresh Continue.

## v0.4.0-beta.1 (2026-10-03)

- Replace the coin-scene's repeated 8-pixel edge slivers with wider authored
  scenery sections and complete close-up fans, retaining vertical scroll and
  one centered television.
- Widen halftime/fulltime statistics scenery for 16:10, 16:9 and 21:9 while
  retaining the original centered card. Explicitly clip presentation sprites
  during the coin-toss scroll and stats screens.
- Remove the ball outline and ignore saved legacy enables. Add optional Color Boost
  and adjustable CRT Strength with saved settings and preview support.
- Extend the verified coin-toss introduction/minigame background to 16:10, 16:9
  and 21:9 while keeping its native panels and action clipped to the original view.
- Replace modded moving team identities using matched native sprite geometry,
  including competition screens and coin-toss panels.

- Add an optional eight-direction enhanced running/dash animation: eight new
  palette-indexed lower-body poses per direction, retaining original gameplay
  timing, sprite positions, palettes and priority.
- Add a saved Original/Enhanced Running Animation setting to Graphics. Original
  and Sharp presets disable it; Enhanced enables it.
- Add real-cartridge pose previews, render isolation and classic/widescreen
  deterministic replay checks.

## v0.3.0-beta.1 (2026-10-02)

Beta prerelease for Windows and Android, with graphics, controls, exhibition
shortcuts, optional original-game corrections and expanded acceptance coverage.
The project remains below 1.0. Android version code advances to 5.

### release reliability

- Preserve host animation and pose history in checked snapshots for deterministic
  advancing widescreen replay, while retaining older snapshot compatibility.
- Redirect explicit kit overrides to isolated home/away palette records; fix
  shared-kit allocations in both shipped packs without hiding validation warnings.
- Correct goalkeeper appearance documentation and tests to match original graphics
  behavior, retaining exhaustive outfield hairstyle coverage.

### original team management acceptance

- Add input-driven original formation/substitution, request cancellation and
  advancing save/reload checks with both AI policies off and on; document
  first-team scope and pending both-team/extra-time/limit cases.
- Add bounded per-actor AI metadata tracing. Keep player and goalkeeper debug
  reads from changing CPU open-bus or cartridge bookkeeping state.
- Preserve original auxiliary vertical culling in widescreen fallback drawing,
  fixing a stale object drawn over the native top edge after management return.

### graphics and readability

- Widen penalty-camera scenery for 16:10/16:9/21:9, retaining the original goal,
  players, HUD and kick mechanics. Add real-shot and successive-turn comparisons.

- Decode original ROM animation tables for verified culled player states, keeping
  native timing/state changes authoritative and deliberate terminal poses held.
- Add reproducible original Cup-final graphics checks across 256/398/504 pixels,
  including pause/replay and opt-in accelerated period/extra-time/shootout entry.

- Add Original/Sharp/Enhanced visual presets, derived Custom status, a filtered
  sample preview and a graphics reset which retains display/gameplay preferences.
- Add selected-player label scaling/collision avoidance and enlarged-radar
  placement, background opacity and small-surface fitting.
- Extend ultrawide to a safe 504x224 native view; discard stale animation history
  on scene/submode/layout changes and inactive/reused actor slots.
- Recover SDL textures after renderer resets and add optional graphics timing
  reports with actual output/native/intermediate dimensions.

- Add a Graphics / Readability page, desktop window-size presets, actual output
  and intermediate dimensions, independent integer scaling and Sharp filtering.
- Draw sharply scaled overlays/notifications after filtering the game, with
  scrolling main/settings pages and matching output-pixel mouse/touch handling.
- Add optional ball outline/shadow, P1–P4 markers, selected-player names and
  enlarged radar; defaults preserve the original match appearance.
- Continue learned widescreen locomotion with paired geometry/graphics and
  camera-compensated movement, without changing guest simulation.
- Preserve paused framebuffer stride during aspect changes, update texture
  filters live, and retain notification durations across high-refresh drawing.
- Unsupported wider actions remain limited; physical device acceptance is pending.

### original game bug fixes

- One persistent Original Bug Fixes toggle under Gameplay Tweaks: Off preserves
  original behavior; On enables the verified bundle alongside AI tweaks.
- Fix keeper movement stacking, phantom skill-point refunds, repeated behind-goal
  awards, two-digit score wrap, invalid foul restarts and name graphics overflow.
- Include the switch in save/password compatibility and invalidate resident
  exhibition checkpoints when it changes. Added cartridge and native regressions.
- Stuck-post, substitution-camera and penalty-replay reports remain unverified;
  the bundle does not claim every original defect is fixed.

### exhibition shortcuts

- Gameplay Tweaks now links to Match Shortcuts / Presets: instant exhibition
  rematch, independent mark/restart drill checkpoints and a persistent favorite.
- Retain complete native setups, including teams, kits, weather, stadium,
  lineups, rules and controller assignments. Campaigns and numbered saves are
  independent; incompatible favorites are retained and rejected safely.
- Original (default), Classic, Casual and custom rules use verified original
  duration, five difficulty levels, offside, foul, card and extra-time options.
  Apply at the original pre-constructor boundary rather than during live play.
- Added original-exhibition constructor/replay/favorite acceptance tests and
  failure/compatibility regressions. No verified input-origin scoring exploit
  was identified, so shooting and goalkeeper balance remain unchanged.

### controls and acceptance

- Saved P1–P4 controller profiles with presets, in-game button/axis/trigger
  remapping, and independent adjustable stick/trigger deadzones.
- In-game P1 keyboard remapping; configured bindings now drive input. Default
  aliases are sampled together, so releasing one cannot cancel another.
- Individual normalized touch positions and button sizes, reset actions, and
  rotation-safe bounds. Layout changes clear held touch state.
- Background/foreground pause path clears held inputs, pauses simulation/audio
  and requires explicit resume. Real Android lifecycle validation remains pending.
- VSync applies live where supported; the inactive engine selector now displays
  a fixed baseline rather than advertising an alternate engine.
- Evidence-based supported-feature/setting inventory and repeatable acceptance
  checks distinguish automated fixtures from full campaign/device certification.

## v0.2.0-beta.1 (2026-10-01)

Beta prerelease for Windows and Android; the project remains below 1.0.

### Gameplay tweaks

- Added a Gameplay Tweaks overlay page with independent, persistent,
  default-off goalkeeper and player AI switches, usable together without restart.
- Added bounded goalkeeper shot-plane positioning and formation/role/active
  substitute positioning adjustments at native and interpreted AI decisions.
- Preserved human controller ownership, original movement and save animations.
- Included the enabled combination in campaign/manual-save compatibility and
  retail password eligibility. Added policy, menu, live-match and replay checks.

### Saves, passwords and multiplayer

- Native Continue Campaign with checked snapshots, context-specific autosaves, and two recoverable backup generations.
- Verified Cup group/knockout and World Series setup/result/completion checkpoints, with team metadata and no duplicate saves after Continue.
- Native 64-symbol password entry/export using the cartridge's original packing, checksum, and restore routines; accepted imports autosave once settled. Unmodified retail gameplay is required.
- Atomic save publication, bounded snapshot validation, and transactional restoration that leaves the running game unchanged on failure.
- Compatibility identity from applied ROM/gameplay data; presentation settings remain compatible. New manual states share integrity/context protection; legacy raw states require explicit confirmation.
- Stable per-user save roots plus `--save-dir`, explicit headless `--continue`, and `--allow-legacy-save` options.
- Native CPU/APU timing serialization and corrected completed-frame capture restore exact advancing replay in title, classic live play, and widescreen live play regression tests.
- Local multiplayer for up to four SDL2 gamepads, including the original game's port-two multitap detection and input protocol.
- Stable player slots, connection/disconnection notifications, and automatic pause when a gamepad disconnects during live play.
- Frame-based gamepad sampling prevents stuck or cancelled controls when sticks, D-pad, buttons, and triggers overlap. Held overlay controls are consumed until released; unfocused windows send no gameplay input.
- Save format v8 preserves multitap state; older v4–v7 snapshots remain supported.
- Deterministic input scripts accept optional P1–P4 labels for multiplayer regression tests.
- In-process mod reapplication synchronizes the active cartridge image before resetting execution caches and save compatibility identity.

### Packaging and validation

- Windows x64 ZIP, Android APK and ZIP, existing Liga MX and World Cup mod packs,
  and SHA-256 checksums. No ROM or personal configuration/saves are packaged.
- Android version code advances to 4; version name is `0.2.0-beta.1`.
- Full suite: 156 passed, 2 existing mod-pack failures, 1 skipped, 7 subtests
  passed. Final focused AI/menu/save/replay/multiplayer/password checks:
  34 passed, 1 skipped. Windows, Linux and Android builds succeeded.
- Gameplay balance, wider mode coverage and device validation remain in progress.
  Android uses the existing debug signing configuration for sideload testing.

See [the release notes](docs/releases/v0.2.0-beta.1.md) for install instructions
and known limitations.

## v0.1.1b

### Highlights

- **Vertical Stripes Kit Support (`stripes: true`)**: Added native support for striped kit overlays in the mod system. In-game player sprites can now feature vertical stripes on their shirts using the SNES jersey detail table `$81:CE8A` overlay (e.g. CD Guadalajara / Chivas, CF Monterrey, Club Necaxa, Atlético San Luis, Atlante FC).
- **Away Kit & Change Strip Customization**: Fixed kit repainting in `patch_kit` to update both the Home kit pointer table (`$82:827A`) and Away kit pointer table (`$82:82D0`), ensuring teams playing as Away (P2, or P1 in away kit) wear their authentic customized uniforms on the pitch rather than falling back to original cartridge palettes.
- **Match State Team Resolution Fix**: Fixed team ID resolution in `issd_bridge.c` where RAM byte offsets `0x0DA0` and `0x0EA0` are properly scaled to 0-based team IDs, preventing mismatches in HUD rendering and match overrides.
- **Goalkeeper Sprite Integrity**: Fixed SNES VRAM sprite table corruption where field player hair attribute bits inadvertently corrupted goalkeeper composite head and glove animations.
- **Team Selection & Menu HUD Overlays**: Pixel-perfect alignment and positioning for upscaled flags in team selection grid cells, Handicap selection, Tonight's Game pre-match screen, Coin Toss minigame, and in-game live scoreboard HUD.
- **Liga MX y Expansión MX Mod Pack (Apertura 2026)**: Complete 36-team mod pack including all 18 Liga MX first-division clubs and 15 Liga de Expansión clubs (plus 3 historical clubs) updated to Apertura 2026, featuring:
  - Authentic flag pixel art and high-resolution squad photos.
  - Dynamically calculated SNES player attributes for all 720 players.
  - Club América with vibrant canary yellow (`#FFE600`) and navy blue.
  - CD Guadalajara (Chivas) with authentic red & white vertical stripes on the pitch, blue shorts, and white socks.
- **FIFA World Cup 2026 Mod Pack**: Official 32-nation mod pack for World Cup 2026 with up-to-date rosters, tactical formations, upscaled national flags, and custom team kits.
- **Android Touch Overlay Menu Navigation**: Fully enabled touchscreen navigation in the in-game overlay menu. Players can navigate menus using on-screen D-pad buttons (Up/Down/Left/Right), confirm/cancel with action buttons (A/X/Start and B/Y/Select), or tap directly on menu items to toggle settings.
- **Android ROM Picker Stability**: Resolved crash when selecting a ROM from the system file picker by waiting asynchronously for user selection and executing a clean process restart upon importing ROM/mods.

### Packaging

- Windows artifact: `ISSDNative-v0.1.1b-windows-x64.zip`
- Android APK: `ISSDNative-v0.1.1b-android.apk`
- Android ZIP package: `ISSDNative-v0.1.1b-android.zip`
- Mod Pack: `liga_mx_expansion.zip`
- Mod Pack: `world_cup_2026.zip`

## v0.1.0-beta.2 - Second Beta

### Highlights

- **Goalkeeper Sprite Integrity**: Fixed SNES VRAM sprite table corruption where field player hair attribute bits inadvertently corrupted goalkeeper composite head and glove animations.
- **Team Selection Menu Alignment**: Pixel-perfect alignment and positioning for upscaled national team flags in both grid cells and preview cards.
- **Custom Squad Photographs**: Integrated high-resolution squad photograph overrides supporting replacement nations with authentic official kit colors (e.g. Canada red/white, Ecuador yellow/blue).
- **Handicap & Tonight's Game Overlays**: Dynamic flag and name banner overlays on the Handicap selection screen and the pre-match Tonight's Game presentation screen.
- **Pre-Match Presentation Scene**: Seamless sky rendering and authentic typography for custom teams during the pre-match fly-in banner cutscene.
- **In-Game Scoreboard HUD**: Real-time HUD scoreboard support displaying custom national flags, country name plates, and match scores for both Player 1 and Player 2 squads.
- **FIFA World Cup 2026 Mod Pack**: Standalone official mod pack release including all 32 qualified nations, rosters, 4-2-3-1/4-3-3 formations, custom flags, and official kits.

### Packaging

- Windows artifact: `ISSDNative-v0.1.0-beta.2-windows-x64.zip`
- Android artifact: `ISSDNative-v0.1.0-beta.2-android.apk`
- Mod Pack artifact: `ISSDNative-WorldCup2026-ModPack.zip`

## v0.1.0-beta.1 - First Beta

ISSD Native is a passion project: a native recompilation and modernization effort for International Superstar Soccer Deluxe that preserves the original gameplay while making it comfortable to run on modern hardware.

This release does not include any ROM, cartridge data, copyrighted game assets, BIOS files, or commercial media. You must provide your own legally dumped cartridge image.

### Highlights

- First public beta release for Windows and Android.
- Complete native recompilation coverage for the original game code path.
- Playable menus, exhibition matches, cups, scenarios, training, and penalty shootout modes.
- Authentic baseline gameplay, animation timing, ball physics, player inertia, referee behavior, collision behavior, and tactical AI.
- Native C asset decompression and memory streaming paths for faster boot and runtime behavior.
- Native audio fast path for Konami SPC700 command handling, sound effects, music, and announcer voice triggering.
- SDL2 video, audio, keyboard, and gamepad support.
- Modern controller layout option alongside classic SNES-style controls.
- In-game overlay menu for presentation, audio, gameplay, and mod settings.
- True widescreen rendering support for 16:10, 16:9, and ultrawide viewports without stretching the SNES image.
- Internal resolution scaling, nearest/linear filtering, and CRT scanline presentation options.
- Save-state and quickload support.
- Mod stack support for roster, formation, kit, stadium, team photo, team plate, and HD tile replacement packs.
- Windows ROM selection through a native file picker.
- Windows mods folder selection through a native folder picker, plus `mods_dir` config support.
- Android ROM picker and mods folder picker integration through the platform file picker.
- Android touch overlay support for handheld play.
- Headless regression and screenshot/dump modes for automated validation.

### Packaging

- Windows artifact: `ISSDNative-v0.1.0-beta.1-windows-x64.zip`
- Android artifact: `ISSDNative-v0.1.0-beta.1-android.apk`

### Known Notes

- Windows and Android packages intentionally ship without a ROM.
- The Android APK is a beta/dev-style release build signed with the debug signing configuration for sideload testing.
- This release is intended for testing, preservation, and research by users who own the original cartridge.
