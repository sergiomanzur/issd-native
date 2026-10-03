# Game and device acceptance

This checklist separates source/unit coverage, accelerated retail-ROM execution,
and actual playtesting. It is not a claim that every item passed. Record build
commit, platform/device, ROM hash, configuration, enabled packs, input devices,
duration and artifacts for each run. Baseline uses the headerless USA ROM,
4:3, gameplay/debug tweaks off, no mods and an isolated save root. Re-run relevant
checks with each supported tweak/mod combination separately.

## Automated checks

Build the current native executable first. Run `python -m pytest tests -q` from
the repository; do not collect vendored dependencies. ROM tests skip when the
user's cartridge image or native build is absent. A skipped test is **not a
pass**. Tests use owned temporary config/save directories, not personal saves.

Focused game persistence command:

```powershell
python -m pytest tests/test_campaign_full_acceptance.py tests/test_game_acceptance.py tests/test_campaign_transitions.py tests/test_password_flow.py tests/test_campaign_saves.py tests/test_snapshot_transactional.py -q
```

Focused controls, replay and policies:

```powershell
python -m pytest tests/test_local_multiplayer.py tests/test_control_profiles.py tests/test_touch.py tests/test_config_persistence.py tests/test_savestate_replay.py tests/test_gameplay_integration.py tests/test_gameplay_tweaks.py -q
```

| Check | What execution proves | What it does not prove |
| --- | --- | --- |
| Campaign setup/result tests | Actual cartridge setup, halftime exclusion, fulltime result commit, one checkpoint, Continue preserving campaign WRAM and no immediate duplicate write. | Winning a full match with ordinary input, every team/mode. |
| Existing Cup phase/final fixture | Original qualification, group/knockout constructors, semifinal/final commit, champion, ceremony exclusion, final table save/Continue. Test privately seeds prior stage/counters/entrants and winning score. | Playing an untouched tournament from round one. |
| Existing World terminal fixture | Original result increments the privately seeded previous counter 34 to 35; final table, one completion checkpoint and Continue. | Playing all 35 games or validating every intermediate schedule. |
| `test_game_acceptance.py` | Original Password-screen submission of cartridge-generated semifinal password, original final match selection, original halftime/fulltime, champion/ceremony/table commit, completion checkpoint and Continue; a drawn fixture reaches original extra-time periods 2 and 3. Only test-owned clocks and scores are accelerated; no callback/period/stage/champion writes. | Full tournament, skill-driven win, naturally elapsed match clock or completed shootout. The password fixture itself originates in an earlier accelerated campaign trace. |
| `test_campaign_full_acceptance.py` Cup | Fresh native Cup setup and all nine scheduled matches through qualification, groups, knockouts, champion/ceremony/final table; new-process Continue after every committed result and completion. Only clocks/scores accelerated, with no seeded stages, entrants, opponents, periods or callbacks. | Normal elapsed clock, human winning play, alternate teams/difficulties or full World Series. World test is opt-in and unexecuted. |
| Password flow/codec | Six retail formats, original encoder agreement, invalid input rejection and original guest import; no direct decoded campaign copy. | Modified-ROM password support or every historical password ever generated. |
| Save storage/transaction tests | Context/integrity validation, fallback backup selection, failed publication/load protection and snapshot rollback. | Real device power loss during storage flush or every filesystem behavior. |
| Local multiplayer | Four SDL virtual controllers, mapping/slot/disconnect/reconnect behavior and original SNES serial reads. | Four physical controllers, Bluetooth reconnect latency, whole 2v2 match. |
| Gameplay policy tests | Optional hooks run, bounded target changes, replay equality, current-lineup/condition semantics including seeded substituted lineup. | Original substitution menu execution or every real substitution during match/extra time. |
| Original team-change inputs | First-team original formation and bench substitution commits, actor refresh, SELECT request cancellation and deterministic advancing save/reload with AI off/on. | Both-team menus, halftime/extra-time changes, original replacement limits, and the substituted human-team actor's optional AI decision. See [team-change checks](TEAM_CHANGES_ACCEPTANCE.md). |
| Touch/config tests | Geometry, multi-touch, persistence, mapping, threshold, edit/reset semantics. | Android touch ergonomics, actual host focus or OS lifecycle. |
| Exhibition shortcuts | Original-menu preconstructor witness, all four preset clocks/CPU levels, exact full-WRAM rematch/drill replay, favorite process restart and changed-AI rejection. Storage faults preserve prior favorite; UI/model regressions cover unsafe scenes, health and recapture rollback. | Every kit/weather/roster/controller combination, physical ergonomics or a certified goalkeeper/shooting balance patch. |

Each guest launch is bounded to 120 seconds; phase fixtures run a bounded number
of guest frames. Assertions fail on interpreter caps and wrong native callback,
period or campaign bytes. Acceleration only modifies temporary owned snapshots
and rehashes their envelopes. Such edits are never a supported user save API.
Retain pytest output and capture artifacts when failures occur.

## Required untouched tournament acceptance — not executed here

1. Start a fresh Cup through original menus. Choose a documented team, settings
   and difficulty. Play every scheduled match without clock/score/stage edits,
   cheat tooling, prepared late-stage snapshots or passwords. Record initial
   qualifying stage, group progression, all knockout entrants and final result.
2. At each committed result, return to the original between-match page, quit the
   application completely and start it again using Continue. Verify team,
   opponent, bracket/table, settings and next match before proceeding. Check
   replay/halftime/ceremony do not replace that checkpoint.
3. Win the final through normal input. Verify actual champion/ceremony and final
   bracket, then restart with Continue. Completion page and champion must match;
   no fresh or duplicate completion checkpoint should appear merely by loading.
4. Repeat for a fresh World Series through all 35 scheduled results and the final
   table. Verify every opponent/round and standings after save/reload. Test an
   early exit/elimination and starting a new same-kind campaign as separate runs.
5. Copy one owned test campaign's current envelope and two backups before
   deliberately damaging only that owned current file. Confirm the latest valid
   backup recovers, original damage is not silently overwritten, and the UI
   explains recovery. Restore the test files afterwards.

Pass requires screenshots/logs at each phase, the final completion page, owned
save generations and recorded restart observations. Existing accelerated tests
cover terminal native transitions but **do not satisfy this untouched check**.

## Extra time and penalties — manual game-flow acceptance pending

Use an actual Cup knockout/final reached normally or through a recorded original
password. Set the original available overtime rules in the game setup. Play a
draw through regulation with ordinary input; do not seed the period, timer,
score, task callbacks or campaign fields for this acceptance run.

1. Confirm tied regulation moves into the original extra-time period rather than
   committing a draw/result or creating a new campaign checkpoint prematurely.
2. Play both extra-time halves. Record period transitions, clock behavior,
   side change, pause/resume, substitutions and audio continuity.
3. Maintain the draw to the original shootout. Complete the initial kicks and,
   in a separate run, sudden death. Check shooter/keeper ownership, turn order,
   tally, winner, and original bracket progression. Test a winning extra-time
   goal under each supported original overtime rule separately.
4. Quit after the committed result, Continue and verify the next opponent/table
   and shootout winner. A snapshot taken during extra time/shootout should also
   resume its exact period/turn using manual load, with no stuck input.

The disassembly's original period counter is WRAM `$00A8`; this is a diagnostic,
not an instruction to alter it. Merely setting `$00A8`, choosing the standalone
Penalty Kick game, or seeing its graphics does not establish that a drawn
knockout transitions to a shootout. No completed native draw-to-shootout run is
claimed in this audit.

## Substitutions and controller disconnect — physical acceptance pending

1. Start an actual match. Open the original substitution menu through its normal
   pause flow, replace a starter with a bench player and confirm. Verify names,
   numbers, formation role and on-field actor change. Repeat for both sides and
   at halftime/extra time; verify limits and cancel behavior remain original.
2. With the player AI tweak enabled, check CPU decisions use the replacement's
   actual lineup/condition. Save/load after a substitution and verify team data,
   changed player and next substitution opportunity. Policy unit tests with a
   seeded lineup are useful but do not satisfy step 1.
3. Connect four physical SDL-recognized controllers; play an original supported
   four-human match. Hold a direction/button on P2 and unplug it. No input may
   remain stuck; P1/P3/P4 assignments must stay stable. P1 keyboard/touch must
   remain usable. Repeat while original pause and native overlay are open.
4. Reconnect and verify the first vacant slot; connect a fifth device and confirm
   it does not replace a player. Check Guide/Home and Back+Start menu behavior
   does not generate an unintended held game button. Verify custom bindings,
   independent profiles, deadzones, triggers and persistence after restart.
5. On a touch device, exercise held direction plus two buttons, hide/unhide,
   resized/moved controls, and layout reset. No stale direction/menu edge after
   editing, focus loss or suspend. Record device-specific collisions/ergonomics.

## Android suspend/resume and process death — device check not executed

No Android device was attached when queried in this audit. APK builds and
desktop source tests cannot replace the following checks. Use Android 8+ device,
the correct ABI build, and record device/OS/audio route. The package is
`com.issdnative`, activity `.ISSDActivity`. ADB must be installed separately.

```sh
adb devices -l
adb install -r path/to/app-debug.apk
adb shell am start -n com.issdnative/.ISSDActivity
adb logcat -c
adb logcat -v threadtime > android-acceptance.log
```

1. Select the owned ROM using the document picker, start an actual match and
   hold touch/physical input. Press Home:
   `adb shell input keyevent KEYCODE_HOME`. Leave backgrounded for 5, 30 and 120
   seconds on separate runs. Return with the activity command above and choose
   Resume in the overlay (foreground deliberately stays paused). Game time
   must not fast-forward to compensate for background time, held input must be
   cleared, audio must resume without a stuck backlog, and gameplay must remain
   responsive. Compare recorded clock/frame observations before/after.
2. Repeat at original pause, native menu, password editor, between-match Cup and
   World results, extra time and shootout. Lock/unlock the screen, switch audio
   route and unplug/reconnect a controller while backgrounded. Confirm the menu
   and control layout remain reachable. Exercise landscape rotation/window
   changes on a compatible device.
3. At a verified campaign checkpoint, go Home and execute
   `adb shell am force-stop com.issdnative`, then relaunch. Continue must recover
   the last **committed** campaign; this is not a promise of a mid-match automatic
   snapshot. Verify app-private saves and config survive restart.
4. Enable developer “Don't keep activities” and repeat background/resume to test
   activity recreation separately from a normal pause. Test low-memory process
   termination with appropriate device tooling. Record whether the process was
   recreated and how the user reaches the last committed save.
5. On a debug build, inspect test-owned app files when `run-as` is supported:
   `adb shell run-as com.issdnative ls -R files`. Retail builds may reject
   `run-as`; inability to read private files externally is not a save failure.

Capture `adb logcat` and screenshots around every transition. Fail on crash,
ANR, lost checkpoint, stuck input, fast-forward, runaway audio or unusable menu.
No lifecycle hardware result should be marked passed without an actual run.

## Release acceptance record

October 1, 2026, Windows current development build (`build/ISSDNative.exe`),
retail USA ROM, dummy SDL audio/video, isolated default config/no packs:
`python -m pytest tests/test_game_acceptance.py -q` completed **2 passed in
93.05 seconds**. This proves the two accelerated flows described above, including
no campaign checkpoint during drawn periods. The suite is not a certification of
the published beta binary or of physical Windows/Android behavior. A release
still needs hardware acceptance against its exact release build.

| Area | Current evidence | Remaining acceptance |
| --- | --- | --- |
| Windows native saved campaign/password paths | Automated retail-ROM transitions and focused storage/codec/UI tests. Final current-build pytest output must accompany a release. | Untouched Cup/World, physical pads/audio/display and extended play. |
| Cup/World terminal state | Full accelerated nine-match fresh Cup plus original World terminal fixture, including completion-save/Continue. | Full untouched Cup, full 35-match World, and every elimination/restart variant. |
| Extra time / shootout / original substitutions | Accelerated original transition into both extra-time halves; no completed shootout or original substitution-menu flow certified here. | Normal elapsed draw, complete shootout and manual game-flow checks above. |
| Disconnect | SDL virtual pad transport tests. | Four physical pads across match/pause/background/reconnect. |
| Android | Source/APK integration and portable touch tests. | All lifecycle checks on real device; not executed here. |
| Linux / Steam Deck | Current Linux build succeeds; source first-run defaults. | SteamOS SDK package and real handheld session. |

Keep outstanding items visible in release notes. Replace “pending” with a dated,
linked execution record only after collecting evidence for that exact check.

Exhibition shortcut tests use explicit original-menu input, not generic auto-start
mashing. The pre-constructor boundary was witnessed at frame 1897 and kickoff
at frame 3004 in the isolated default setup. Live match identity comes from
the original backup at `$DE07`, because `$1648` is reused for player data.
See [match shortcuts](MATCH_SHORTCUTS.md) for the balance reproduction protocol.

Final integration on October 1: the project suite returned **174 passed, 2 failed, 1 skipped, 7 subtests passed** in 588.66 seconds. The two existing failures concern an appearance-nibble expectation and shipped-pack kit-sharing warnings. After the last host/menu input fixes, the focused suite returned **30 passed** in 125.74 seconds against the final Windows executable. Windows, Linux, and Android arm64-v8a/x86_64 compiled successfully, and the three new overlay pages were visually inspected. No Android device was attached for ADB acceptance; no physical-device result is implied by these checks.

Fresh-start Cup acceptance also completed **1 passed in 266.03 seconds** against the final Windows build: all nine native matches, stages 0 through 9, championship ceremony/table, and Continue after each result and final completion. This test was added after the 174-pass full-suite run and verified separately. Its World Series variant is skipped by default and has not been executed; opt in with ISSD_RUN_FULL_WORLD=1 only for a dedicated 35-match run. The accelerated Cup pass does not replace the untouched or physical-device checklist above.

Current exhibition integration verification (October 1): `python -m pytest tests -q --tb=short` returned **204 passed, 2 failed, 2 skipped, 7 subtests passed** in **890.47 seconds**. Both failures are the existing appearance-nibble expectation and shipped-pack kit-sharing warnings. All eight original-exhibition preset/shortcut cases and the fresh Cup passed. After the final paused-page error display change, the model/menu/mod-stack checks returned **3 passed**; a final Windows binary smoke independently saved/loaded a favorite across processes and retained Casual rules despite Classic selection. Final Windows, Linux and both Android ABI builds succeeded; menu pages were visually inspected. Physical controller/touch ergonomics and device acceptance remain unexecuted.

Original-game bug-fix verification (October 1): the master switch covers six
verified corrections, with the exact scope in [the bug-fix guide](ORIGINAL_BUG_FIXES.md).
The final Windows artifact passed **16 focused tests in 63.86 seconds**, including
actual cartridge opcodes, unchanged generated goal/restart routines, constructed
native multiplayer ownership fixtures, full WRAM replay, context rejection and
AI stacking. Windows, Linux and Android arm64-v8a/x86_64 builds succeeded.
Both toggle states were visually inspected in the rendered overlay. Native
fixtures establish the corrected mechanisms, not exhaustive natural playtests;
stuck-post, substitution-camera and penalty-replay reports remain unverified.

The final bug-fix integration suite, `python -m pytest tests -q --tb=short`,
returned **217 passed, 2 failed, 3 skipped, 7 subtests passed** in **970.62 seconds**.
The two failures remain `test_mod_rom_patching` (appearance-nibble expectation)
and `test_shipped_packs_validate` (existing shared-kit warnings). All new bug-fix
checks passed. The standard Windows executable was updated to the focused-tested
final artifact after the suite completed. Version/release publishing is unchanged.

The additional boot-replay skip was checked separately: the previous exhibition
artifact and final bug-fix executable produced identical 240-frame guest WRAM
(SHA-256 `c1d25fd82995526afb23cf5da82639ffa44ea7d24aa084baa18432e2da0f5abb`)
and each rendered 42 colors. `test_loaded_state_resumes_identically` then passed
in **4.32 seconds** on its own. The blank screenshot observed during the full
run remains an intermittent acceptance limitation; no speculative runtime change
was made. Final Windows executable SHA-256:
`6418398c5d334489302687064ceeb39a9ceec032064fe6d6daf542a9110c4f80`.

Graphics/readability verification (October 1): the integration suite returned
**222 passed, 2 failed, 2 skipped, 7 subtests passed** in **934.13 seconds**.
The failures remain the existing `test_mod_rom_patching` appearance-nibble
expectation and `test_shipped_packs_validate` kit-sharing warnings. Final
focused verification against the standard Windows executable returned
**10 passed in 32.00 seconds**, after safe-area, retained-screenshot-stride and
culled-player radar corrections. It covers config persistence, viewport fitting,
sharp prescaling, overlay rendering/clicks/scrolling/notifications, clipping,
descriptor animation, complete PPU transaction restoration and native rendering.

The separate real-ROM viewport regression passed original, Authentic 320,
16:10, 16:9 and 21:9 at frame 1200: all five produced identical guest WRAM,
and widened images retained the exact original center pixels. The native
readability comparison reached scene 6 / live submode 8 with toggles off/on,
changed the resulting pixels, preserved guest WRAM and left the top HUD band
unchanged. Unit coverage additionally retains radar players whose screen pose
was culled. These checks do not certify every offscreen action or a full match.

Real menu harness images were inspected at 1280x720 and 1920x1080, including
main/Graphics/control pages and a 400x120 short layout. Extreme-inset 2x2
surfaces were checked with buffer guards. Android safe insets use a UI-thread
snapshot of cutout/visible-bar bounds minus space already consumed by SDL's
surface; native mouse/touch mapping and notification placement share the same
safe rectangle. Windows, Linux and both Android ABI builds succeeded; physical
display/touch/cutout acceptance remains pending.

Final graphics Windows executable SHA-256:
`765645fec6de6d5594361acdb345baaa682b1aa92b4292137958ddbf984dd487`.

### Graphics follow-up acceptance

Current sources add visual presets/preview/reset, selected-label scaling,
radar position/opacity, renderer-reset recovery and a 504x224 ultrawide view.
The five real-ROM viewport comparisons passed at frames 600 and 1200, retaining
identical guest WRAM and exact native-center pixels. Native fixtures cover all
eight tile scroll phases, extreme supplemental sprite coordinates, immediate
stats/pitch/submode cuts, cold descriptors, actor-slot reuse and transaction reset.
These fixtures establish rendering mechanisms, not complete natural-match coverage.

SDL software-renderer acceptance passed four output configurations: 320x240
Nearest, 1280x720 Sharp/integer/16:9, 1920x1080 Sharp/ultrawide and 3840x2160
Sharp/4:3. Each rendered a 60-frame cartridge replay and wrote actual dimensions
and timing metrics. The renderer-reset harness compiles the actual host event
guard and cleanup block, testing both SDL reset events and all texture caches;
it does not emulate a physical GPU context loss. Preview images were inspected
at 400x224 and 1920x1080. The native readability comparison and label/radar guards
retain unchanged guest WRAM and the top native HUD band.

Software-renderer reports are retained under `build/graphics-next-focused`.
They ran concurrently with integration tests and are not hardware benchmarks:
CPU presentation means were approximately 0.34, 3.58, 6.80 and 19.31 ms;
total presentation means were 0.57, 5.58, 19.82 and 75.18 ms respectively.
Use `--graphics-report report.json` on the target device to obtain its own
measurements; VSync waiting can be included in the total.

A separate 1200-frame graphical SDL software replay reached live scene 6 /
submode 8 with Sharp, ball effects, 2x labels and a moved/translucent radar.
It produced 1202 presentations at 1280x720 with a 398x224 native view and
1592x896 intermediate. CPU presentation mean was 3.62 ms and total mean 10.87 ms;
the 64.45 ms maximum under concurrent load prevents treating this as a stable
60-FPS hardware acceptance claim. Artifacts: `build/graphics-next-live`.

Physical acceptance remains unexecuted. No ADB device was attached. Record
device, OS, display scale, output size, preset and report alongside each result:

- Desktop: resize with the menu open, switch windowed/fullscreen, move between
  monitors at 100%/125%/150%/200% DPI, and confirm sharp text and matching clicks.
- Presets: exercise Original, Sharp, Enhanced, Custom and Reset; restart and
  confirm persistence without changing gameplay/controls/mods/fullscreen/VSync.
- Match: play widened views through both camera ends, goals/replays, substitutions,
  half time, extra time and penalties. Confirm no stale actors, clipped labels
  or corrupted stadium margins; unlearned offscreen actions can still hold.
- Android: check cutouts and system bars in both landscape orientations, all menu
  taps, touch controls and a background/resume cycle. Resume stays paused until
  explicitly selected; confirm textures return if the graphics context is lost.
- Performance: capture a live-match timing report with Sharp/readability enabled
  on each target device; check responsiveness and pacing rather than assuming
  software-renderer results transfer to a hardware renderer.

The follow-up full integration suite returned **227 passed, 2 failed, 2 skipped,
7 subtests passed** in **971.41 seconds**. The failures remain
`test_mod_rom.py::test_mod_rom_patching` (existing appearance-nibble expectation)
and `test_mods_valid.py::test_shipped_packs_validate` (shared-kit warnings).
The final capture fix was verified separately: HD composition uses the captured
frame's margin instead of a newly selected aspect while paused. Windows, Linux
and Android arm64-v8a/x86_64 builds succeeded with the final source changes.
Final Windows executable SHA-256:
`f0d70842712cf807a39b84f0fffa507528ac31a3acb2e0c6ef202d6391334c9e`.
Focused verification against that final executable returned **17 passed in
68.07 seconds**, including the live graphical replay and paused-capture margin
regression. Final log: `build/graphics-next-final-focused.log`.


## Offscreen ROM animation and match graphics follow-up (2026-10-01)

The renderer now decodes the authored bank-$82 animation direction, frame and
duration tables for verified culled player handlers. It never writes guest
animation fields. Live timer/state/direction changes take priority, unsupported
handlers hold their native descriptor, and terminal durations/timers suppress
learned-cycle fallback. Inactive records clear both decoder and pose history.

Native fixtures compare scalar and table timing against an independent
transcription of the original shared interpreter for 100 frames. A cold,
stationary margin actor renders all four authored graphics steps and its terminal
hold without cycle learning. Identical-snapshot actor reuse, state/direction
changes, original-window handoff and malformed/truncated tables are covered.
Every rendering transaction checks unchanged guest WRAM and restored VRAM/OAM.

Fresh focused verification: **12 passed in 22.04 seconds** for the native graphics,
menu, configuration, animation, readability, render-contract, renderer-reset and
capture fixtures, plus **6 passed in 52.62 seconds** for real SDL/native graphics
and display checks. Logs: `build/graphics-animation-unit.log` and
`build/graphics-animation-display.log`. Windows, Linux and Android arm64-v8a/x86_64
builds succeeded. Windows executable SHA-256:
`c36f2946cf24acc8f9c712aaf265dea29dc8e1fba28e50b9b4f09fbc102d92f2`.
Android APK SHA-256:
`69c85bd39533b00b4ff9ea0263eb57dbc4768160cba7120fc793fd2a299abad2`.

Five real-ROM widths (256/320/358/398/504) passed at frames 1200 and 3480 with
byte-identical full WRAM and original center pixels. The 3480-frame case also
checks actual pixels of the cold offscreen player. Artifacts:
`build/graphics-animation-widescreen` and `build/graphics-animation-widescreen-3480`.
The [match graphics acceptance guide](GRAPHICS_MATCH_ACCEPTANCE.md) records
additional original Cup-final scenes and accelerated period-transition evidence.
These are bounded graphics checks; untouched full-match play, goals, original
substitutions, completed shootouts and physical Android/display acceptance remain
unverified. The full project suite was not repeated for this follow-up; its two
previously recorded mod-data failures remain unresolved.

The updated executable passed the complete extended match graphics run: **3 tests
in 213.68 seconds**, covering 11 scenes at 256/398/504 pixels with 13 consecutive
or final captures per scene/width, full WRAM equality and exact center pixels.
The tied fourth-period continuation reaches the original shootout first-kick
scene (mode $0C, period 3); it does not prove shootout completion. Log and
manifests: `build/visual-match-acceptance-final`. Widened live/replay/extra-time
PNG margins were inspected; the dedicated shootout camera retains the native
center with black borders.


## Widened penalty camera follow-up (2026-10-01)

The dedicated penalty layout now expands only BG2's tiled crowd, advertising and
pitch. BG1's goal/net and BG3's HUD stay centered, with untouched native OAM and
no VRAM/WRAM mutation or wider simulation activation. Original submodes $0C
(shootout) and $11 (awarded penalty) share this layout; mismatched registers and
other scenes retain their existing policy.

Focused verification: **11 passed in 68.42 seconds**, including native penalty
policy/guard fixtures, animation/widescreen regression, SDL output through 4K and
paused capture geometry. Log: `build/graphics-penalty-focused.log`.
The extended retail test passed **3 tests in 279.40 seconds**: 16 scenes across
53 viewport runs and 1,197 captures. Five penalty stages compare original 256,
16:10 358, 16:9 398 and 21:9 504 pixels. Actual B input resolves the human kick;
original logic advances to the CPU's turn and then the third kick. Full final
WRAM and all sampled native-center pixels match; both margins contain pitch
scenery. The shot/camera stage captures every frame after the first. Details,
logs and gaps: [match graphics guide](GRAPHICS_MATCH_ACCEPTANCE.md).

Windows, Linux and Android arm64-v8a/x86_64 builds succeeded. Windows SHA-256:
`16eaffbe8e5434246ec4033be0bdeca8349cd36475df6509cd5294b064a55511`.
Android APK SHA-256:
`9c4126bfefa54b277e0d6d810393f28c73c9dffa128cdb1db3a8ecb48d53e3fc`.
Actual foul-to-penalty acceptance, completed shootouts and physical device checks
remain pending. The prior two mod-data suite failures were not changed, and the
full project suite was not repeated for this rendering-policy change.


## Original team-management follow-up (2026-10-01)

Original SELECT request/cancellation, first-team formation and bench substitution
commits, actor attributes/roles/home positions, resumed live play and deterministic
advancing save/reload now have input-driven native acceptance with AI off/on.
The focused run passed **19 tests in 117.33 seconds**. Read-only player/keeper
trace diagnostics additionally preserve CPU bus bookkeeping. Widescreen fallback
now respects original auxiliary admission, preventing stale center-origin poses
from being revived on management return; **3 graphics/animation regression tests
passed in 4.94 seconds** after that final correction.

The final visual run passed **1 test in 112.92 seconds**, covering nine stages,
36 runs and 468 captures at 256/358/398/504 pixels. Full endpoint WRAM and sampled
native-center pixels match; the actual visible replacement resolves to Bucario
through the shipped name lookup. Windows, Linux and Android builds succeeded.
Evidence, commands, binary hashes and remaining both-team/extra-time/limit/device
cases are in [original team-change acceptance](TEAM_CHANGES_ACCEPTANCE.md).
This does not certify the substituted human-team actor's optional AI decision
or improved football balance. The full suite's two prior mod-data failures were
not changed.


## v0.3.0-beta.1 release gate (2026-10-02)

The pre-release full suite exposed three failures (238 passed, 3 skipped): the
stale goalkeeper appearance assertion, shared palette records in shipped packs,
and lost widened-player animation across snapshot loads. All three were handled
before publication. Keeper tests/documentation now reflect original graphics
consumers; explicit kit overrides redirect both pointers into pack-specific
isolated records; checked snapshots retain host animation and learned pose cycles.
The prior 128-byte extension and guest-only v4-v8 snapshots remain readable.

Affected save/campaign/match/mod/widescreen checks passed **59 tests in 216.09s**
(`build/release-beta-final-unit-tests.log`). Native advancing replay, video restore,
campaign integration, rematch/drill/favorite, original team management and visuals,
and widescreen checks passed **27 tests, 1 skipped in 365.67s**
(`build/release-beta-final-native-tests.log`). The skip is the absent optional
penalty-area save fixture. These are focused reruns, not a new full-suite result.

Review additionally hardened restored pose-coordinate arithmetic with 64-bit
intermediates. Four serializer/transactional tests passed afterward in 5.56s;
standalone extreme-coordinate tests also passed Clang undefined-behavior traps.
The final Windows rebuild additionally passed **8 tests, 1 skipped in 26.01s**
(`build/release-beta-published-replay-tests.log`), including byte-identical advancing
classic/widescreen replay, animation/pose serializers and snapshot transactions.
Windows, Linux and both Android ABIs build successfully. Final downloadable
package hashes are supplied in the release's SHA256SUMS.txt. APK signature
verification matches the previous beta certificate. Package checks exclude ROMs
and saves and verify archive integrity, version metadata and Android ABIs.

Physical device acceptance and the previously documented full-tournament,
both-team/extra-time substitution and balance gaps remain pending.


## Enhanced eight-direction running (2026-10-02)

The optional Graphics → Running Animation setting defaults to Original. Enhanced
adds a lower-body midpoint to each of the eight authored running/dash keyframes
in every direction. Hold Y / Dash to trigger the verified loop. Walking, shooting,
tackling, goalkeeper and special-screen clips remain original. New pixels derive
from the locally loaded ROM; no cartridge artwork is shipped with this change.

Focused animation, configuration, menu, snapshot and widescreen checks passed
**8 tests in 15.54s** (`build/running-animation-focused-tests.log`). Final synthetic
and real-ROM animation acceptance passed **6 tests in 21.91s**
(`build/running-animation-acceptance-tests.log`). The real geometry atlas verifies
all **64** direction/keyframe midpoints. Classic and widescreen input runs retain
identical guest RAM and snapshot payloads with Original/Enhanced, while rendering
the intermediate poses. Both views reproduce 40 advancing frames byte-for-byte
after save/reload. Unit checks cover owned tiles, uniform details, upper-body
preservation, sprite flips, guarded states and transient VRAM restoration.

Preview artifacts are `build/running-animation-acceptance/running-atlas.png` and
`running-comparison.gif`. Windows, Linux and Android arm64-v8a/x86_64 builds
succeeded. Final Windows executable SHA-256:
`6f6ccf77e86442e3b27a97bd5a4b993cc25494dd13f1bc45360fb22c650998bd`.
Android APK SHA-256:
`239784e84e63e227cfaaa1df54e8a9bcd62612be5dae33986c808aa693e71343`.
Physical-device visual acceptance and subjective animation quality remain
playtesting tasks. This source addition is not part of the published beta.

Final full project regression: **248 passed, 4 skipped, 7 subtests passed in
1262.68s** (`build/running-animation-full-tests.log`). This includes original cup
completion/Continue, campaign transitions, extra time, team-management visuals,
advancing snapshot replay and the new animation acceptance. Skipped optional
cases and physical-device acceptance remain outside this result.


## Mod identities, coin-toss widening and optional cosmetics (2026-10-02)

Current sources remove Ball Outline, including saved legacy enables. Color Boost
is an optional 12.5% saturation increase, default off, applied to game artwork
before host overlays. CRT Strength selects 0/25/50/75/100%; 100% preserves prior
CRT darkening and 0% disables it. Configuration, menu, preview and pure pixel
checks cover these settings. Real-ROM Color Boost/legacy-outline comparisons
preserve endpoint WRAM; native artwork follows the documented color transform,
while host notifications remain separately composited.

The verified pre-match introduction/hand-coin minigame now extends authored
BG1 crowd edge tiles into 16:10, 16:9 and 21:9 margins. Original center pixels and WRAM match
classic at frames 1940 and 2450. BG2, BG3 and OBJ stay clipped to the original
view. A temporary VRAM transaction extends only the outer crowd tiles, avoiding
duplicated television frames; the margin test checks every pixel against the
original crowd edges. This uses existing artwork.

Modded names and flags follow the cartridge's active native sprite identities.
Exact OAM matching suppresses stock parts only when every visible part matches;
the complete OAM transaction is restored before saves or guest execution.
Competition flags associate with source-verified team/name slots, including
repeated Cup group-grid/next-game copies and asymmetric World Series fixtures.
Tests reject malformed geometry, partial matches, an offscreen match hiding a
visible mismatch, conflicting team associations and unsupported scenes.

Final native identity acceptance passed **6 tests in 85.62s**
(`build/team-identity-final-native-tests.log`). Cases cover moving, settled,
partially exiting and fully departed coin banners, Cup list/grid/next-game
copies, and World Series matchups. A distinctive fixture flag appears at all
expected native positions; zero custom pixels leak into widescreen margins.
Original/modded WRAM match, and modded center pixels match classic/21:9.
At frame 2000 the latched original frame still shows eight flag columns
(128 pixels); frame 2010 verifies complete departure. The initial exit test's
zero-pixel assumption was corrected using that native evidence.

The combined graphics/mod/save/animation/display regression passed **28 other
checks**, with the six identity cases rerun as above: **34 unique current checks
passed**. Logs: `build/visual-identity-final-tests.log` and the final native log.
The full project suite was last run before this follow-up (248 passed, 4 skipped,
7 subtests); it was not repeated for these rendering-only changes.

Final Windows, Linux and Android arm64-v8a/x86_64 builds succeeded. Logs:
`build/coin-edge-final-windows-build.log`,
`build/coin-edge-final-linux-build.log`, and
`build/coin-edge-final-android-build.log`.
Windows executable SHA-256:
`75cd2ccc356dc327b7683dab54361c5fdd1941d16c14bd0c4f2ef0298ebe9a45`.
Android APK SHA-256:
`3a830d0e7c2070c896e0219c0d4464086b2a7646dee40f2cbeebf8469ee3f12a`.
Native capture artifacts are under `build/team-identity-final-native` and
`build/visual-identity-final-acceptance`. These source additions have not been
published as a release; physical-device visual/touch acceptance remains pending.

Final crowd-edge refinement: Windows, Linux and both Android ABI builds succeeded.
The affected coin, widescreen and identity checks passed **10 tests in 141.21s**
(build/coin-edge-final-tests.log). Actual 21:9 coin captures were visually
inspected: one centered television, with crowd artwork in both margins.
The coin tests compare every added minigame pixel to the original edge tiles;
original center pixels and WRAM remain identical at all three widened ratios.
Final captures: build/coin-edge-final-acceptance.

## Coin scroll and halftime widescreen (2026-10-02)

The 16:9 coin sequence was captured continuously from original menu input.
The final regression compares all 1061 frames from 1900 through 2960 against
classic center pixels and compares endpoint WRAM through kickoff at frame 3100.
Ten sampled stages cover the sky/stands scroll, television entry, hand/coin,
direction selection and fade. Every added pixel at those stages matches the
isolated native scenery edge. A two-pixel crowd-flag spill during television
entry was reproduced before adding explicit OBJ clipping to these presentations.
The television remains singular and follows the original vertical scroll.

The halftime fixture reaches a live Cup match through original menu input;
only the privately saved clock is shortened to reach the original stats
transition. Classic, 16:10, 16:9 and 21:9 captures have identical center pixels
and endpoint WRAM. The stadium edges extend, while the card and sprites remain
clipped. Native unit checks cover layout rejection, untouched center columns,
VRAM/OAM/WRAM restoration and explicit OBJ clipping that leaves live-match
sprite widening unchanged. This fixture verifies halftime; fulltime and every
stadium/transition variant have not been independently captured in this run.

Final affected checks: **10 passed in 130.00s**
(`build/coin-stats-final-tests.log`). Windows, Linux and Android
arm64-v8a/x86_64 builds succeeded (`build/coin-stats-final-*-build.log`).
Windows SHA-256:
`ea4b82ef934142ba6b6eebf7e33036ce9b053c6d9c945390a6d4fcbbd7f5b4e3`.
Android APK SHA-256:
`a360ab6e363a08b30c8efa40d8f96528c554eee88cdddc86ce2e29ba57210c7b7`.
Captures and the 16:9 scroll preview are under
`build/coin-stats-final-acceptance`. The full project suite was not rerun.

## Coin crowd quality correction (2026-10-03)

Visual feedback rejected the prior single-column extension: repeating an
8-pixel edge cut through close-up fan bodies and produced identical spectators.
The earlier pixel checks established consistency with that algorithm, rather
than adequate visual quality. Those expectations have been replaced.

The extension now uses 128-pixel authored scenery sections and 96-pixel
close-up crowd sections containing four complete 24-pixel fans. The left
section's phase preserves the partial native fan across the boundary. Narrow
16-pixel sections avoid the television; four plain wall rows use their first
8-pixel tile because the next tile already contains television shadow. All
selection uses map rows, so the vertical scroll remains unchanged. Native
center tiles, game state, sprite clipping and transaction restoration remain
preserved. No replacement artwork is shipped.

The same tilemap rows initially contain sky before the cartridge streams in
the stadium. Narrow sections activate only when the row contains the verified
television tiles; close-up fans require their ordered head/body tile groups.
Unloaded sky retains wide sections instead of repeating small cloud fragments.
The scroll regression includes this early loading stage.

Final affected validation passed **6 tests in 179.79s**
(`build/coin-crowd-stream-final-tests.log`): coin captures at all three widened
ratios, every native center frame through the 16:9 scroll, sampled margin
references, rejection of 8-pixel fan repetition on both sides, halftime
regression, and native layout/transaction checks. Fresh 16:9/21:9 captures and
a nine-stage scroll contact sheet were visually inspected. Artifacts and the
replacement GIF are in `build/coin-crowd-stream-final-acceptance`.

Windows, Linux and Android arm64-v8a/x86_64 builds succeeded; logs are
`build/coin-crowd-stream-final-*-build.log`. Windows executable SHA-256:
`ad13fb584fea2222b4aa9ea903fab288bd685c3cb70b0fca4a8e206bfc704f1b`.
Android APK SHA-256:
`a1b7f9ffe2bf9a4bf75e288a7cd8350f0f6cdc28ae842242fea910ab850a3281`.
The full project suite was not rerun for this presentation correction.

## v0.4.0-beta.1 release gate (2026-10-03)

The complete project suite passed: **262 passed, 3 skipped, 7 subtests passed in
1834.47s**, recorded in `build/release-0.4.0-full-suite.log`. Windows x64,
Linux and Android builds succeeded (`build/release-0.4.0-*-build.log`).
Archive integrity, checksums, Windows x64 architecture and both Android ABIs
were verified. A clean Windows package extraction booted for 60 frames with
isolated configuration and save directories. Android signature verification
passed and its signing certificate matches v0.3.0-beta.1; version code is 6.
The pending physical-device and untouched campaign acceptance remains pending.
