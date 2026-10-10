# v0.5.0-beta.1 release validation

This release includes the changes since v0.4.0-beta.1: audio timing,
shootout saves, substitution/campaign flows, replay margins and snapshot
animation preservation, expanded stadium identity, independent profiles,
native/local HD artwork, and the improved Mod Studio.

## Release gate

The fresh `python -m pytest tests -q -rs` run exited successfully: **430 passed,
5 skipped, 8 subtests passed in 2887.14 seconds (48:07)**. Logs and
extracted-package checks are recorded under the private ignored
`build/beta-release-0.5.0/` directory. Earlier acceptance evidence is retained
in the linked reports; it is not relabeled as a fresh run.

The five skips cover idle-machine/default-device audio timing, the opt-in
35-match World Series run, the opt-in sustained natural stadium goal run,
extended visual period transitions, and an unavailable penalty-area savestate
fixture. Prior natural campaign/stadium and audio evidence is linked below.

Eight new archive tests passed separately after their initial failing runs
(they were added after full-suite collection). They verify
explicit public file lists, preservation of existing ZIPs on missing inputs,
safe archive member paths, Linux executable permissions, and rejection of stale
Android version names or upgrade codes.

## Builds and packaging

Windows x64 and Linux x64 use the current Release builds under
`build/release-validation/windows` and `build/release-validation/linux`.
Android `assembleRelease` rebuilt successfully with version name
`0.5.0-beta.1`, version code 7, arm64-v8a and x86_64. Mod Studio was rebuilt
with PyInstaller and its bundled Tk/Pillow startup selftest passed.

The assembler uses explicit documents, executables, libraries and licenses.
The original-art example is passed through the editor's validated portable
dependency exporter. Its manifest is placed directly in the bundle's `mods/`
root so the native nonrecursive scanner can find it. No cartridge, personal
configuration, saves or investigation dumps are selected.

Windows includes the frozen `ISSDModStudio.exe`; every platform archive
includes the editor source, baked facts and twelve editor images. A standalone
editor ZIP and standalone example ZIP are separate release assets. Linux
retains executable permissions and requires system SDL2/compatible libraries.

Android uses the existing beta development signing key. Actual APK manifest
identity/version/code/minimum SDK, both ABI ELF headers and three native
libraries per ABI, canonical deny bytes, ZIP CRC and alignment all passed.
APK v2 signing verified; the signing certificate matches the downloaded
v0.4.0-beta.1 APK. This is an automated
package check, not physical Android gameplay acceptance.

Clean extraction passed ordinary fresh-menu startup to both example profiles
(IDs 8 and 31) on Windows and Linux. Read-only state dumps verified logical ID,
base template, pitch dimensions and live/replay mode. The extracted frozen
Windows editor passed the two-profile workflow: invalid draft, compiled
preview and portable export. Relocated editor source passed the same workflow
on Windows with the complete Python/Tk runtime. This does not claim Linux GUI
or Android device execution. Archive CRC and all eight checksums passed.

Independent release review rechecked the packaging/version gate and eight
focused tests, with no remaining code findings. The inspection binds manifest,
ABI and signature checks to the actual APK SHA-256.

`SHA256SUMS.txt` covers all eight binary/package assets. Source code is published
at the release tag. No untested macOS or separate SteamOS binary is supplied.

## Acceptance scope

See [independent stadium acceptance](INDEPENDENT_STADIUM_ACCEPTANCE.md),
[continuous campaign acceptance](CONTINUOUS_CAMPAIGN_ACCEPTANCE.md),
[Cup winning acceptance](CUP_WINNING_ACCEPTANCE.md),
[shootouts](SHOOTOUT_ACCEPTANCE.md), [substitutions](SUBSTITUTION_FLOW_ACCEPTANCE.md),
[audio timing](AUDIO_TIMING_FIX.md) and [goal replays](GOAL_REPLAY_FIX.md).

The independent stadium contract remains bounded: shortened pitches, original
template widths/scenery and native allocation budgets. Physical Android
gameplay/touch/lifecycle and physical four-controller acceptance remain
unverified. Netplay is not implemented.
