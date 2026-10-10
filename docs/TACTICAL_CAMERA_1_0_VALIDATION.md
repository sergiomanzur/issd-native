# Tactical camera — 1.0 development validation

Implemented on 2026-10-10. This is a development feature; VERSION and the
published v0.5.0-beta.1 release are unchanged.

| Graphics camera | World scale | Visible coverage per axis |
| --- | --- | --- |
| Classic | 100% | Original |
| Tactical | 80% (4/5) | 25% more |
| Tactical Wide | 67% (2/3) | 50% more |

The renderer reads the stadium world directly rather than shrinking the native
frame or wrapping the 512-pixel tile ring. Signed actor pieces retain coordinates
outside native OAM limits. The original projection, tracking, simulation, guest
replay payload and framebuffer dimensions remain intact. HUD extraction and
readability overlays retain normal size. Shared source lists expose the same
controls on desktop and Android.

## Verified coverage

- Geometry/config tests cover every supported width, negative coordinates,
  odd sizes, integer overflow boundaries and real persistence round trips.
- Renderer fixtures cover distinct world columns separated by 512 pixels,
  vertical expansion, flips, signed actor positions, private PPU restoration,
  artwork lookup, sprite overlap, BG/OBJ window masks and color effects.
- The destination-sampling optimization matches every output pixel against
  the complete expanded-world reference. Explicit regression fixtures cover
  native OBJ priority, native/supplemental overlap and save-load identity reset.
- Fresh identical-input native runs at live frame 4000 and natural goal replay
  frame 6420 compare all guest WRAM across the three modes.
- Linux runtime checks load the same live save: all three modes produce
  byte-identical WRAM and pixel-identical pictures to Windows.
- Saved live/replay and menu checks cover 4:3, 16:10, 16:9 and 21:9 in all modes:
  guest WRAM stays identical; live/replay pictures differ, menu pictures do not.
- Custom stadium replay acceptance uses independently authored geometry and
  local artwork, natural goals and save reloads. Camera comparisons additionally
  cover 16:10, 16:9 and 21:9.

Private captures, saves, cartridge-derived fixtures, hashes and build/test logs
remain under ignored `build/tactical-camera/`; no cartridge assets are committed.

## Scene/history limits

Dedicated goal-facing penalty/shootout scenes and management/substitution
screens use original scanout. Fixture guards cover this fallback; the tactical
setting does not change their cameras. Campaign cards and menus likewise retain
existing presentation. A complete human playthrough with camera switching at
all transitions is still a release acceptance gate.

The existing companion replay history records omitted player positions and
animation descriptors, selected by the original replay cursor and payload hash.
It uses snapshot extension version 4; earlier formats remain accepted. The
original decoder supplies the ball. No additional auxiliary history format is
introduced without a concrete missing-motion reproduction. Unlisted replay
auxiliaries remain excluded to avoid stale objects. Live vertical auxiliary
expansion requires previously admitted identity; loading clears that cache.
Older recordings cannot invent missing player motion.

Custom-art replacements retain the native output at pixels undergoing native
clipping or color math. Arbitrary HDMA raster layouts, rotated perspectives,
new tracking modes and new sprite artwork are outside this feature.

## Portability and performance

Windows x64, Linux x64 (WSL Ubuntu 24.04), and Android arm64-v8a/x86_64 compile.
Android physical-device performance, touch ergonomics and full-match visual
acceptance remain unverified; an APK build is not runtime acceptance.

Timing is measured around the complete native frame drawing function using
`ISSD_CAMERA_PROFILE=1`, including ordinary scanout and tactical composition,
excluding simulation/audio/SDL presentation. The private benchmark loads the
same live/replay save for each mode and discards ten warm-up frames. Host:
Windows, AMD Ryzen 7 2700X. Measurements were taken alongside background
build/tests, so they are diagnostic costs rather than a 60 Hz device guarantee.
Replay sampling is intentionally short to stay within the saved replay segment.

| Scene / mode | Samples | Median ms | p95 ms | Max ms |
| --- | --- | --- | --- | --- |
| live / Classic | 170 | 1.91 | 2.53 | 3.00 |
| live / Tactical | 170 | 6.34 | 9.72 | 11.63 |
| live / Tactical Wide | 170 | 6.61 | 9.88 | 10.82 |
| replay / Classic | 20 | 1.96 | 3.47 | 3.47 |
| replay / Tactical | 20 | 6.78 | 11.93 | 11.93 |
| replay / Tactical Wide | 20 | 7.87 | 11.77 | 11.77 |

A compiled size probe reports 2,391,640 bytes (2.28 MiB) of camera-owned static
buffers, including auxiliary identity storage, the field PPU copy and HUD capture, in addition to existing replay/presentation history.
Destination sampling bounds allocations at 504x224; expanded world dimensions
do not multiply framebuffer storage.

## Final targeted checks

The final enhanced-running integration first failed both tactical real-input
fixtures: private sprite uploads replaced the running leg edits. Camera preparation
now precedes the running overlay, with reverse restoration unchanged. Both
zoom-outs now change the expected running pixels and reload a saved live frame
pixel-identically: `test_running_animation_native.py -k tactical` reports
**4 passed**. Classic retains its existing ordering.

Latest targeted results include **14 passed** for geometry, configuration,
renderer, actor, HUD, artwork, replay history and snapshot units; **4 passed** for
fresh natural live/replay camera and custom-stadium/artwork native acceptance;
and **3 passed** for the corrected save-context/capture harnesses and gameplay
hook checks. Linux reproduces the Windows live-save WRAM and image in all
three modes. Windows, Linux and both Android ABIs build after the running fix.

Three broad-suite harness failures required corrections: save-context
expectations now account for the always-installed passive replay observer,
the extracted screenshot harness supplies the camera-active query, and the
standalone substitution name probe links the production camera module. Camera
selection is explicitly verified not to change save compatibility flags.

Five pre-existing acceptance exclusions remain: idle-device audio timing,
opt-in 35-match World Series, sustained natural added-stadium goals, extended
period transitions, and an unavailable penalty-area save fixture. These are
recorded exclusions, not successful runtime checks.

Actual same-frame implementation pictures are exported to the task's local
visualizations directory. They show fresh gameplay and a naturally scored goal
replay; no cartridge images are included in the Git repository.

The complete project command was `python -m pytest tests -q`: **444 passed,
3 failed, 5 skipped, 8 subtests passed**, in 54m55s. Its three failures are the
harness issues above. The broad run retained its initial immutable Windows
binary while final targeted native checks used the isolated candidate build.
The latest Windows candidate has since been copied to `build/ISSDNative.exe`
and its SHA-256 matches the isolated build. This report does not represent a
second complete all-green sweep of the final candidate.

The first two failing tests and the gameplay hook check were rerun against
current sources after that copy: **3 passed in 5.20s**. The substitution probe's
missing linkage was reproduced independently before correcting its source list.

The entire `test_team_changes_visual.py` native sequence then passed (one test,
exit 0): management request, formation change/commit, pending substitution,
return to play and completed throw-in across the supported wider aspects,
followed by the actual incoming-player name check. Thus all three broad-run
failures passed corrected reruns. Four additional tactical enhanced-running
cases also passed after the broad run's original collection.

Independent code review found no remaining issues after the rendering fixes,
and a final focused review confirmed the running-overlay order and subscreen
sampling optimization. `git diff --check` passes. Remaining 1.0 release gates
are the full human transition/campaign playthrough and physical Android runtime
acceptance described above.
