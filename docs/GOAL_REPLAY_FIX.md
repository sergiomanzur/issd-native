# Widescreen goal replay correction (2026-10-09)

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
