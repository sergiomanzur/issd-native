# Supported features and settings

This is the source-based capability audit for **v0.5.0-beta.1**, updated
October 10, 2026. Control remapping, touch layout, lifecycle, exhibition shortcuts,
original bug fixes and graphics/readability improvements describe this beta's
runtime implementation. This inventory takes
precedence over aspirational descriptions in older enhancement,
modding, and fidelity documents. **Delivered** means there is a runtime consumer;
it does not mean every cartridge scene or device has been certified. **Partial**
means an implemented feature has material limits. **Inactive** means a saved
setting has no corresponding runtime behavior. **Planned** means no delivered
implementation was found. See [acceptance checks](ACCEPTANCE_TESTS.md) for what
has actually been tested and what still needs hardware or complete playthroughs.

Current source also adds direct pause-menu Restart Match and Back to Main Menu
actions using compatible session snapshots, and corrects blue wallpaper after
stadium loading and authored vertical stadium boundary padding. See
[pause action limits](MATCH_SHORTCUTS.md) and [graphics settings](GRAPHICS_SETTINGS.md).

The background audit covers the eight original stadium constructors with Liga
MX enabled. Separate HD checks cover scanout ownership and replacements.
Added Mexico logical stadium slots 8–11 now pass original Exhibition menu
selection, live play and native save/reload. Slot 9 also completes a naturally
timed match through checkpoint reloads and advancing goal replay at four aspect
ratios. Added slots retain their logical IDs but inherit the original match
layout `ID & 7` when no independent profile is supplied. The new bounded
`stadium_profile` path supports shortened pitches, authored native maps/artwork
and stadium-local HD sets. Two same-template profiles completed uninterrupted
natural matches. See [independent stadium acceptance](INDEPENDENT_STADIUM_ACCEPTANCE.md)
for geometry, resource and platform limits.
See [added stadium acceptance](EXTRA_STADIUM_ACCEPTANCE.md). Current
HD fixes preserve scanout data, protect player pixels and extend detail into
eligible margins. See the acceptance record for the precise coverage.

## Game, persistence, and input

| Advertised capability | Status and actual behavior | Source and evidence |
| --- | --- | --- |
| Native ISS Deluxe runtime | Partial. Original cartridge code runs through translated routines, interpreter fallback, native hardware models, and HLE replacements. It is not a cycle-accurate SNES emulator or a proven complete recreation. Retail USA, headerless 2 MiB ROM is the supported baseline; ROM is not distributed. | `ISSDNative/main.c`, `recomp/generated/`, `deps/snesrecomp/runner/src/`; `test_boot_reaches_main_loop.py`, `test_headless_match.py`, runtime interrupt tests |
| Original game modes, Cup and World Series | Partial. Original nine-match Cup and 35-match World progression and Continue pass accelerated acceptance. World also completes all 35 naturally timed matches in one process without reloads/state edits, with all-round and completion restoration. Ordinary and sudden-death Cup shootouts complete with original winner/champion and persistence. Both controller sides, original three-replacement limit, halftime and extra-time substitutions pass advancing replay. Natural Cup qualifying elimination passes continuous execution. Full untouched winning Cup, physical controls, alternate teams/settings, scenarios and training remain open. | [shootouts](SHOOTOUT_ACCEPTANCE.md), [substitution flows](SUBSTITUTION_FLOW_ACCEPTANCE.md), [continuous campaigns](CONTINUOUS_CAMPAIGN_ACCEPTANCE.md), [cartridge investigation](GHIDRA_INVESTIGATION.md) |
| Continue and campaign autosave | Delivered for verified Cup/World setup, results, completion, and original password import checkpoints, including the settled terminal shootout ceremony before its direct title exit. Active match, replay, halftime and unfinished ceremonies do not create checkpoints. A failed write leaves earlier committed saves intact; the observed transition is consumed, so retry occurs at a later verified checkpoint rather than every frame. | `issd_campaign.c`, `issd_save.c`, main integration; campaign save/transition and completed-shootout tests |
| Backup recovery and compatibility | Delivered. Current plus two backup generations, integrity checks and pure snapshot validation before publication/load; ROM/mod/debug/gameplay context must match. Successful Continue restores a snapshot, including between-match campaign data. Incompatible saves are preserved and explained, not converted. | `issd_save.c`, `issd_snapshot.c`; `test_campaign_saves.py`, `test_snapshot_transactional.py` |
| Manual saves | Delivered. Quicksave and eight numbered slots; validated envelope and transactional snapshot load. Legacy raw versions 4–8 require explicit approval (`--allow-legacy-save` for CLI), and lack modern context guarantees. | `issd_save.c`, `main.c`, `issd_menu.c`; save, transactional, replay/video tests |
| Original password bridge | Delivered for six verified retail formats. Export uses original cartridge encoding in private memory. Import validates then queues symbols to the original Password screen; the guest applies the campaign. Modified ROM or active gameplay/debug compatibility flags disable the bridge. It is not an arbitrary snapshot-to-password converter. | `issd_password.c`, `issd_password_ui.c`; codec, flow, UI tests |
| Password editor | Delivered. Controller, touch and keyboard; 64-symbol palette, 60-symbol capacity, delete/import/export/back, visible errors and named special icons. Spaces/newlines ignored; case preserved. Keyboard cannot directly type every custom icon: use the palette. | `issd_password_ui.c`; `test_password_ui.py` |
| Four local players | Delivered transport and assignment. Four SDL gamepads by connection order, keyboard/touch additive on P1, stable remaining assignments on disconnect, vacant slot reused on reconnect, fifth pad ignored. Original game mode still determines how many humans play. No network multiplayer. | `issd_input.c`, SNES joypad model, `main.c`; `test_local_multiplayer.py` uses SDL virtual pads and original guest serial reads |
| Control profiles and remapping | Delivered in current sources: per-player Classic/FIFA/PES/custom profiles, button/axis/trigger binding and thresholds; P1 keyboard remapping and persisted configuration. These are mappings of cartridge actions, not FIFA/PES game mechanics. Hardware ergonomics and four physical pads require acceptance. | `issd_config.c`, `issd_input.c`, `issd_controls.c`, `issd_menu.c`; `test_control_profiles.py`, config persistence tests |
| Touch controls | Delivered hit-testing, multi-touch, hide/menu behavior and configurable position/size per control. Touch feeds P1. Menu control stays reachable while the pad is hidden; changing layout clears held touches. Physical touch/device layout acceptance remains pending. | `issd_touch.c`, `issd_menu.c`, main renderer; touch and layout tests |
| Native overlay | Delivered pause/resume, controls, mods, presentation, save/load, Continue, password, gameplay switches, restart and quit; Android also exposes ROM/mod selection. Original in-game menus remain separate. | `issd_menu.c`, `main.c`; native menu tests |
| Exhibition rematch / drill / favorite | Delivered in current sources: full original setup/kickoff, separate resident drill, persistent context-checked favorite and deterministic restore. Campaign, scenario, training and penalty modes excluded. External loads/context changes invalidate resident caches. | `issd_match.c`, `issd_match_menu.c`, `issd_save.c`; shortcut native/model/storage tests; [usage](MATCH_SHORTCUTS.md) |
| Match rule presets | Delivered Original (default), Classic, Casual and custom duration/difficulty/offside/fouls/cards/extra-time using verified original options. Apply at pre-constructor exhibition boundary, not during play. Playing favorite retains its recorded rules. | `issd_config.c`, `issd_match.c`; original constructor/clock/CPU actor acceptance |
| Original Bug Fixes master switch | Delivered for six verified defects: keeper movement stacking, skill-point refunds, repeated behind-goal awards, score wrap, invalid foul restarts and completed-name caret overflow. Defaults off; context-checked saves and AI stacking. Other reported glitches remain unverified. | `issd_bugfix_*.c`, `main.c`; cartridge/native tests; [scope](ORIGINAL_BUG_FIXES.md) |
| Optional goalkeeper/player AI | Partial, default off. Bounded goalkeeper target tracking and formation/current-lineup/condition-aware CPU target adjustments run in live translated/interpreted routes. Human actors excluded; original physics and RNG retained. No replacement engine, universal improved AI, or difficulty certification. | `issd_gameplay.c`, `main.c`; gameplay hooks, integration, stability, tweaks tests; [details](GAMEPLAY_TWEAKS.md) |

## Presentation, audio, assets, and platforms

| Advertised capability | Status and actual behavior | Source and evidence |
| --- | --- | --- |
| 60/120/144/165/240 Hz or uncapped | Partial. Game simulation remains 60 Hz. Faster presentation repeats frames; no interpolation of simulation positions or whole rendered frames. VSync/driver/host limits can bound the requested rate. | `issd_frame_pacing.h`, `main.c`; `test_frame_pacing.py`, `test_pose_history.py` |
| Fullscreen and aspect ratios | Delivered desktop fullscreen, 4:3 CRT, square pixels, integer viewport, 320-wide authentic margin, 16:10/16:9/21:9 presets. Wider gameplay uses expanded horizontal view when enabled; verified culled handlers continue authored ROM animations without prior learning, with learned locomotion fallback; unsupported actions may hold. 4:3 is fidelity baseline. Dedicated penalty-camera scenery also widens for 16:10/16:9/21:9; the goal/players/HUD retain native scale and placement. No exclusive-fullscreen implementation. | `main.c`, `issd_widescreen.c`; widescreen native tests |
| Enhanced running animation | Delivered in source builds after v0.3.0-beta.1, default off. Saved Graphics switch adds eight crisp lower-body midpoint poses to the original eight-keyframe dash cycle in each direction. Uses loaded ROM palette indices and original OAM/depth/flips; game speed and action timing are unchanged. Verified running state only; walking, shots/tackles and keepers retain original animations. | `issd_running.c`, `issd_animation.c`; synthetic, 64 real-pose, native render isolation and replay checks; [graphics settings](GRAPHICS_SETTINGS.md) |
| Internal resolution / “HD” / “4K” | Partial. 1x–8x scales a 256x224 baseline (8x is 2048x1792, not 3840x2160). CRT, Sharp and HD replacements use larger intermediate surfaces; nearest/linear ordinary rendering keeps the native surface. Window presets and physical output dimensions are separate. Does not redraw all sprites/geometry at high definition. | `main.c`, `issd_hd.c`; native internal-resolution, render contract tests |
| Nearest, linear, Sharp, CRT | Delivered nearest/linear texture filtering, bounded integer prescale plus linear Sharp filtering, and CPU CRT effect. Current source adds CRT Strength (0/25/50/75/100%) and optional Color Boost. No adjustable shader suite, aperture-grille controls or HDR. Legacy `scanlines` is a separate darkening path at 1x only. | `main.c`; menu/internal-resolution tests |
| Sharp overlays and match readability | Delivered in current sources. Output-sized overlay after game filtering, scaled bitmap glyphs, mapped clicks, optional ball shadow, P1-P4 markers and selected names with size/collision placement, enlarged radar with position/opacity/fitting, and visual presets/sample preview/reset. Physical high-DPI/touch/cutout acceptance remains pending. | `issd_menu.c`, `issd_readability.c`, `issd_video.c`; [graphics guide](GRAPHICS_SETTINGS.md), graphics/menu/readability tests |
| Ultrawide and display recovery | Partial. 21:9 extends to a 504x224 surface within the tile ring; narrow borders remain on a true 21:9 output. Scene cuts and reused actors invalidate animation history; unsupported actions still hold. SDL reset events rebuild presentation textures. Optional graphics reports measure rendering, not simulation FPS. | `main.c`, `issd_widescreen.c`, `tests/test_display_native.py`, `tests/test_renderer_reset.py` |
| Audio and volume | Partial. Native SPC/DSP audio, linear resampling to selected SDL rate, master gain, occupancy correction and join/fade protection. No bandlimited sinc resampling, separate music/SFX mix, MSU-1 or CD soundtrack packs. Not proof of cartridge-perfect or device-perfect audio. | `main.c`, `deps/snesrecomp/runner/src/snes/dsp.c`; audio output, realtime, handshake tests; [differences](KNOWN_DIFFERENCES.md) |
| Competition/pre-match mod identities | Current sources replace verified big-name and flag sprites using native active objects and team descriptors, including cup group/next-game copies, World Series matchup/standings slots and moving coin-toss panels. Missing flag files retain original artwork. | `issd_team_visual.c`, `issd_menu.c`; team identity unit/native tests |
| Widened halftime/fulltime statistics | Current sources extend the verified stadium edge tiles for 16:10, 16:9 and 21:9. The original stats card and sprites remain centered/clipped; loading or unsupported layouts retain their fallback. | issd_widescreen.c; stats native acceptance tests |
| Widened coin-toss presentation | Current sources extend wider authored BG1 scenery sections for 16:10, 16:9 and 21:9, retaining whole close-up fans and the native vertical scroll. Native panels/action stay clipped to the original view; no new stadium artwork or gameplay. | `issd_widescreen.c`; coin native acceptance tests |
| Ordered mods | Delivered last-enabled pack wins collisions. Roster/name/stat/team data, formations, kits/stripes/away appearance, palette variants, flags, team/stadium plates, photos and supported BMP background tiles are implemented. Not arbitrary scripts, expanded game rules, or unrestricted replacement assets. | `issd_mod.c`, `issd_mod_rom.c`, `issd_mod.h`, `issd_hd.c`; mod JSON/ROM/stack and formation tests |
| Roster/stat editing | Partial. Engine storage limits still apply: 42 team slots, existing All-Stars reused for added teams, 20 roster positions, short names with restricted character repertoire, quantized stat steps. Exposed `hair_style` data is not implemented as a distinct native hairstyle renderer. | `issd_mod.c`, Mod Studio; mod ROM/JSON tests |
| Stadium and HD texture packs | Partial. Up to 32 logical entries. Legacy entries inherit layout ID & 7. Independent profiles can shorten template pitches and supply native maps, artwork, palettes and local HD sets; widths, scenery and resource budgets remain constrained. Display yards alone do not change playable boundaries. No general HD sprite or Mode 7 replacement. | [independent stadium acceptance](INDEPENDENT_STADIUM_ACCEPTANCE.md), [legacy added stadium acceptance](EXTRA_STADIUM_ACCEPTANCE.md); native/editor tests |
| Mod Studio | Delivered Python editor/import/export and previews with tests; standalone packaged Studio executable is a separate build artifact, not guaranteed by runtime packaging. | `tools/mod_studio/`; Studio tests |
| ROM/config/save roots, launcher | Delivered explicit CLI paths and persistent per-user roots, with local-config compatibility. Windows launcher and Android document selection avoid requiring writable install directories. Save context is based on effective ROM/flags, not presentation settings. | `main.c`, `issd_config.c`, `issd_save.c`, launcher/Android sources; config roots, launcher, save tests |
| Headless and capture tools | Delivered bounded headless frames, scripted input/auto-start, screenshots, frame/state/tile dumps, CLI save/load/Continue and password symbol import/export. These are diagnostics and automation, not bots capable of certifying complete tournaments. Screenshot scale is capture output sizing. | `main.c`, input script code; headless, password flow, replay and capture tests |
| Windows | Build/runtime/headless tests exercised here. Physical controller, display/audio and full untouched campaign acceptance are separate pending checks. | Windows build scripts and acceptance record |
| Linux / Steam Deck | Linux build/headless smoke previously recorded; Steam Deck first-run defaults are implemented. SteamOS SDK/device session and complete physical controls/performance acceptance are not established by Windows tests. | `issd_config.c`, Linux/Steam Deck build docs |
| Android | APK/platform integration, landscape UI and document selection implemented. Background events pause simulation/audio, clear held inputs and open the overlay; foreground requires explicit Resume. Device behavior remains unverified; desktop tests cannot certify activity recreation, audio routing or OS process death. | `android/`, `main.c`; `test_host_input.py`; [device procedure](ACCEPTANCE_TESTS.md) |
| Netplay / rollback, interpolation, replacement soundtrack | Planned; no delivered runtime path verified. Local snapshots and timing infrastructure do not constitute these features. | Older enhancement roadmap only |

## Persisted setting inventory

Every field advertised in `IssdConfig` is accounted for below. Configuration
parsing/persistence alone is not evidence that a setting affects gameplay.

| Keys / fields | Status / consumer |
| --- | --- |
| `window_width`, `window_height` | Delivered at desktop window creation; not an in-game resolution guarantee. |
| `fullscreen` | Delivered SDL desktop fullscreen, including shortcut. |
| `vsync` | Delivered renderer creation/live application; supported backend may reject a live change. Verify on physical display. |
| `target_fps` | Delivered presentation scheduling only; simulation 60 Hz. |
| `aspect_ratio`, `true_widescreen` | Delivered viewport and horizontal FOV with culling limits above. |
| `internal_res`, `scaling_filter` | Delivered intermediate/filter selection; limits above. |
| `integer_scaling` | Delivered independent integer-height viewport scaling retaining chosen aspect; small outputs fit fractionally. Legacy integer aspect remains supported. |
| `output_resolution` | Delivered desktop window presets 0-4. Fullscreen/Android use actual display dimensions. |
| `overlay_scale` | Delivered sharp output overlay scaling Auto/1x-4x, bounded to fit; main/Graphics pages scroll. |
| `color_boost`, `crt_strength` | Current-source cosmetic controls after the published beta. Color Boost defaults off; CRT Strength defaults 100% and supports 0/25/50/75/100%. |
| `ball_outline` | Retired. Old files load/save it disabled; no drawing or menu toggle. |
| `ball_shadow`, `player_markers`, `player_names`, `radar_scale` | Delivered optional live-match read-only presentation. Defaults off/1x. Selected human names only; enlarged radar fits the surface. |
| `hud_scale`, `radar_position`, `radar_opacity` | Delivered added-label/marker scale 1x–3x, five enlarged-map positions and 25%–100% map background opacity. Original scoreboard stays unchanged; original radar remains visible when the enlarged map moves. |
| Visual Preset / Preview / Reset Graphics | Delivered Original/Sharp/Enhanced presentation presets with derived Custom state, illustrative filtered sample and reset. Preserves gameplay, controls, mods and fullscreen/VSync. |
| `scanlines` | Delivered only at 1x; not a configurable CRT shader. |
| `audio_freq` | Delivered requested SDL output frequency at initialization; device may negotiate another rate. |
| `master_volume` | Delivered runtime gain. |
| `music_volume`, `sfx_volume` | **Inactive**; no independent music/SFX consumer. |
| `engine_mode` | **Inactive legacy enum**; one baseline engine. Menu identifies it as fixed. Old Classic/Enhanced comments and startup descriptions do not demonstrate separate timing/slowdown implementations. |
| `skip_intro`, `fast_menus` | **Inactive**; no runtime consumers. |
| `debug_unhooked_code` | Delivered experimental ROM/debug patch path; affects save/password compatibility, not a fidelity guarantee. |
| `gameplay_bug_fixes` | Delivered single switch for all verified fixes in the bundle, default off; part of save/password compatibility. |
| `gameplay_goalkeeper_ai`, `gameplay_player_ai` | Delivered optional policies, default off, part of save/password context. |
| `match_preset`, `match_duration`, `match_difficulty`, `match_offside`, `match_fouls`, `match_cards`, `match_extra_time` | Delivered exhibition initialization only. Duration 0–2 and difficulty 0–4; binary switches use 0 on / 1 off. Selection does not alter a favorite's recorded rules or an ongoing match. |
| `active_mod_packs`, `hd_texture_packs` | Delivered ordered pack lists; old singular keys are import compatibility aliases. |
| `rom_path`, `mods_dir` | Delivered selection/loading roots. |
| `key_p1_up/down/left/right/a/b/x/y/l/r/start/select` | Delivered configured P1 SDL scancodes and native keyboard editor. |
| `player_profiles[0..3].schema`, `.bindings`, `.stick_deadzone`, `.trigger_deadzone` | Delivered player-indexed physical input presets/custom source masks and thresholds. Assignment follows connected slot, not permanent device identity. |
| `touch_x[]`, `touch_y[]`, `touch_size[]` | Delivered normalized control centers and 50–200% sizes; automatic center sentinel -1. |

## Corrections to older claims

`ENHANCEMENTS.md` is a mixture of implementation ideas and roadmap. Its claims
of interpolated frames, eliminated slowdown through Enhanced mode, independent
audio channels, sinc resampling, soundtrack packs and exclusive fullscreen must
be read as plans unless specifically supported above. `KNOWN_DIFFERENCES.md`
cannot establish 100% fidelity, sub-millisecond input latency or full cycle
accuracy; these would require measurements not supplied by the current suite.
Earlier `MODDING.md` statements that flags, striped kits and plates cannot be
modified are superseded by implemented ROM/host overrides; conversely visual
stadium dimensions do not prove playable field boundaries were changed.
The old `test_full_suite.py` directory/screenshot smoke script is not evidence
of 100% game coverage. A build, boot, or passing unit test is not a completed
campaign or physical hardware acceptance.
