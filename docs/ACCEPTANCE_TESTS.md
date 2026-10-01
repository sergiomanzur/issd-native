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
| Touch/config tests | Geometry, multi-touch, persistence, mapping, threshold, edit/reset semantics. | Android touch ergonomics, actual host focus or OS lifecycle. |

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

Final integration on October 1: the project suite returned **174 passed, 2 failed, 1 skipped, 7 subtests passed** in 588.66 seconds. The two existing failures concern an appearance-nibble expectation and shipped-pack kit-sharing warnings. After the last host/menu input fixes, the focused suite returned **30 passed** in 125.74 seconds against the final Windows executable. Windows, Linux, and Android arm64-v8a/x86_64 compiled successfully, and the three new overlay pages were visually inspected. No Android device was attached for ADB acceptance; no physical-device result is implied by these checks.

Fresh-start Cup acceptance also completed **1 passed in 266.03 seconds** against the final Windows build: all nine native matches, stages 0 through 9, championship ceremony/table, and Continue after each result and final completion. This test was added after the 174-pass full-suite run and verified separately. Its World Series variant is skipped by default and has not been executed; opt in with ISSD_RUN_FULL_WORLD=1 only for a dedicated 35-match run. The accelerated Cup pass does not replace the untouched or physical-device checklist above.
