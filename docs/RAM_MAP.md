# ISSD Native — Comprehensive RAM & Memory Map

This document provides a categorized directory of all identified RAM variables, mirrors, buffers, and hardware registers in *International Superstar Soccer Deluxe* (SNES USA).

---

## 1. Direct Page ($00:0000 – $00:00FF)

Direct Page instructions execute 1 cycle faster on the 65816 CPU and form the high-frequency scratchpad for the engine:

| Address | Variable / Identifier | Type | Subsystem / Description |
| :--- | :--- | :---: | :--- |
| `$000008` | `RAM_ISSD_DP_TRAMPOLINE_ENTRY` | Code | Entry point of synthesized Direct Page `MVN` palette copy trampoline |
| `$000009` | `RAM_ISSD_DP_TRAMPOLINE_DEST` | uint16 | Destination offset in Bank `$7E` for palette transfer |
| `$00000C` | `RAM_ISSD_DP_TRAMPOLINE_DST_BANK` | uint8 | Destination bank byte (always `$7E`) |
| `$00000D` | `RAM_ISSD_DP_TRAMPOLINE_SRC_BANK` | uint8 | Source bank byte (usually `$89`) |
| `$000010` | `RAM_ISSD_Global_Joypad1Lo` | uint8 | Raw Joypad 1 data low byte |
| `$000011` | `RAM_ISSD_Global_Joypad1Hi` | uint8 | Raw Joypad 1 data high byte |
| `$000012` | `RAM_ISSD_Global_Joypad2Lo` | uint8 | Raw Joypad 2 data low byte |
| `$000013` | `RAM_ISSD_Global_Joypad2Hi` | uint8 | Raw Joypad 2 data high byte |
| `$000014` | `RAM_ISSD_Global_Joypad4Lo` | uint8 | Raw Joypad 4 data low byte (multitap) |
| `$000020` | `RAM_ISSD_Global_HeldButtonsLoP1` | uint16 | Bitmask of currently held buttons on Player 1 controller |
| `$000022` | `RAM_ISSD_Global_HeldButtonsLoP2` | uint16 | Bitmask of currently held buttons on Player 2 controller |
| `$000028` | `RAM_ISSD_Global_PressedButtonsLoP1` | uint16 | Bitmask of newly pressed buttons this frame on P1 (edge-triggered) |
| `$00002A` | `RAM_ISSD_Global_PressedButtonsLoP2` | uint16 | Bitmask of newly pressed buttons this frame on P2 (edge-triggered) |
| `$000032` | `RAM_ISSD_Global_CurrentGameMode1` | uint8 | Primary game mode (0=Boot, 1=Title, 3=Demo, 6=Menu/Match) |
| `$000034` | `RAM_ISSD_NMI_BUSY_FLAG` | uint16 | NMI handler execution re-entrancy lock |
| `$00003C` | `RAM_ISSD_NMI_FRAME_COUNTER` | uint16 | Increments every V-Blank tick |
| `$00003E` | `RAM_ISSD_INIDISP_SHADOW` | uint8 | Shadow register for PPU `$2100` (brightness / forced blank) |
| `$000040` | `RAM_ISSD_MOSAIC_SHADOW` | uint8 | Shadow register for PPU `$2106` (mosaic effect) |
| `$000070` | `RAM_ISSD_Global_CurrentGameMode2` | uint8 | Secondary sub-mode (0=Idle, 2=Menu Nav, 4=Menu Init, 8=Match, 12=Match Init) |
| `$000084` | `RAM_ISSD_Palette_DestOffsetBase` | uint16 | Base offset in `$7E:2C00` for active palette animation slot |
| `$000086` | `RAM_ISSD_Intro_SceneTimer` | uint16 | Countdown timer for intro cutscene frames |
| `$0000E8` | `RAM_ISSD_Menu_OptionCursor` | uint16 | Active cursor position in menus |
| `$0000F8` | `RAM_ISSD_Global_PressedButtonsLo` | uint16 | Global active player edge-triggered buttons |

---

## 2. Low WRAM & OAM Mirrors ($00:0100 – $00:1FFF)

| Address | Variable / Identifier | Type | Subsystem / Description |
| :--- | :--- | :---: | :--- |
| `$000100` - `$0001FF` | `RAM_ISSD_CPU_Stack` | Buffer | 65816 hardware stack (default SP = `$01AF`) |
| `$0001E0` - `$0003DF` | `RAM_ISSD_Global_OAMBuffer` | Buffer | 512-byte mirror for 128 hardware sprites (X, Y, tile, attributes) |
| `$0003E0` - `$0003FF` | `RAM_ISSD_Global_OAMHiBuffer` | Buffer | 32-byte mirror for sprite size bits and 9th X-coordinate bit |
| `$000DA0` | `RAM_ISSD_Global_Player1Team` | uint16 | Team ID selected by Player 1 (e.g., Italy = `$0000`, Brazil = `$003C`) |
| `$000DA2` | `RAM_ISSD_Global_Player1Score` | uint16 | Goals scored by Player 1 team in current match |
| `$000DE2` | `RAM_ISSD_Global_GoalieSkillForP1` | uint16 | Goalkeeper skill attribute rating for Player 1 |
| `$000EA0` | `RAM_ISSD_Global_Player2Team` | uint16 | Team ID selected by Player 2 |
| `$000EA2` | `RAM_ISSD_Global_Player2Score` | uint16 | Goals scored by Player 2 team in current match |
| `$000EE2` | `RAM_ISSD_Global_GoalieSkillForP2` | uint16 | Goalkeeper skill attribute rating for Player 2 |
| `$0011E6` | `RAM_ISSD_Global_NumberOfPlayersSetting` | uint16 | Active human controllers configuration (1P vs COM, 1P vs 2P, etc.) |
| `$0013A0` | `RAM_ISSD_Global_Layer1XPosLo` | uint16 | Background Layer 1 X horizontal scroll offset (pitch turf) |
| `$0013B0` | `RAM_ISSD_Global_Layer1YPosLo` | uint16 | Background Layer 1 Y vertical scroll offset |
| `$0013C0` | `RAM_ISSD_Global_Layer2XPosLo` | uint16 | Background Layer 2 X horizontal scroll offset (stadium/stands) |
| `$0013D0` | `RAM_ISSD_Global_Layer2YPosLo` | uint16 | Background Layer 2 Y vertical scroll offset |
| `$0013E0` | `RAM_ISSD_Global_Layer3XPosLo` | uint16 | Background Layer 3 X horizontal scroll offset (HUD/score) |
| `$0013F0` | `RAM_ISSD_Global_Layer3YPosLo` | uint16 | Background Layer 3 Y vertical scroll offset |
| `$0014F0` | `RAM_ISSD_ChallengeMode_RemainingSeconds` | uint8 | Countdown clock seconds for Challenge Mode scenarios |
| `$0014F1` | `RAM_ISSD_ChallengeMode_RemainingMinutes` | uint8 | Countdown clock minutes for Challenge Mode scenarios |
| `$00156C` | `RAM_ISSD_HandicapScreen_GoalieConditionForP1` | uint16 | Goalkeeper condition on Handicap setup screen |
| `$001570` | `RAM_ISSD_HandicapScreen_NumPlayersP1` | uint16 | Number of players on field for Player 1 (Handicap: 7..11) |
| `$001572` | `RAM_ISSD_HandicapScreen_NumPlayersP2` | uint16 | Number of players on field for Player 2 (Handicap: 7..11) |
| `$0016D0` | `RAM_ISSD_Global_GameTimerSeconds` | uint8 | Match elapsed time: seconds |
| `$0016D1` | `RAM_ISSD_Global_GameTimerMinutes` | uint8 | Match elapsed time: minutes |
| `$0016D2` | `RAM_ISSD_Global_DisplayedSecondsOnTimer` | uint8 | Clock display seconds value rendered on HUD |
| `$0016D3` | `RAM_ISSD_Global_DisplayedMinutesOnTimer` | uint8 | Clock display minutes value rendered on HUD |
| `$0019A2` | `RAM_ISSD_MainMenu_ActiveOptionIndex` | uint16 | Selected option index on Main Menu (0..7) |
| `$0019A6` | `RAM_ISSD_MainMenus_CurrentSplashScreen` | uint16 | Active full-screen splash graphic ID |
| `$001CC0` | `RAM_ISSD_InGame_CurrentPlayerNameTilemap` | Buffer | VRAM staging buffer for active player name banner |
| `$001E40` | `RAM_ISSD_WRAM_MVN_TRAMPOLINE` | Code | Dynamic `MVN` block-transfer routine for LZ decompression |
| `$001E4C` | `RAM_ISSD_InGame_CurrentWeather` | uint16 | Weather setting (0=Snow, 1=Fine, 2=Rain) |
| `$001E50` | `RAM_ISSD_InGame_CurrentFoulSetting` | uint16 | Foul sensitivity (0=Off, 1=Normal, 2=Strict) |
| `$001E52` | `RAM_ISSD_InGame_CurrentYellowCardSetting` | uint16 | Yellow card persistence rule |
| `$001E54` | `RAM_ISSD_InGame_CurrentGameLevel` | uint16 | Match difficulty rating (0=Easy, 1=Medium, 2=Hard) |
| `$001E58` | `RAM_ISSD_InGame_CurrentReferee` | uint16 | Active match referee personality |
| `$001E5A` | `RAM_ISSD_InGame_CurrentGameTimeSetting` | uint16 | Match period duration (3, 5, 7 minutes) |
| `$001E5C` | `RAM_ISSD_InGame_CurrentTime` | uint16 | Time of match (0=Day, 1=Dusk, 2=Night) |
| `$001EA4` | `RAM_ISSD_Global_BGModeAndTileSizeSetting` | uint8 | Shadow for PPU `$2105` (BG Mode 1..7 and tile sizes) |
| `$001FA2` | `RAM_ISSD_MainMenu_SelectedStadium` | uint16 | Stadium selection (0..7) |

---

## 3. High WRAM ($7E:2000 – $7F:FFFF)

| Address | Variable / Identifier | Type | Subsystem / Description |
| :--- | :--- | :---: | :--- |
| `$7E:2000` - `$7E:2BFF` | `RAM_ISSD_DecompressBuffer` | Buffer | Staging buffer for decompressed VRAM tile graphics |
| `$7E:2C00` - `$7E:2DFF` | `RAM_ISSD_Global_PaletteMirror` | Buffer | 512-byte CGRAM color mirror (256 15-bit BGR555 words) |
| `$7E:3200` - `$7E:35FF` | `RAM_ISSD_Global_DMAVRAMUploadTable` | Table | DMA transfer queue dispatched during vertical blanking |
| `$7E:D472` | `RAM_ISSD_TeamSelectScreen_NumberOfPages` | uint16 | Total team selection confederation pages |
| `$7F:8000` - `$7F:9FFF` | `RAM_ISSD_METATILE_DEFS_BG1` | Buffer | 32x32 Metatile definitions for pitch turf (16 tiles $\times$ 2 bytes each) |
| `$7F:A000` - `$7F:BFFF` | `RAM_ISSD_METATILE_DEFS_BG2` | Buffer | 32x32 Metatile definitions for stadium stands and goalposts |
| `$7F:D000` - `$7F:DFFF` | `RAM_ISSD_WORLD_METATILE_MAP_BG1` | Buffer | World-space 2D byte ID grid for pitch turf |
| `$7F:E000` - `$7F:EFFF` | `RAM_ISSD_WORLD_METATILE_MAP_BG2` | Buffer | World-space 2D byte ID grid for stadium geometry |
| `$7F:FFCC` | `RAM_ISSD_STADIUM_STRIDE` | uint16 | Horizontal stride byte width for active stadium metatile map |
