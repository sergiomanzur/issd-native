# Changelog

## v0.1.0-beta.1 - First Beta

ISSD Native is a passion project: a native recompilation and modernization effort for International Superstar Soccer Deluxe that preserves the original gameplay while making it comfortable to run on modern hardware.

This release does not include any ROM, cartridge data, copyrighted game assets, BIOS files, or commercial media. You must provide your own legally dumped cartridge image.

### Highlights

- First public beta release for Windows and Android.
- Complete native recompilation coverage for the original game code path.
- Playable menus, exhibition matches, cups, scenarios, training, and penalty shootout modes.
- Authentic baseline gameplay, animation timing, ball physics, player inertia, referee behavior, collision behavior, and tactical AI.
- Native C asset decompression and memory streaming paths for faster boot and runtime behavior.
- Native audio fast path for Konami SPC700 command handling, sound effects, music, and announcer voice triggering.
- SDL2 video, audio, keyboard, and gamepad support.
- Modern controller layout option alongside classic SNES-style controls.
- In-game overlay menu for presentation, audio, gameplay, and mod settings.
- True widescreen rendering support for 16:10, 16:9, and ultrawide viewports without stretching the SNES image.
- Internal resolution scaling, nearest/linear filtering, and CRT scanline presentation options.
- Save-state and quickload support.
- Mod stack support for roster, formation, kit, stadium, team photo, team plate, and HD tile replacement packs.
- Windows ROM selection through a native file picker.
- Windows mods folder selection through a native folder picker, plus `mods_dir` config support.
- Android ROM picker and mods folder picker integration through the platform file picker.
- Android touch overlay support for handheld play.
- Headless regression and screenshot/dump modes for automated validation.

### Packaging

- Windows artifact: `ISSDNative-v0.1.0-beta.1-windows-x64.zip`
- Android artifact: `ISSDNative-v0.1.0-beta.1-android.apk`

### Known Notes

- Windows and Android packages intentionally ship without a ROM.
- The Android APK is a beta/dev-style release build signed with the debug signing configuration for sideload testing.
- This release is intended for testing, preservation, and research by users who own the original cartridge.
