# Fresh Linux and Android validation — 2026-10-09

This validates the current working tree for release preparation, without changing the declared version or publishing a release. Windows and the full test suite are recorded separately by the main validation task. Build products and logs live in `build/release-validation/`; earlier platform build directories were preserved.

## Linux x86-64

Fresh configure/build completed with exit code 0 and all 228 Ninja steps. Toolchain: WSL Ubuntu 24.04, CMake 3.28.3, Ninja 1.11.1, GNU C 13.3.0, SDL 2.30.0. Commands, run from the repository in WSL:

```sh
cmake --preset linux -B build/release-validation/linux
cmake --build build/release-validation/linux -j 6
file build/release-validation/linux/ISSDNative
sha256sum build/release-validation/linux/ISSDNative
ldd build/release-validation/linux/ISSDNative
```

Output: `build/release-validation/linux/ISSDNative`, ELF x86-64 PIE. SHA-256: `0f9c61e94153f12692b7f8d77b80bc5b77b51588b32afb2d62a48df012cb69f4`. Configure/compiler output: `linux-build.log`; artifact identity and complete dependency list: `linux-artifact.log`. Direct dependencies are `libm.so.6`, `libSDL2-2.0.so.0`, `libc.so.6` and the ELF loader; every transitive dependency resolved on this host.

The binary and its freshly copied `aot_boot_deny.txt` were staged in `linux-package/`. From that isolated working directory, with empty `saves/` and `mods/` directories and no configuration, this command completed 240 frames and exited cleanly (exit 0):

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./ISSDNative --headless 240 \
  --rom "/mnt/c/Users/sergi/Homestead/code/isssdeluxe-recomp/International Superstar Soccer Deluxe (USA).sfc" \
  --config config.json --save-dir saves --mods-dir mods
```

Evidence: `linux-package-smoke.log` reports the local AOT deny file, default configuration, zero mods, successful private ROM loading, 240 frames and clean exit. The ROM was supplied externally and is not included in the package. A separate smoke from the repository also passed (`linux-smoke.log`). An initial package command failed because its shell variable resolved to an empty path; the literal absolute-path rerun above passed.

Limits: this is the host Linux target, not the SteamOS target. No Steam Deck compatibility or real display/audio/controller validation is claimed. The existing startup banner says “Target: Windows x86-64” even in this verified ELF Linux build; that cosmetic issue was not changed here.

Clean distribution archive: `build/release-validation/issd-linux-validation.tar.gz`, SHA-256 `854e33861d6a5edf746891d74539f5507ae3526065485ade33bf6e6e8f846d70`. Its verified file list is exactly `ISSDNative` and `aot_boot_deny.txt`; no ROM or generated config/save/mod fixtures are included. It depends on the host libraries listed above. Archive inspection is recorded in `linux-package-inspection.log`.

Stronger clean-install acceptance also passed. `wsl -d Ubuntu-24.04 -- python3 /mnt/c/Users/sergi/Homestead/code/isssdeluxe-recomp/build/release-validation/stage_linux.py` extracted that archive into the new `linux-clean-install/` directory and started with owned empty mods/saves and a fresh Enhanced-mode, true-widescreen 16:10 configuration. Scripted ordinary Exhibition input ran 6,420 frames to a natural goal: dumped 131,072-byte WRAM contained mode `0x13` and score `0–1`; a native quicksave was written. A new process loaded it at frame 0 and ran 90 frames with `0 NONE`, `20 X`, `30 NONE`: WRAM retained mode `0x13`, replay pause flag `$18B4` was 1, and quicksave bytes were unchanged. Both process exits were 0 with no interpreter-cap error. Evidence: `linux-clean-install.json`, `linux-clean-install.log`, `linux-clean-reload.log`, and the exact reproducible script `stage_linux.py`. SDL video/audio used dummy devices throughout.

## Android

Fresh `assembleRelease` completed with exit code 0 in 1h 19m 13s; all 51 actionable Gradle tasks executed. Both ABIs were configured and compiled from fresh owned native staging. Configuration remains application ID `com.issdnative`, version name `0.4.0-beta.1`, version code 6, SDK 34, minimum SDK 26, NDK `26.2.11394342`, ABIs `arm64-v8a` and `x86_64`. The release build uses the project's debug signing configuration.

Gradle 8.7 (Azul JVM 21.0.12.1), Android Gradle Plugin 8.5.2 and SDK CMake 3.22.1 were selected. The globally installed Gradle 9.7.1 was not used. A temporary init script (`android-fresh.init.gradle`) redirects Gradle output and native staging into new `android/` subdirectories inside the validation directory, preserving the canonical `android/app/build` and `.cxx` directories. From repository `android/`:

```powershell
& C:/Users/sergi/.gradle/wrapper/dists/gradle-8.7-bin/bhs2wmbdwecv87pi65oeuq5iu/gradle-8.7/bin/gradle.bat --no-daemon --no-build-cache --rerun-tasks --init-script ../build/release-validation/android-fresh.init.gradle assembleRelease
```

Logs: `android-toolchain.log`, `android-build.log`. The SDK reports `[CXX5304]` because installed `cmdline-tools;19.0` resides under `cmdline-tools/latest` instead of `cmdline-tools/19.0`; the build has continued through this warning. Physical-device install/lifecycle/input acceptance is outside this run.

The initial fresh build generated `build/release-validation/android/app/outputs/apk/release/app-release.apk`; that output was subsequently replaced by the fixed incremental build described below. The preserved initial inspection copy is `build/release-validation/android-package/ISSDNative-validation.apk`, SHA-256 `8451e70eda9f63571c724336675ec8ae804baf709b1aa773936eabdf0e690ad5`. ZIP CRC validation passed. Each ABI contains `libSDL2.so`, `libc++_shared.so`, and `libmain.so`; individual hashes are in `android-apk-inspection.json`. No `.sfc` or `.smc` ROM is present. SDK `aapt dump badging` confirmed the application ID, version and API levels above (`android-apk-badging.log`). SDK `apksigner verify --verbose --print-certs` exited 0, confirming APK v2 signature with the Android Debug certificate (`android-apk-signature.log`). SDL's existing use of deprecated `ASensorManager_getInstance` produced a compiler warning on both ABIs.

Inspection of the initial APK found no assets and no `aot_boot_deny.txt`. Investigation confirmed no embedded Android equivalent: the current canonical deny file contains `00CF2A` and `80CF2A`, forcing the squad loader to use patched cartridge bytes for added teams. The native loader reads this set only from `SNESRECOMP_LLE_INTERP_TARGET_FILE`; Android had neither packaged the file nor published such a path. The native missing-file warning mentions the SPC700 handshake, but the actual current deny contents establish an added-team mod defect rather than proving a vanilla boot failure.

The Android packaging defect was corrected in `android/app/build.gradle` and `ISSDActivity.java`: a generated-assets Copy task takes the canonical deny file directly from `recomp/`, and Activity startup refreshes it in app-private storage and sets the native environment path **before** `super.onCreate` can start SDL's native thread. An explicitly supplied nonempty environment path is preserved. Extraction errors fail startup explicitly instead of silently running without the cartridge patch gate. Native gameplay code and declared version were unchanged.

After this packaging-only correction, an explicitly **incremental** `assembleRelease` against the already fresh validation staging passed in 49 seconds (16 tasks executed, 36 up-to-date; log `android-packaging-fix-build.log`). Final APK: `build/release-validation/android-package/ISSDNative-validation-fixed.apk`, SHA-256 `447b102efb50d978982f60ab07ac9a6416852db11e324aac0db946b5c54e7703`. The initial inspection APK remains preserved separately. Final ZIP CRC, both ABI library identities, absence of ROMs, manifest identity/version/API levels, and APK v2 debug signature passed verification. Every native-library hash matches the earlier fresh build. `assets/aot_boot_deny.txt` matches the canonical file byte-for-byte (SHA-256 `e788e05e29eab3890fcc18254fa8d51d112599798f129c8afe252ff29925bfd6`). Source inspection confirms extraction precedes SDL startup; Java compilation and release lint passed. Evidence: `android-fixed-inspection.json`, `android-fixed-signature.log`, `android-fixed-badging.log`.

No emulator or physical-device execution was performed. Asset and startup preparation are verified through build/artifact/source checks, while actual Android installation, first-run lifecycle, display, audio and input acceptance remain unverified.

Final review corrected the empty-environment edge case: after preserving a nonempty explicit path, `Os.setenv` now uses overwrite=true so an existing empty variable receives the extracted asset path. A second incremental build passed in 38 seconds (12 tasks executed, 40 up-to-date; `android-empty-env-fix-build.log`). **Final candidate**: `build/release-validation/android-package/ISSDNative-validation-final.apk`, SHA-256 `6d2d24f6009ce63dfa7617099cbc5978c5bf1d64f179c9265199add5436d542d`; the owned Gradle `app-release.apk` now contains this final candidate. Earlier initial and first-fix APK copies remain preserved for history. ZIP CRC, asset byte identity, unchanged native libraries, both ABIs, ROM absence, APK v2 signature, manifest identity/version/API levels, startup ordering, nonempty-path guard and empty-path overwrite checks passed. Final evidence: `android-final-inspection.json`, `android-final-signature.log`, `android-final-badging.log`; `git diff --check` exited 0 (`android-final-diff-check.log`). Runtime environment behavior is established by the source guard and overwrite flag, not a device execution claim.
