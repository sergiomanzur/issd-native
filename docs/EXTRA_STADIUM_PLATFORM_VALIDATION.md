# Extra-stadium platform validation

The bounded stadium changes were rebuilt on Linux and Android on 2026-10-09. These were explicitly **incremental** rebuilds of the previously fresh, isolated validation build directories. Earlier staged APK copies were preserved. Version remains `0.4.0-beta.1` (Android code 6). No native gameplay edits were made by this validation task.

## Linux

From WSL Ubuntu 24.04 at the repository root:

```sh
cmake --build build/release-validation/linux -j 6
```

Exit 0; six Ninja steps completed, including current `issd_mod_rom.c`, `issd_mod.c`, `issd_menu.c`, `main.c` and linking. Toolchain remains CMake 3.28.3, Ninja 1.11.1, GNU C 13.3.0 and SDL 2.30.0. Log: `build/extra-stadium-validation/platform/linux-build.log`.

New staged executable: `build/extra-stadium-validation/platform/linux-stage/ISSDNative`; SHA-256 `84691648a1dec55c64c3dc28c76ce45362ddb88e4ba418746f5b6c844b7ba312`. Staged deny file matches current `recomp/aot_boot_deny.txt` byte identity (SHA-256 `a91900a039a1b3d92fb6a7806bb1be016c14827efdf5a528d5bdb8ee1af2e167`), including `05A3FB` and `85A3FB` stadium serializer mirrors alongside the squad-loader entries.

From the new staged directory, with its own empty mods/saves and fresh configuration:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./ISSDNative --headless 240 \
  --rom "/mnt/c/Users/sergi/Homestead/code/isssdeluxe-recomp/International Superstar Soccer Deluxe (USA).sfc" \
  --config isolated.cfg --save-dir saves --mods-dir mods
```

Exit 0; 240 frames and clean shutdown. Log: `build/extra-stadium-validation/platform/linux-smoke.log`. This is a dummy-device startup smoke, not an expanded-stadium gameplay assertion or SteamOS compatibility claim. The private ROM remains external to the staged files.

## Android

From repository `android/`:

```powershell
& C:/Users/sergi/.gradle/wrapper/dists/gradle-8.7-bin/bhs2wmbdwecv87pi65oeuq5iu/gradle-8.7/bin/gradle.bat --no-daemon --no-build-cache --init-script ../build/release-validation/android-fresh.init.gradle assembleRelease
```

Exit 0; build successful in 1m 33s, 52 actionable tasks (15 executed, 37 up-to-date). Both native ABI build tasks ran. Toolchain remains Gradle 8.7, JVM 21.0.12.1, Android Gradle Plugin 8.5.2, SDK 34, minimum SDK 26, NDK 26.2.11394342 and SDK CMake 3.22.1. Log: `build/extra-stadium-validation/platform/android-build.log`.

New inspection copy: `build/extra-stadium-validation/platform/ISSDNative-extra-stadium-validation.apk`; SHA-256 `a911f5b5d03a9e41dda74b7b632e3a966985ceef47597ece9b58a403f54b10a3`. ZIP CRC validation passed. `arm64-v8a` and `x86_64` each contain `libSDL2.so`, `libc++_shared.so` and current `libmain.so`; individual hashes are recorded in `android-inspection.json`. `assets/aot_boot_deny.txt` matches the current canonical file byte-for-byte, including both stadium serializer mirrors. No `.sfc` or `.smc` ROM is packaged. The Activity's previously verified private-file extraction and environment publication run before SDL startup, so the updated generated asset follows that same path.

SDK `aapt dump badging` confirmed unchanged ID `com.issdnative`, version name `0.4.0-beta.1`, code 6 and API levels 26/34 (`android-badging.log`). SDK `apksigner verify --verbose --print-certs` exited 0 and confirmed the APK v2 Android Debug signature (`android-signature.log`). All Android inspection logs are under `build/extra-stadium-validation/platform/`.

No emulator or physical-device execution was performed. Android runtime, first-install lifecycle, expanded-stadium visuals, real audio and input are not claimed by these build/artifact checks. Windows gameplay and expanded-stadium acceptance are recorded separately by the main task.
