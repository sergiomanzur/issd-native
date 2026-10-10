# Widescreen goal replay correction (2026-10-09)

## Follow-up: missing edge players (2026-10-10)

The original correction below removes stale ghosts, but cannot supply players
that the original narrow-view replay never recorded. The new implementation
keeps a separate presentation companion for each of the 512 original directory
entries. It records all 22 active players' positions, properties and descriptors;
live widescreen rendering also records the resolved pose actually shown.

The observer at `$8BAAFD` runs after the original payload and end link are
written. `$8BAE80` captures the selected playback entry before its cursor moves;
that selection is paired with the previous RAM generation used by native OAM.
The original `$8BA997` directory reset clears the companion. A fingerprint of
the original payload rejects ring reuse. Only omitted actors with matching
history are restored into presentation RAM, and playback uses their recorded
descriptors rather than advancing the live predictor. Original replay bytes,
WRAM, native OAM and the simulation camera are unchanged.

Snapshot extension version 4 retains the companion. Versions 1–3 and guest-only
saves remain readable; old recordings cannot recover data they never captured.
Without valid companion history the prior ghost-suppression rule still applies.

A naturally scored retail goal was compared against the released beta binary.
At frame 6420 the companion restores omitted players in the left margin. All
131,072 guest RAM bytes and the original 256-pixel center are identical across
released native, released wide and updated wide runs. The difference is confined
to widened columns. Both-side multi-piece actor tests and a native/supplemental
split sprite test pass. Pause, rewind, save/load and custom-art native checks
passed in the affected regression run (17 passed, one missing optional fixture
skipped). Unit checks also cover invalid state, unchanged guest memory and ring
reuse. The black-card/menu corrections have their own native and unit tests.

The first native probe exposed an interior-opcode hook that worked only in its
unit fixture; no companion records were created in the actual compiled path.
Moving capture to the verified `$8BAAFD` block boundary corrected that integration
defect. Private proof and comparison images are under
`build/reported-widescreen/`; they contain owned cartridge pixels and are not
release resources. No new public release is claimed by this source change.

## Original ghost correction

Goal replays could show stationary players with frozen poses in widened margins.
These were stale player records, not an animation interpreter that needed to run.

## Cause and correction

Original `$8BAAC6` records only players whose `$1E` admission word is zero.
Replay decoder `$98F279` marks all 22 player records absent (`$1E=1`), then
`$98F291` clears that word only for players restored from the current recorded
frame. Omitted records retain their old screen coordinates and descriptors.
They contain no current replay motion or animation to reconstruct.

The widescreen supplement previously scanned all active-looking player records,
including those omitted by the replay. The live-play culling workaround could
therefore draw stationary leftovers. `issd_widescreen.c` now excludes absent
players specifically in replay mode `$13`, in both object discovery and rendering,
and expires their presentation caches. Live culls continue to be reconstructed;
recorded replay actors retain their authentic edge pieces. No guest WRAM, replay
data, timing, camera or native sprites are changed.

## Fresh verification

An original-menu Exhibition with ordinary menu input and untouched clocks/scores
produced a real CPU goal. At frame 6420 the original replay was mode `$13`, stage
6, score 0–1. Five omitted players retained valid descriptors at x=-31/-32:
objects `$0800`, `$0B00`, `$0C00`, `$1700`, `$1900`.

The regression failed before the fix because these absent records produced
supplemental sprites. Afterward it passes for both sides at 16:10, 16:9 and 21:9,
through scanned and native-list paths. It also checks recorded edge sprites,
live culls, absence/re-entry with previous-frame OAM timing, unchanged WRAM and
restored render transactions.

Affected unit checks passed **5 tests**. Native checks passed **12 tests with
1 skipped** in 124.57 seconds: natural-goal automatic playback, pause and rewind
at 4:3/16:10/16:9/21:9, existing original pause/replay width comparisons, enhanced
running replay and advancing save/load image/state checks. Every tested widened
replay center matched native 4:3 exactly, with identical full WRAM. The skip is
the missing optional penalty-area snapshot fixture; it is not counted as a pass.

A separate fresh before/after run also matched all 131,072 WRAM bytes. Matching
16:9 screenshots at frame 6360 differ in **1,112 left-margin pixels**, removing
the ghost column; center and right-margin pixels are identical. The remaining
recorded edge players are retained. Independent read-only code review found no
correctness issues.

Artifacts remain under ignored `build/goal-replay-investigation`: `replay-before`,
`replay-after`, `before-after-evidence.json`, `goal-replay-difference.png`,
`green-final.log` and `native-green.log`. Native width/control manifests and
consecutive screenshots are in `native-green`. ROM/snapshots are private.

Updated Windows executable SHA-256:
`f630e38da941ddb1b73183e035e09a4a8a6876f11fd4e4760e939cf3b65b9603`.
The configured widescreen compile and production link succeeded. The previous
complete project suite belongs to the earlier `8d9a0fbe...a51c4` build; this
correction received the affected checks above, not a second complete-suite run.

```powershell
python -m pytest tests/test_widescreen_goal_replay.py tests/test_widescreen_native.py tests/test_widescreen_reliability.py tests/test_animation.py tests/test_pose_history.py -q
python -m pytest tests/test_goal_replay_native.py tests/test_match_visual_acceptance.py::test_original_pause_and_replay_visual_widths tests/test_running_animation_native.py tests/test_savestate_replay.py -q
```
