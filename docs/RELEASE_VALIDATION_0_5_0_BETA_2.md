# v0.5.0-beta.2 release validation

Release candidate: tactical cameras, companion replay player history,
centered black campaign cards, and all fixes from the previous beta.
Version name is 0.5.0-beta.2; Android upgrade code is 8.

## Release gate

The fresh `python -m pytest tests -q -rs` run exited successfully:
**451 passed, 5 skipped, 8 subtests passed in 3073.35 seconds (51:13)**.
This is a complete green run of the current runtime candidate, including the
camera/running, save-context, screenshot and substitution-probe corrections.
Private logs and extracted-package evidence remain under
`build/beta-release-0.5.0-beta.2/`.

The five skips are idle-machine/default-device audio timing; the opt-in 35-match
World Series; the opt-in sustained natural added-stadium goal run; extended
visual period transitions; and an unavailable penalty-area save fixture.
Prior natural campaign/stadium and audio evidence is retained in the previous
beta's acceptance records. These exclusions are not fresh successful tests.

## Builds and actual package checks

Windows x64 and Linux x64 Release builds succeeded from current sources.
Android `assembleRelease` succeeded with version name 0.5.0-beta.2 and code 8,
containing arm64-v8a and x86_64. Mod Studio was rebuilt with PyInstaller; its
isolated startup self-test passed. Eight archive/version-gate unit tests passed.

Clean extraction from the actual Windows ZIP and Linux tar.gz passed normal
menu-driven startup of both independent stadium profiles: IDs 8/31, lengths
1728/1664, base template 0 and width 576. Tactical 80% and Tactical Wide 67%
were respectively enabled for those profiles; trace evidence confirms actual
renderer activation. Each Linux final image and full guest RAM matches its
Windows counterpart. This is package runtime evidence on Windows and WSL
Ubuntu 24.04, not an Android gameplay claim.

The extracted frozen editor passed authored preview, invalid-draft and portable
export workflows. Relocated bundled source passed the same workflow using a
complete Windows Python/Tk runtime. These checks do not certify a Linux GUI.

Actual APK manifest identity, version/code/minimum SDK, both native ABI ELF
headers, all three native libraries per ABI, canonical deny asset, ZIP CRC,
alignment and v2 signature passed. Its signing certificate matches the
v0.5.0-beta.1 APK downloaded from GitHub for this check.

Archives use explicit public resource lists and retain Linux executable mode.
Windows includes the rebuilt frozen editor; every platform archive includes
editor source, baked facts, twelve editor images, licenses and the validated
portable independent-stadium example. Separate editor, example and existing
Liga MX/World Cup 2026 pack ZIPs remain available. No ROM, personal config,
saves or investigation dumps are selected. `SHA256SUMS.txt` covers all eight
binary/package assets. Final documentation-only reassembly is checked
against the clean-tested runtime/resources before upload.

## Acceptance scope

See [tactical camera validation](TACTICAL_CAMERA_1_0_VALIDATION.md),
[previous beta acceptance](RELEASE_VALIDATION_2026_10_10.md),
[goal replay history](GOAL_REPLAY_FIX.md),
[independent stadiums](INDEPENDENT_STADIUM_ACCEPTANCE.md),
[shootouts](SHOOTOUT_ACCEPTANCE.md) and
[substitutions](SUBSTITUTION_FLOW_ACCEPTANCE.md).

Camera changes preserve the original simulation and projection, and HUD size.
Behind-goal penalty/shootout views and management/unsupported scenes keep their
original camera. Earlier saves remain accepted; older replay recordings cannot
invent motion that they did not record. Physical Android gameplay/touch/lifecycle,
physical four-controller matches, and full human campaign camera-switching
acceptance remain unverified for 1.0. Netplay is not implemented.
