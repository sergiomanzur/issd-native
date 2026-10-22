# Steam Deck AppImage

Build on Linux x86-64 with CMake, Ninja, SDL2 development headers, patchelf,
SquashFS tools and appimagetool available. Use the Steam Linux Runtime 3.0
(sniper) SDK or a compatible toolchain. The packager rejects ELF dependencies
requiring glibc newer than 2.36; a recent distribution's SDL may exceed this.

```sh
cmake --preset steamos
cmake --build --preset steamos -j4
APPIMAGE_TOOL=/absolute/path/to/appimagetool bash tools/make_appimage.sh
```

The result is `dist/appimage/ISSDNative-x86_64.AppImage`. `BUILD_DIR`,
`OUTPUT_DIR`, `APPIMAGE_TOOL`, `APPIMAGE_RUNTIME` and `SDL_LIBRARY_DIR` are
optional environment overrides. `SDL_LIBRARY_DIR` supplies compatible shared
SDL/helper libraries when the build host's libraries are newer. Inspect the
payload without creating an image using `bash tools/make_appimage.sh --stage-only`.
Staging directories are retained for inspection. The final image is replaced
only after appimagetool succeeds. No ROM or user saves are packaged.

On Deck, mark the AppImage executable, add it to Steam as a non-Steam game,
and select the Gamepad controller layout. Choose ROM in the launcher and
supply your own supported USA cartridge dump. Keyboard, mouse, controller and
touch use the same launcher; full controller remapping remains in the in-game
Controls menu. If FUSE is unavailable, launch with `--appimage-extract-and-run`.

Writable state lives in `${XDG_DATA_HOME:-$HOME/.local/share}/issd-native`:
`issd_native.cfg`, `saves/` and `mods/`. `ISSD_DATA_DIR` overrides that directory.
Bundled mod assets seed missing files; existing user files are preserved.
Explicit `--config` and `--save-dir` arguments override defaults. Relative ROM
and state paths resolve from the caller's original directory.

Validation in this change includes Linux compilation, a generated AppImage,
ELF dependency checks, and a 900-frame no-FUSE headless boot. Physical Deck
Gaming Mode, suspend/resume and graphics/audio driver acceptance remain device
checks; a headless boot does not establish those properties.
