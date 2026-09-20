# Changelog

## v0.1.1b

### Highlights

- **Vertical Stripes Kit Support (`stripes: true`)**: Added native support for striped kit overlays in the mod system. In-game player sprites can now feature vertical stripes on their shirts using the SNES jersey detail table `$81:CE8A` overlay (e.g. CD Guadalajara / Chivas, CF Monterrey, Club Necaxa, Atlético San Luis, Atlante FC).
- **Away Kit & Change Strip Customization**: Fixed kit repainting in `patch_kit` to update both the Home kit pointer table (`$82:827A`) and Away kit pointer table (`$82:82D0`), ensuring teams playing as Away (P2, or P1 in away kit) wear their authentic customized uniforms on the pitch rather than falling back to original cartridge palettes.
- **Match State Team Resolution Fix**: Fixed team ID resolution in `issd_bridge.c` where RAM byte offsets `0x0DA0` and `0x0EA0` are properly scaled to 0-based team IDs, preventing mismatches in HUD rendering and match overrides.
- **Goalkeeper Sprite Integrity**: Fixed SNES VRAM sprite table corruption where field player hair attribute bits inadvertently corrupted goalkeeper composite head and glove animations.
- **Team Selection & Menu HUD Overlays**: Pixel-perfect alignment and positioning for upscaled flags in team selection grid cells, Handicap selection, Tonight's Game pre-match screen, Coin Toss minigame, and in-game live scoreboard HUD.
- **Liga MX y Expansión MX Mod Pack (Apertura 2026)**: Complete 36-team mod pack including all 18 Liga MX first-division clubs and 15 Liga de Expansión clubs (plus 3 historical clubs) updated to Apertura 2026, featuring:
  - Authentic flag pixel art and high-resolution squad photos.
  - Dynamically calculated SNES player attributes for all 720 players.
  - Club América with vibrant canary yellow (`#FFE600`) and navy blue.
  - CD Guadalajara (Chivas) with authentic red & white vertical stripes on the pitch, blue shorts, and white socks.
- **FIFA World Cup 2026 Mod Pack**: Official 32-nation mod pack for World Cup 2026 with up-to-date rosters, tactical formations, upscaled national flags, and custom team kits.
- **Android Touch Overlay Menu Navigation**: Fully enabled touchscreen navigation in the in-game overlay menu. Players can navigate menus using on-screen D-pad buttons (Up/Down/Left/Right), confirm/cancel with action buttons (A/X/Start and B/Y/Select), or tap directly on menu items to toggle settings.
- **Android ROM Picker Stability**: Resolved crash when selecting a ROM from the system file picker by waiting asynchronously for user selection and executing a clean process restart upon importing ROM/mods.

### Packaging

- Windows artifact: `ISSDNative-v0.1.1b-windows-x64.zip`
- Android APK: `ISSDNative-v0.1.1b-android.apk`
- Android ZIP package: `ISSDNative-v0.1.1b-android.zip`
- Mod Pack: `liga_mx_expansion.zip`
- Mod Pack: `world_cup_2026.zip`

## v0.1.0-beta.2 - Second Beta

### Highlights

- **Goalkeeper Sprite Integrity**: Fixed SNES VRAM sprite table corruption where field player hair attribute bits inadvertently corrupted goalkeeper composite head and glove animations.
- **Team Selection Menu Alignment**: Pixel-perfect alignment and positioning for upscaled national team flags in both grid cells and preview cards.
- **Custom Squad Photographs**: Integrated high-resolution squad photograph overrides supporting replacement nations with authentic official kit colors (e.g. Canada red/white, Ecuador yellow/blue).
- **Handicap & Tonight's Game Overlays**: Dynamic flag and name banner overlays on the Handicap selection screen and the pre-match Tonight's Game presentation screen.
- **Pre-Match Presentation Scene**: Seamless sky rendering and authentic typography for custom teams during the pre-match fly-in banner cutscene.
- **In-Game Scoreboard HUD**: Real-time HUD scoreboard support displaying custom national flags, country name plates, and match scores for both Player 1 and Player 2 squads.
- **FIFA World Cup 2026 Mod Pack**: Standalone official mod pack release including all 32 qualified nations, rosters, 4-2-3-1/4-3-3 formations, custom flags, and official kits.

### Packaging

- Windows artifact: `ISSDNative-v0.1.0-beta.2-windows-x64.zip`
- Android artifact: `ISSDNative-v0.1.0-beta.2-android.apk`
- Mod Pack artifact: `ISSDNative-WorldCup2026-ModPack.zip`

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
