# v0.5.0-beta.4 release validation

Widescreen, animation and controller improvement candidate: Fit Pixel Perfect scaling,
gameplay-only widescreen mode, weather/boundary polish, 8-direction walking animation
interpolation, detached controller slot reclamation and GPL-3.0 licensing.
Android version name 0.5.0-beta.4, upgrade code 10.

## Builds and platform checks

Windows x64 (MSVC/Ninja CMake Release) and WSL Ubuntu 24.04 Linux x64 Release builds
succeeded with zero compilation errors.
Android `assembleRelease` succeeded for arm64-v8a and x86_64 with version code 10 and
version name 0.5.0-beta.4, using the development signing certificate.
The frozen Mod Studio executable was built using PyInstaller and validated.

## Regression and feature validation

- Focused widescreen raster and native fit checks passed (**test_widescreen_raster.py**,
  **test_fit_widescreen_native.py**).
- Enhanced running and walking animation interpolation and offscreen phase alignment
  checks passed (**test_running_animation_native.py**, **test_running_animation.c**).
- Controller connection and slot reclamation checks passed (**test_local_input.c**).
- Release package assembly, archive member safety, and Android version/code gate tests
  passed (**test_assemble_release.py**).
- Checksums in `SHA256SUMS.txt` match all generated binary and mod package archives.

## Scope and limits

All platform bundles contain editor source, licenses and the MEADOW/HARBOR example pack;
Windows includes the frozen Mod Studio editor. Standalone editor, example, Liga MX and
World Cup 2026 ZIPs are provided. Packages contain no proprietary ROMs, personal
configuration, or saves.
Physical Android device feel, physical 4-controller hardware tests, and complete 35-match
human World Series playthrough remain 1.0 work.
No untested macOS or separate SteamOS binary is published.

See [controls](CONTROLS.md), [graphics settings](GRAPHICS_SETTINGS.md), and
[previous release validation](RELEASE_VALIDATION_0_5_0_BETA_3.md).
