# Supported features and settings

This is the source-based capability audit of current work after 0.2.0-beta.1,
updated October 1, 2026. New control remapping, touch layout and lifecycle changes
describe current sources, not the already published beta binary. It takes
precedence over aspirational descriptions in older enhancement,
modding, and fidelity documents. **Delivered** means there is a runtime consumer;
it does not mean every cartridge scene or device has been certified. **Partial**
means an implemented feature has material limits. **Inactive** means a saved
setting has no corresponding runtime behavior. **Planned** means no delivered
implementation was found. See [acceptance checks](ACCEPTANCE_TESTS.md) for what
has actually been tested and what still needs hardware or complete playthroughs.

## Game, persistence, and input

| Advertised capability | Status and actual behavior | Source and evidence |
| --- | --- | --- |
| Native ISS Deluxe runtime | Partial. Original cartridge code runs through translated routines, interpreter fallback, native hardware models, and HLE replacements. It is not a cycle-accurate SNES emulator or a proven complete recreation. Retail USA, headerless 2 MiB ROM is the supported baseline; ROM is not distributed. | `ISSDNative/main.c`, `recomp/generated/`, `deps/snesrecomp/runner/src/`; `test_boot_reaches_main_loop.py`, `test_headless_match.py`, runtime interrupt tests |
| Original game modes, Cup and World Series | Partial. Native menu and gameplay paths run, including original committed results and terminal tables. A fresh accelerated nine-match Cup completes native progression and Continue after every result; World terminal fixtures prove specific transitions. Other scenarios, difficulties, teams, full World Series and untouched tournaments remain unverified. Training, scenarios, match penalties and substitution menu still require dedicated end-to-end acceptance. | `test_campaign_full_acceptance.py`, `test_campaign_transitions.py`, `test_game_acceptance.py`; [cartridge trace](CAMPAIGN_CARTRIDGE_FLOW.md) |
| Continue and campaign autosave | Delivered for verified Cup/World setup, results, completion, and original password import checkpoints. Saves do not occur continuously during a match, replay, halftime, or ceremony. A failed write leaves earlier committed saves intact; the observed transition is consumed, so retry occurs at a later verified checkpoint rather than every frame. | `issd_campaign.c`, `issd_save.c`, main integration; campaign save/transition tests |
| Backup recovery and compatibility | Delivered. Current plus two backup generations, integrity checks and pure snapshot validation before publication/load; ROM/mod/debug/gameplay context must match. Successful Continue restores a snapshot, including between-match campaign data. Incompatible saves are preserved and explained, not converted. | `issd_save.c`, `issd_snapshot.c`; `test_campaign_saves.py`, `test_snapshot_transactional.py` |
| Manual saves | Delivered. Quicksave and eight numbered slots; validated envelope and transactional snapshot load. Legacy raw versions 4–8 require explicit approval (`--allow-legacy-save` for CLI), and lack modern context guarantees. | `issd_save.c`, `main.c`, `issd_menu.c`; save, transactional, replay/video tests |
| Original password bridge | Delivered for six verified retail formats. Export uses original cartridge encoding in private memory. Import validates then queues symbols to the original Password screen; the guest applies the campaign. Modified ROM or active gameplay/debug compatibility flags disable the bridge. It is not an arbitrary snapshot-to-password converter. | `issd_password.c`, `issd_password_ui.c`; codec, flow, UI tests |
| Password editor | Delivered. Controller, touch and keyboard; 64-symbol palette, 60-symbol capacity, delete/import/export/back, visible errors and named special icons. Spaces/newlines ignored; case preserved. Keyboard cannot directly type every custom icon: use the palette. | `issd_password_ui.c`; `test_password_ui.py` |
| Four local players | Delivered transport and assignment. Four SDL gamepads by connection order, keyboard/touch additive on P1, stable remaining assignments on disconnect, vacant slot reused on reconnect, fifth pad ignored. Original game mode still determines how many humans play. No network multiplayer. | `issd_input.c`, SNES joypad model, `main.c`; `test_local_multiplayer.py` uses SDL virtual pads and original guest serial reads |
| Control profiles and remapping | Delivered in current sources: per-player Classic/FIFA/PES/custom profiles, button/axis/trigger binding and thresholds; P1 keyboard remapping and persisted configuration. These are mappings of cartridge actions, not FIFA/PES game mechanics. Hardware ergonomics and four physical pads require acceptance. | `issd_config.c`, `issd_input.c`, `issd_controls.c`, `issd_menu.c`; `test_control_profiles.py`, config persistence tests |
| Touch controls | Delivered hit-testing, multi-touch, hide/menu behavior and configurable position/size per control. Touch feeds P1. Menu control stays reachable while the pad is hidden; changing layout clears held touches. Physical touch/device layout acceptance remains pending. | `issd_touch.c`, `issd_menu.c`, main renderer; touch and layout tests |
| Native overlay | Delivered pause/resume, controls, mods, presentation, save/load, Continue, password, gameplay switches, restart and quit; Android also exposes ROM/mod selection. Original in-game menus remain separate. | `issd_menu.c`, `main.c`; native menu tests |
| Optional goalkeeper/player AI | Partial, default off. Bounded goalkeeper target tracking and formation/current-lineup/condition-aware CPU target adjustments run in live translated/interpreted routes. Human actors excluded; original physics and RNG retained. No replacement engine, universal improved AI, or difficulty certification. | `issd_gameplay.c`, `main.c`; gameplay hooks, integration, stability, tweaks tests; [details](GAMEPLAY_TWEAKS.md) |

## Presentation, audio, assets, and platforms

| Advertised capability | Status and actual behavior | Source and evidence |
| --- | --- | --- |
| 60/120/144/165/240 Hz or uncapped | Partial. Game simulation remains 60 Hz. Faster presentation repeats frames; no pose/frame interpolation. VSync/driver/host limits can bound the requested rate. | `issd_frame_pacing.h`, `main.c`; `test_frame_pacing.py`, `test_pose_history.py` |
| Fullscreen and aspect ratios | Delivered desktop fullscreen, 4:3 CRT, square pixels, integer viewport, 320-wide authentic margin, 16:10/16:9/21:9 presets. Wider gameplay uses expanded horizontal view when enabled; original culling freezes outer-band players beyond the authentic margin. 4:3 is fidelity baseline. No exclusive-fullscreen implementation. | `main.c`, `issd_widescreen.c`; widescreen native tests |
| Internal resolution / “HD” / “4K” | Partial. 1x–8x scales a 256x224 baseline (8x is 2048x1792, not 3840x2160). CRT processing and replacement textures can use a larger intermediate surface; nearest/linear ordinary rendering may keep the native surface. Does not redraw all sprites/geometry at high definition. | `main.c`, `issd_hd.c`; native internal-resolution, render contract tests |
| Nearest, linear, CRT | Delivered nearest/linear texture filtering and CPU CRT effect. No adjustable shader suite, aperture-grille controls, HDR, or CRT strength slider. Legacy `scanlines` is a separate darkening path at 1x only. | `main.c`; menu/internal-resolution tests |
| Audio and volume | Partial. Native SPC/DSP audio, linear resampling to selected SDL rate, master gain, occupancy correction and join/fade protection. No bandlimited sinc resampling, separate music/SFX mix, MSU-1 or CD soundtrack packs. Not proof of cartridge-perfect or device-perfect audio. | `main.c`, `deps/snesrecomp/runner/src/snes/dsp.c`; audio output, realtime, handshake tests; [differences](KNOWN_DIFFERENCES.md) |
| Ordered mods | Delivered last-enabled pack wins collisions. Roster/name/stat/team data, formations, kits/stripes/away appearance, palette variants, flags, team/stadium plates, photos and supported BMP background tiles are implemented. Not arbitrary scripts, expanded game rules, or unrestricted replacement assets. | `issd_mod.c`, `issd_mod_rom.c`, `issd_mod.h`, `issd_hd.c`; mod JSON/ROM/stack and formation tests |
| Roster/stat editing | Partial. Engine storage limits still apply: 42 team slots, existing All-Stars reused for added teams, 20 roster positions, short names with restricted character repertoire, quantized stat steps. Exposed `hair_style` data is not implemented as a distinct native hairstyle renderer. | `issd_mod.c`, Mod Studio; mod ROM/JSON tests |
| Stadium and HD texture packs | Partial. Visual stadium dimensions/appearance and replacement backgrounds do not establish changed playable pitch boundaries. HD replacement is for supported background tiles; no general HD sprite or Mode 7 replacement or automatic wide-margin asset creation. | `issd_mod.c`, `issd_hd.c`; HD/mod tests |
| Mod Studio | Delivered Python editor/import/export and previews with tests; standalone packaged Studio executable is a separate build artifact, not guaranteed by runtime packaging. | `tools/mod_studio/`; Studio tests |
| ROM/config/save roots, launcher | Delivered explicit CLI paths and persistent per-user roots, with local-config compatibility. Windows launcher and Android document selection avoid requiring writable install directories. Save context is based on effective ROM/flags, not presentation settings. | `main.c`, `issd_config.c`, `issd_save.c`, launcher/Android sources; config roots, launcher, save tests |
| Headless and capture tools | Delivered bounded headless frames, scripted input/auto-start, screenshots, frame/state/tile dumps, CLI save/load/Continue and password symbol import/export. These are diagnostics and automation, not bots capable of certifying complete tournaments. Screenshot scale is capture output sizing. | `main.c`, input script code; headless, password flow, replay and capture tests |
| Windows | Build/runtime/headless tests exercised here. Physical controller, display/audio and full untouched campaign acceptance are separate pending checks. | Windows build scripts and acceptance record |
| Linux / Steam Deck | Linux build/headless smoke previously recorded; Steam Deck first-run defaults are implemented. SteamOS SDK/device session and complete physical controls/performance acceptance are not established by Windows tests. | `issd_config.c`, Linux/Steam Deck build docs |
| Android | APK/platform integration, landscape UI and document selection implemented. Background events pause simulation/audio, clear held inputs and open the overlay; foreground requires explicit Resume. Device behavior remains unverified; desktop tests cannot certify activity recreation, audio routing or OS process death. | `android/`, `main.c`; `test_host_input.py`; [device procedure](ACCEPTANCE_TESTS.md) |
| Netplay / rollback, interpolation, replacement soundtrack, instant rematch | Planned; no delivered runtime path verified. Local snapshots and timing infrastructure do not constitute these features. | Older enhancement roadmap only |

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
| `integer_scaling` | **Inactive legacy boolean**. Choose integer `aspect_ratio` to activate integer viewport scaling. |
| `scanlines` | Delivered only at 1x; not a configurable CRT shader. |
| `audio_freq` | Delivered requested SDL output frequency at initialization; device may negotiate another rate. |
| `master_volume` | Delivered runtime gain. |
| `music_volume`, `sfx_volume` | **Inactive**; no independent music/SFX consumer. |
| `engine_mode` | **Inactive legacy enum**; one baseline engine. Menu identifies it as fixed. Old Classic/Enhanced comments and startup descriptions do not demonstrate separate timing/slowdown implementations. |
| `skip_intro`, `fast_menus` | **Inactive**; no runtime consumers. |
| `debug_unhooked_code` | Delivered experimental ROM/debug patch path; affects save/password compatibility, not a fidelity guarantee. |
| `gameplay_goalkeeper_ai`, `gameplay_player_ai` | Delivered optional policies, default off, part of save/password context. |
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
