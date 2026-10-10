# Current-source release validation — 2026-10-09

This is validation of the uncommitted working source, including the audio,
shootout autosave and goal-replay fixes. It does not create a tagged release or
change `VERSION` (`0.4.0-beta.1`). Candidate artifacts are isolated under
`build/release-validation`; existing published release archives are untouched.

The later same-day expanded-stadium repair has separate
[acceptance](EXTRA_STADIUM_ACCEPTANCE.md) and
[platform build evidence](EXTRA_STADIUM_PLATFORM_VALIDATION.md). Those artifacts
include the subsequent mod-loader and serializer changes; the hashes below
identify the earlier Cup/release validation baseline.

## Windows

A fresh Release build used Clang, Ninja and the existing SDL2 installation:

```powershell
cmake -S . -B build/release-validation/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=C:/Users/sergi/scoop/apps/sdl2/current
cmake --build build/release-validation/windows --parallel 4
```

The executable SHA-256 is
`d9710b024189d2aaed556f2d4b0a89d8247a1affc491a9a364e396dd618a3433`.
The build supplied the required AOT deny list. SDL2.dll was explicitly copied
from the existing dependency into the candidate output; this CMake
configuration did not copy it automatically.

The candidate ZIP uses an explicit file list: executable, SDL2.dll, deny list,
documentation, licenses and an empty mods directory. ZIP CRC validation and
content checks passed; it contains no ROM, save, local configuration or WRAM
dump. The archive SHA-256 is
`9d3084c76e44cf986729e1c1b185e95662a76d3f136cba634688133029671bca`.

The archive was extracted into a new system temporary directory outside the
checkout. With a separately supplied private retail ROM, fresh settings and a
fresh save directory, 6,420 ordinary scripted frames reached an actual 0–1
goal replay at 16:10. Reloading its native quicksave advanced the original
replay and accepted its pause control; loading did not change the save file.
A second reload with explicit `--load-state 0` also passed with PATH restricted
to Windows System32. The initial helper incorrectly supplied a path to this
frame-number option; its conversion to frame zero was corrected and rerun.
The 358×224 replay capture was visually inspected; the native replay, radar,
players and pitch are present without the stale edge-player column.

Executable and SDL2 import inventories are retained with the evidence. The
executable imports VCRUNTIME140.dll and the Windows Universal CRT, so this
check uses the host's installed system runtimes; it does not certify a pristine
Windows installation without the VC runtime. SDL dummy video/audio checks do
not certify a visible window, sound device or physical controller.

Evidence: `packages/windows-clean-install.json`, the clean-install/reload logs,
DLL import inventories, `windows-configure.log` and `windows-build.log` within
the owned validation directory.

## Full project suite

The fresh Windows executable above was installed at the canonical test path.
The previous executable was retained in the owned validation directory.
The complete suite passed: **302 passed, 9 skipped, 8 subtests passed** in
4,652.85 seconds. This run collected 311 tests before the 18 new diagnostic
controller/certificate tests were added.

```powershell
python -m pytest tests -q --basetemp=build/release-validation/pytest-fresh
```

The optional 35-match World Series/Continue and extended-period visual tests
both passed against the same executable with `ISSD_RUN_FULL_WORLD=1` and
`ISSD_VISUAL_EXTENDED=1`: **2 passed** in 2,266.82 seconds, recorded in
`extended-suite.log`. Their clock/score fixtures are explicitly accelerated.

The later diagnostic tests passed separately: **18 passed** in 4.41 seconds
(`cup-certificate-final.log`). Current collection contains 329 tests. The
initially skipped Mod Studio tile-pane test passed on a focused rerun
(`remaining-skips.log`). Across these runs, 323 distinct tests passed and six
remain skipped: physical audio timing, the unavailable penalty-area snapshot,
and four optional known-failing expanded-stadium constructor audits. Skips are
not passes; `skipped-test-ids.json` retains the original nine skipped IDs.

## Linux and Android

Fresh independent platform builds and their artifact checks are recorded in
[the platform build report](PLATFORM_BUILD_VALIDATION_2026_10_09.md).
The fresh Linux binary also passed an extracted-package 6,420-frame natural
goal replay and 90-frame save/reload/pause check at 16:10, retaining identical
save bytes on load. Its private ROM remained external to the archive.

Android's fresh build compiled both arm64-v8a and x86_64. Package inspection
found a missing canonical AOT deny-list asset used by the added-team squad
loader. The packaging fix includes that file and extracts it into app-private
storage before SDL starts native code, preserving a supplied nonempty path.
The corrected APK was rebuilt incrementally from the fresh staging; its native
library hashes remained unchanged. Asset bytes, ZIP CRC, ABI contents,
manifest and debug signature were checked. See the platform report for the
final artifact identity and build evidence. Android installation and device
runtime behavior remain unverified.

## Untouched winning Cup

The fresh original-controller run completed all nine Cup matches in one process
with no state loads, passwords or guest clock/score edits. Brazil won the final
17–4 after one qualifying loss. The normal championship ceremony advanced via
ordinary A input to the final table and published generation 12. No gameplay
change was necessary; an earlier diagnostic stop condition was corrected.

All 12 contiguous native checkpoint generations passed Continue using isolated
copies and the fresh production Windows executable. Save bytes, campaign tables,
progress, settings and teams were preserved. The strict winning certificate
binds every checkpoint hash to its production Continue result. Evidence and
commands are in [winning Cup acceptance](CUP_WINNING_ACCEPTANCE.md),
`build/cup_control_nomash10/winning-certificate.json` and
`build/cup_control_nomash10-continue-all/verification.json`. An independent
read-only review confirmed the certificate coverage and documentation scope.
