# ISS Deluxe Comprehensive Routine & Jump Table Map

This document provides complete semantic documentation for every reverse-engineered routine, indirect jump table, and dispatch state machine in *International Superstar Soccer Deluxe*.

## Subsystem Jump Table Summary

| Bank | LoROM | Jump Tables | Target Routines | Subsystem Description |
|:---|:---|:---:|:---:|:---|
| **$80** | `bank00.cfg` | 1 | 44 | Bank $80: Master Game Mode & Screen Dispatcher |
| **$83** | `bank03.cfg` | 6 | 56 | Bank $83: Ball Physics, Player Collision & Goalkeeper Saves |
| **$84** | `bank04.cfg` | 7 | 47 | Bank $84: Player Sprite Composition & Animation Frame Sequencing |
| **$85** | `bank05.cfg` | 9 | 70 | Bank $85: Referee Decision Engine, Foul & Card Logic |
| **$86** | `bank06.cfg` | 74 | 317 | Bank $86: Match Gameplay Loop & Player Action State Machines |
| **$8A** | `bank0a.cfg` | 22 | 156 | Bank $8A: AI Tactical Decision Trees, Team Formations & Defensive Pressing |
| **$8B** | `bank0b.cfg` | 11 | 132 | Bank $8B: Pitch Geometry, Metatile Streamer & Camera Movement |
| **$8C** | `bank0c.cfg` | 7 | 44 | Bank $8C: Scenario Match Situations & Tournament Progress |
| **$A4** | `bank24.cfg` | 16 | 146 | Bank $A4: UI Screens, Team Selection, Tactics Board & Passwords |
| **Total** | | **153** | **1012** | **Complete Engine Matrix** |

---

## Bank $80: Master Game Mode & Screen Dispatcher

### Table 1: `CODE_80A8EF` -> `DATA_80A476` ($A8F7 -> $A476)

- **Subsystem Purpose:** Master Game Mode State Dispatcher (Boot, Logos, Title, In-Game)
- **Dispatch Site:** `$80:A8F7` (`JMP.w (DATA_80A476,x)`)
- **Table Base:** `$80:A476` (44 target routines)
- **Index Selector:** `Unknown`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$80A417` | `CODE_80A417` | 00 - ??? |
| `1` | `$80A4CE` | `CODE_80A4CE` | 01 - ??? |
| `2` | `$80A51F` | `CODE_80A51F` | 02 - ??? |
| `3` | `$80A572` | `CODE_80A572` | 03 - ??? |
| `4` | `$80A5E1` | `CODE_80A5E1` | 04 - ??? |
| `5` | `$80A6B6` | `CODE_80A6B6` | 05 - ??? |
| `6` | `$80A753` | `CODE_80A753` | 06 - ??? |
| `7` | `$80A811` | `CODE_80A811` | 07 - ??? |
| `8` | `$80A861` | `CODE_80A861` | 08 - ??? |
| `9` | `$80A879` | `CODE_80A879` | 09 - ??? |
| `10` | `$80A417` | `CODE_80A417` | 0A - ??? |
| `11` | `$80A417` | `CODE_80A417` | 0B - ??? |
| `12` | `$80A417` | `CODE_80A417` | 0C - ??? |
| `13` | `$80A417` | `CODE_80A417` | 0D - ??? |
| `14` | `$80A417` | `CODE_80A417` | 0E - ??? |
| `15` | `$80A417` | `CODE_80A417` | 0F - ??? |
| `16` | `$80A417` | `CODE_80A417` | 10 - ??? |
| `17` | `$80A900` | `CODE_80A900` | 11 - ??? |
| `18` | `$80A8FA` | `CODE_80A8FA` | 12 - ??? |
| `19` | `$80A9B4` | `CODE_80A9B4` | 13 - ??? |
| `20` | `$80A9B4` | `CODE_80A9B4` | 14 - ??? |
| `21` | `$80AA88` | `CODE_80AA88` | 15 - ??? |
| `22` | `$80AAA0` | `CODE_80AAA0` | 16 - ??? |
| `23` | `$80AAAC` | `CODE_80AAAC` | 17 - ??? |
| `24` | `$80AACA` | `CODE_80AACA` | 18 - ??? |
| `25` | `$80AAD2` | `CODE_80AAD2` | 19 - ??? |
| `26` | `$80AB2B` | `CODE_80AB2B` | 1A - ??? |
| `27` | `$80ABEB` | `CODE_80ABEB` | 1B - ??? |
| `28` | `$80ADF9` | `CODE_80ADF9` | 1C - ??? |
| `29` | `$80AC8F` | `CODE_80AC8F` | 1D - ??? |
| `30` | `$80AD31` | `CODE_80AD31` | 1E - ??? |
| `31` | `$80AEC9` | `CODE_80AEC9` | 1F - ??? |
| `32` | `$80AEE2` | `CODE_80AEE2` | 20 - ??? |
| `33` | `$80AEF0` | `CODE_80AEF0` | 21 - ??? |
| `34` | `$80AEEA` | `CODE_80AEEA` | 22 - ??? |
| `35` | `$80AAFA` | `CODE_80AAFA` | 23 - ??? |
| `36` | `$80AB02` | `CODE_80AB02` | 24 - ??? |
| `37` | `$80AA45` | `CODE_80AA45` | 25 - ??? |
| `38` | `$80AEFB` | `CODE_80AEFB` | 26 - ??? |
| `39` | `$80AA40` | `CODE_80AA40` | 27 - ??? |
| `40` | `$80A947` | `CODE_80A947` | 28 - ??? |
| `41` | `$80A913` | `CODE_80A913` | 29 - ??? |
| `42` | `$80A91E` | `CODE_80A91E` | 2A - ??? |
| `43` | `$80A932` | `CODE_80A932` | 2B - ??? |

## Bank $83: Ball Physics, Player Collision & Goalkeeper Saves

### Table 1: `CODE_83CF7D` -> `DATA_83CF86` ($CF83 -> $CF86)

- **Subsystem Purpose:** Match Play Phase State Machine (Kickoff, Freeplay, Fouls, Corner, GK)
- **Dispatch Site:** `$83:CF83` (`JMP.w (DATA_83CF86,x)`)
- **Table Base:** `$83:CF86` (15 target routines)
- **Index Selector:** `LDA.w $00C0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$83CFAA` | `CODE_83CFAA` | State handler #0 for DATA_83CF86 |
| `1` | `$83CFAA` | `CODE_83CFAA` | State handler #1 for DATA_83CF86 |
| `2` | `$83CFAA` | `CODE_83CFAA` | State handler #2 for DATA_83CF86 |
| `3` | `$83CFA4` | `CODE_83CFA4` | State handler #3 for DATA_83CF86 |
| `4` | `$83CFAA` | `CODE_83CFAA` | State handler #4 for DATA_83CF86 |
| `5` | `$83CFAA` | `CODE_83CFAA` | State handler #5 for DATA_83CF86 |
| `6` | `$83CFAA` | `CODE_83CFAA` | State handler #6 for DATA_83CF86 |
| `7` | `$83CFAA` | `CODE_83CFAA` | State handler #7 for DATA_83CF86 |
| `8` | `$83CFAA` | `CODE_83CFAA` | State handler #8 for DATA_83CF86 |
| `9` | `$83CFAA` | `CODE_83CFAA` | State handler #9 for DATA_83CF86 |
| `10` | `$83CFAA` | `CODE_83CFAA` | State handler #10 for DATA_83CF86 |
| `11` | `$83CFAA` | `CODE_83CFAA` | State handler #11 for DATA_83CF86 |
| `12` | `$83CFAA` | `CODE_83CFAA` | State handler #12 for DATA_83CF86 |
| `13` | `$83CFAA` | `CODE_83CFAA` | State handler #13 for DATA_83CF86 |
| `14` | `$83CFAA` | `CODE_83CFAA` | State handler #14 for DATA_83CF86 |

### Table 2: `CODE_83D774` -> `DATA_83D77F` ($D77C -> $D77F)

- **Subsystem Purpose:** Match Mode Collision & Ball Physics Handler
- **Dispatch Site:** `$83:D77C` (`JMP.w (DATA_83D77F,x)`)
- **Table Base:** `$83:D77F` (8 target routines)
- **Index Selector:** `LDA.b $32`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$83D78F` | `CODE_83D78F` | State handler #0 for DATA_83D77F |
| `1` | `$83D82C` | `CODE_83D82C` | State handler #1 for DATA_83D77F |
| `2` | `$83D881` | `CODE_83D881` | State handler #2 for DATA_83D77F |
| `3` | `$83D8A8` | `CODE_83D8A8` | State handler #3 for DATA_83D77F |
| `4` | `$83D8A9` | `CODE_83D8A9` | State handler #4 for DATA_83D77F |
| `5` | `$83D8E6` | `CODE_83D8E6` | State handler #5 for DATA_83D77F |
| `6` | `$83D910` | `CODE_83D910` | State handler #6 for DATA_83D77F |
| `7` | `$83D8A8` | `CODE_83D8A8` | State handler #7 for DATA_83D77F |

### Table 3: `CODE_83D99D` -> `DATA_83D9A5` ($D9A2 -> $D9A5)

- **Subsystem Purpose:** Ball Trajectory & Woodwork/Net Interaction Handler
- **Dispatch Site:** `$83:D9A2` (`JMP.w (DATA_83D9A5,x)`)
- **Table Base:** `$83:D9A5` (6 target routines)
- **Index Selector:** `LDA.b $2E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$83D9B1` | `CODE_83D9B1` | State handler #0 for DATA_83D9A5 |
| `1` | `$83D9C7` | `CODE_83D9C7` | State handler #1 for DATA_83D9A5 |
| `2` | `$83DA38` | `CODE_83DA38` | State handler #2 for DATA_83D9A5 |
| `3` | `$83DA98` | `CODE_83DA98` | State handler #3 for DATA_83D9A5 |
| `4` | `$83DAD1` | `CODE_83DAD1` | State handler #4 for DATA_83D9A5 |
| `5` | `$83DAED` | `CODE_83DAED` | State handler #5 for DATA_83D9A5 |

### Table 4: `CODE_83DCA2` -> `DATA_83DCAA` ($DCA7 -> $DCAA)

- **Subsystem Purpose:** Goalkeeper Dive/Save & Catch Animation State Machine
- **Dispatch Site:** `$83:DCA7` (`JMP.w (DATA_83DCAA,x)`)
- **Table Base:** `$83:DCAA` (3 target routines)
- **Index Selector:** `LDA.w $14A8`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$83DCE6` | `CODE_83DCE6` | State handler #0 for DATA_83DCAA |
| `1` | `$83DD05` | `CODE_83DD05` | State handler #1 for DATA_83DCAA |
| `2` | `$83DD6E` | `CODE_83DD6E` | State handler #2 for DATA_83DCAA |

### Table 5: `CODE_83DD99` -> `DATA_83DDA1` ($DD9E -> $DDA1)

- **Subsystem Purpose:** Goalkeeper Dive/Save & Catch Animation State Machine
- **Dispatch Site:** `$83:DD9E` (`JMP.w (DATA_83DDA1,x)`)
- **Table Base:** `$83:DDA1` (9 target routines)
- **Index Selector:** `LDY.w #$1500`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$83DDB3` | `CODE_83DDB3` | State handler #0 for DATA_83DDA1 |
| `1` | `$83DDC8` | `CODE_83DDC8` | State handler #1 for DATA_83DDA1 |
| `2` | `$83DE20` | `CODE_83DE20` | State handler #2 for DATA_83DDA1 |
| `3` | `$83DE2A` | `CODE_83DE2A` | State handler #3 for DATA_83DDA1 |
| `4` | `$83DE88` | `CODE_83DE88` | State handler #4 for DATA_83DDA1 |
| `5` | `$83DE92` | `CODE_83DE92` | State handler #5 for DATA_83DDA1 |
| `6` | `$83DE99` | `CODE_83DE99` | State handler #6 for DATA_83DDA1 |
| `7` | `$83DEA5` | `CODE_83DEA5` | State handler #7 for DATA_83DDA1 |
| `8` | `$83DEAC` | `CODE_83DEAC` | State handler #8 for DATA_83DDA1 |

### Table 6: `CODE_83F36D` -> `DATA_83F375` ($F372 -> $F375)

- **Subsystem Purpose:** Match Play Phase State Machine (Kickoff, Freeplay, Fouls, Corner, GK)
- **Dispatch Site:** `$83:F372` (`JMP.w (DATA_83F375,x)`)
- **Table Base:** `$83:F375` (15 target routines)
- **Index Selector:** `LDA.w $00C0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$83F3B2` | `CODE_83F3B2` | State handler #0 for DATA_83F375 |
| `1` | `$83F3B2` | `CODE_83F3B2` | State handler #1 for DATA_83F375 |
| `2` | `$83F393` | `CODE_83F393` | State handler #2 for DATA_83F375 |
| `3` | `$83F393` | `CODE_83F393` | State handler #3 for DATA_83F375 |
| `4` | `$83F393` | `CODE_83F393` | State handler #4 for DATA_83F375 |
| `5` | `$83F393` | `CODE_83F393` | State handler #5 for DATA_83F375 |
| `6` | `$83F393` | `CODE_83F393` | State handler #6 for DATA_83F375 |
| `7` | `$83F3B2` | `CODE_83F3B2` | State handler #7 for DATA_83F375 |
| `8` | `$83F3B2` | `CODE_83F3B2` | State handler #8 for DATA_83F375 |
| `9` | `$83F393` | `CODE_83F393` | State handler #9 for DATA_83F375 |
| `10` | `$83F3B2` | `CODE_83F3B2` | State handler #10 for DATA_83F375 |
| `11` | `$83F3B2` | `CODE_83F3B2` | State handler #11 for DATA_83F375 |
| `12` | `$83F3B2` | `CODE_83F3B2` | State handler #12 for DATA_83F375 |
| `13` | `$83F3B2` | `CODE_83F3B2` | State handler #13 for DATA_83F375 |
| `14` | `$83F3B2` | `CODE_83F3B2` | State handler #14 for DATA_83F375 |

## Bank $84: Player Sprite Composition & Animation Frame Sequencing

### Table 1: `CODE_84A346` -> `DATA_84A35F` ($A35C -> $A35F)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (7 states)
- **Dispatch Site:** `$84:A35C` (`JMP.w (DATA_84A35F,x)`)
- **Table Base:** `$84:A35F` (7 target routines)
- **Index Selector:** `LDA.b $78`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84A430` | `CODE_84A430` | State handler #0 for DATA_84A35F |
| `1` | `$84A3AB` | `CODE_84A3AB` | State handler #1 for DATA_84A35F |
| `2` | `$84A36D` | `CODE_84A36D` | State handler #2 for DATA_84A35F |
| `3` | `$84A3B5` | `CODE_84A3B5` | State handler #3 for DATA_84A35F |
| `4` | `$84A736` | `CODE_84A736` | State handler #4 for DATA_84A35F |
| `5` | `$84A3EB` | `CODE_84A3EB` | State handler #5 for DATA_84A35F |
| `6` | `$84A7C3` | `CODE_84A7C3` | State handler #6 for DATA_84A35F |

### Table 2: `CODE_84B37B` -> `DATA_84B3A0` ($B39D -> $B3A0)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (4 states)
- **Dispatch Site:** `$84:B39D` (`JMP.w (DATA_84B3A0,x)`)
- **Table Base:** `$84:B3A0` (4 target routines)
- **Index Selector:** `LDA.w DATA_81C5A6,y`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84B3D5` | `CODE_84B3D5` | State handler #0 for DATA_84B3A0 |
| `1` | `$84B42C` | `CODE_84B42C` | State handler #1 for DATA_84B3A0 |
| `2` | `$84B56B` | `CODE_84B56B` | State handler #2 for DATA_84B3A0 |
| `3` | `$84B47B` | `CODE_84B47B` | State handler #3 for DATA_84B3A0 |

### Table 3: `CODE_84D845` -> `DATA_84D84D` ($D84A -> $D84D)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (9 states)
- **Dispatch Site:** `$84:D84A` (`JMP.w (DATA_84D84D,x)`)
- **Table Base:** `$84:D84D` (9 target routines)
- **Index Selector:** `LDA.w $12F0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84D942` | `CODE_84D942` | State handler #0 for DATA_84D84D |
| `1` | `$84D93B` | `CODE_84D93B` | State handler #1 for DATA_84D84D |
| `2` | `$84D982` | `CODE_84D982` | State handler #2 for DATA_84D84D |
| `3` | `$84D917` | `CODE_84D917` | State handler #3 for DATA_84D84D |
| `4` | `$84D860` | `CODE_84D860` | State handler #4 for DATA_84D84D |
| `5` | `$84D864` | `CODE_84D864` | State handler #5 for DATA_84D84D |
| `6` | `$84D85F` | `CODE_84D85F` | State handler #6 for DATA_84D84D |
| `7` | `$84D88C` | `CODE_84D88C` | State handler #7 for DATA_84D84D |
| `8` | `$84D98C` | `CODE_84D98C` | State handler #8 for DATA_84D84D |

### Table 4: `CODE_84D8AC` -> `DATA_84D8C2` ($D8BF -> $D8C2)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (9 states)
- **Dispatch Site:** `$84:D8BF` (`JMP.w (DATA_84D8C2,x)`)
- **Table Base:** `$84:D8C2` (9 target routines)
- **Index Selector:** `LDA.w $12F0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84D942` | `CODE_84D942` | State handler #0 for DATA_84D8C2 |
| `1` | `$84D93B` | `CODE_84D93B` | State handler #1 for DATA_84D8C2 |
| `2` | `$84D982` | `CODE_84D982` | State handler #2 for DATA_84D8C2 |
| `3` | `$84D917` | `CODE_84D917` | State handler #3 for DATA_84D8C2 |
| `4` | `$84D8D7` | `CODE_84D8D7` | State handler #4 for DATA_84D8C2 |
| `5` | `$84D8F5` | `CODE_84D8F5` | State handler #5 for DATA_84D8C2 |
| `6` | `$84D8ED` | `CODE_84D8ED` | State handler #6 for DATA_84D8C2 |
| `7` | `$84D8E0` | `CODE_84D8E0` | State handler #7 for DATA_84D8C2 |
| `8` | `$84D98C` | `CODE_84D98C` | State handler #8 for DATA_84D8C2 |

### Table 5: `CODE_84F802` -> `DATA_84F809` ($F806 -> $F809)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (3 states)
- **Dispatch Site:** `$84:F806` (`JMP.w (DATA_84F809,x)`)
- **Table Base:** `$84:F809` (3 target routines)
- **Index Selector:** `LDA.b $32`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84F813` | `CODE_84F813` | State handler #0 for DATA_84F809 |
| `1` | `$84F820` | `CODE_84F820` | State handler #1 for DATA_84F809 |
| `2` | `$84F88F` | `CODE_84F88F` | State handler #2 for DATA_84F809 |

### Table 6: `CODE_84F931` -> `DATA_84F938` ($F935 -> $F938)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (9 states)
- **Dispatch Site:** `$84:F935` (`JMP.w (DATA_84F938,x)`)
- **Table Base:** `$84:F938` (9 target routines)
- **Index Selector:** `LDA.b $32`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84F94A` | `CODE_84F94A` | State handler #0 for DATA_84F938 |
| `1` | `$84F979` | `CODE_84F979` | State handler #1 for DATA_84F938 |
| `2` | `$84F9F7` | `CODE_84F9F7` | State handler #2 for DATA_84F938 |
| `3` | `$84FA21` | `CODE_84FA21` | State handler #3 for DATA_84F938 |
| `4` | `$84FA87` | `CODE_84FA87` | State handler #4 for DATA_84F938 |
| `5` | `$84FACF` | `CODE_84FACF` | State handler #5 for DATA_84F938 |
| `6` | `$84FB1B` | `CODE_84FB1B` | State handler #6 for DATA_84F938 |
| `7` | `$84FB31` | `CODE_84FB31` | State handler #7 for DATA_84F938 |
| `8` | `$84FB58` | `CODE_84FB58` | State handler #8 for DATA_84F938 |

### Table 7: `CODE_84FC8B` -> `DATA_84FC92` ($FC8F -> $FC92)

- **Subsystem Purpose:** Player Animation Frame Sequencer & Sprite Composer (6 states)
- **Dispatch Site:** `$84:FC8F` (`JMP.w (DATA_84FC92,x)`)
- **Table Base:** `$84:FC92` (6 target routines)
- **Index Selector:** `LDA.b $32`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$84FC9E` | `CODE_84FC9E` | State handler #0 for DATA_84FC92 |
| `1` | `$84FCB8` | `CODE_84FCB8` | State handler #1 for DATA_84FC92 |
| `2` | `$84FD0D` | `CODE_84FD0D` | State handler #2 for DATA_84FC92 |
| `3` | `$84FD4B` | `CODE_84FD4B` | State handler #3 for DATA_84FC92 |
| `4` | `$84FD97` | `CODE_84FD97` | State handler #4 for DATA_84FC92 |
| `5` | `$84FE2A` | `CODE_84FE2A` | State handler #5 for DATA_84FC92 |

## Bank $85: Referee Decision Engine, Foul & Card Logic

### Table 1: `CODE_85828B` -> `DATA_858292` ($828F -> $8292)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (2 states)
- **Dispatch Site:** `$85:828F` (`JMP.w (DATA_858292,x)`)
- **Table Base:** `$85:8292` (2 target routines)
- **Index Selector:** `LDA.b $64`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$858296` | `CODE_858296` | State handler #0 for DATA_858292 |
| `1` | `$8582F2` | `CODE_8582F2` | State handler #1 for DATA_858292 |

### Table 2: `CODE_858354` -> `DATA_85835B` ($8358 -> $835B)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (7 states)
- **Dispatch Site:** `$85:8358` (`JMP.w (DATA_85835B,x)`)
- **Table Base:** `$85:835B` (7 target routines)
- **Index Selector:** `LDA.b $62`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$858369` | `CODE_858369` | State handler #0 for DATA_85835B |
| `1` | `$858371` | `CODE_858371` | State handler #1 for DATA_85835B |
| `2` | `$858398` | `CODE_858398` | State handler #2 for DATA_85835B |
| `3` | `$8583BE` | `CODE_8583BE` | State handler #3 for DATA_85835B |
| `4` | `$85837A` | `CODE_85837A` | State handler #4 for DATA_85835B |
| `5` | `$85839E` | `CODE_85839E` | State handler #5 for DATA_85835B |
| `6` | `$8583C4` | `CODE_8583C4` | State handler #6 for DATA_85835B |

### Table 3: `CODE_85ACC4` -> `DATA_85ACD0` ($ACCD -> $ACD0)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (13 states)
- **Dispatch Site:** `$85:ACCD` (`JMP.w (DATA_85ACD0,x)`)
- **Table Base:** `$85:ACD0` (13 target routines)
- **Index Selector:** `LDA.w #$FFFF`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85AD5B` | `CODE_85AD5B` | State handler #0 for DATA_85ACD0 |
| `1` | `$85ADBE` | `CODE_85ADBE` | State handler #1 for DATA_85ACD0 |
| `2` | `$85AE18` | `CODE_85AE18` | State handler #2 for DATA_85ACD0 |
| `3` | `$85AE18` | `CODE_85AE18` | State handler #3 for DATA_85ACD0 |
| `4` | `$85ACEA` | `CODE_85ACEA` | State handler #4 for DATA_85ACD0 |
| `5` | `$85ACF7` | `CODE_85ACF7` | State handler #5 for DATA_85ACD0 |
| `6` | `$85AE19` | `CODE_85AE19` | State handler #6 for DATA_85ACD0 |
| `7` | `$85AE50` | `CODE_85AE50` | State handler #7 for DATA_85ACD0 |
| `8` | `$85AE5F` | `CODE_85AE5F` | State handler #8 for DATA_85ACD0 |
| `9` | `$85AE6B` | `CODE_85AE6B` | State handler #9 for DATA_85ACD0 |
| `10` | `$85AD5B` | `CODE_85AD5B` | State handler #10 for DATA_85ACD0 |
| `11` | `$85AD5A` | `CODE_85AD5A` | State handler #11 for DATA_85ACD0 |
| `12` | `$85AD5A` | `CODE_85AD5A` | State handler #12 for DATA_85ACD0 |

### Table 4: `CODE_85C37C` -> `DATA_85C383` ($C380 -> $C383)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (13 states)
- **Dispatch Site:** `$85:C380` (`JMP.w (DATA_85C383,x)`)
- **Table Base:** `$85:C383` (13 target routines)
- **Index Selector:** `LDA.w #CODE_85C377`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85C39E` | `CODE_85C39E` | State handler #0 for DATA_85C383 |
| `1` | `$85C3AE` | `CODE_85C3AE` | State handler #1 for DATA_85C383 |
| `2` | `$85C39D` | `CODE_85C39D` | State handler #2 for DATA_85C383 |
| `3` | `$85C39D` | `CODE_85C39D` | State handler #3 for DATA_85C383 |
| `4` | `$85C41B` | `CODE_85C41B` | State handler #4 for DATA_85C383 |
| `5` | `$85C41B` | `CODE_85C41B` | State handler #5 for DATA_85C383 |
| `6` | `$85C400` | `CODE_85C400` | State handler #6 for DATA_85C383 |
| `7` | `$85C400` | `CODE_85C400` | State handler #7 for DATA_85C383 |
| `8` | `$85C3BA` | `CODE_85C3BA` | State handler #8 for DATA_85C383 |
| `9` | `$85C401` | `CODE_85C401` | State handler #9 for DATA_85C383 |
| `10` | `$85C39D` | `CODE_85C39D` | State handler #10 for DATA_85C383 |
| `11` | `$85C39D` | `CODE_85C39D` | State handler #11 for DATA_85C383 |
| `12` | `$85C39D` | `CODE_85C39D` | State handler #12 for DATA_85C383 |

### Table 5: `CODE_85C8D8` -> `DATA_85C8E0` ($C8DD -> $C8E0)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (3 states)
- **Dispatch Site:** `$85:C8DD` (`JMP.w (DATA_85C8E0,x)`)
- **Table Base:** `$85:C8E0` (3 target routines)
- **Index Selector:** `LDA.w $1662`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85C8E6` | `CODE_85C8E6` | State handler #0 for DATA_85C8E0 |
| `1` | `$85C90A` | `CODE_85C90A` | State handler #1 for DATA_85C8E0 |
| `2` | `$85C90B` | `CODE_85C90B` | State handler #2 for DATA_85C8E0 |

### Table 6: `CODE_85CA37` -> `DATA_85CA40` ($CA3D -> $CA40)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (3 states)
- **Dispatch Site:** `$85:CA3D` (`JMP.w (DATA_85CA40,x)`)
- **Table Base:** `$85:CA40` (3 target routines)
- **Index Selector:** `LDA.w $1680`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85CA62` | `CODE_85CA62` | State handler #0 for DATA_85CA40 |
| `1` | `$85CA62` | `CODE_85CA62` | State handler #1 for DATA_85CA40 |
| `2` | `$85CA46` | `CODE_85CA46` | State handler #2 for DATA_85CA40 |

### Table 7: `CODE_85D337` -> `DATA_85D351` ($D34E -> $D351)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (13 states)
- **Dispatch Site:** `$85:D34E` (`JMP.w (DATA_85D351,x)`)
- **Table Base:** `$85:D351` (13 target routines)
- **Index Selector:** `LDA.b ($40)`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85D36C` | `CODE_85D36C` | State handler #0 for DATA_85D351 |
| `1` | `$85D36C` | `CODE_85D36C` | State handler #1 for DATA_85D351 |
| `2` | `$85D37B` | `CODE_85D37B` | State handler #2 for DATA_85D351 |
| `3` | `$85D37C` | `CODE_85D37C` | State handler #3 for DATA_85D351 |
| `4` | `$85D36B` | `CODE_85D36B` | State handler #4 for DATA_85D351 |
| `5` | `$85D36B` | `CODE_85D36B` | State handler #5 for DATA_85D351 |
| `6` | `$85D37D` | `CODE_85D37D` | State handler #6 for DATA_85D351 |
| `7` | `$85D37E` | `CODE_85D37E` | State handler #7 for DATA_85D351 |
| `8` | `$85D37F` | `CODE_85D37F` | State handler #8 for DATA_85D351 |
| `9` | `$85D380` | `CODE_85D380` | State handler #9 for DATA_85D351 |
| `10` | `$85D36B` | `CODE_85D36B` | State handler #10 for DATA_85D351 |
| `11` | `$85D36B` | `CODE_85D36B` | State handler #11 for DATA_85D351 |
| `12` | `$85D36B` | `CODE_85D36B` | State handler #12 for DATA_85D351 |

### Table 8: `CODE_85E811` -> `DATA_85E82B` ($E828 -> $E82B)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (13 states)
- **Dispatch Site:** `$85:E828` (`JMP.w (DATA_85E82B,x)`)
- **Table Base:** `$85:E82B` (13 target routines)
- **Index Selector:** `LDA.b ($40)`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85E846` | `CODE_85E846` | State handler #0 for DATA_85E82B |
| `1` | `$85E855` | `CODE_85E855` | State handler #1 for DATA_85E82B |
| `2` | `$85E864` | `CODE_85E864` | State handler #2 for DATA_85E82B |
| `3` | `$85E865` | `CODE_85E865` | State handler #3 for DATA_85E82B |
| `4` | `$85E845` | `CODE_85E845` | State handler #4 for DATA_85E82B |
| `5` | `$85E845` | `CODE_85E845` | State handler #5 for DATA_85E82B |
| `6` | `$85E866` | `CODE_85E866` | State handler #6 for DATA_85E82B |
| `7` | `$85E867` | `CODE_85E867` | State handler #7 for DATA_85E82B |
| `8` | `$85E868` | `CODE_85E868` | State handler #8 for DATA_85E82B |
| `9` | `$85E869` | `CODE_85E869` | State handler #9 for DATA_85E82B |
| `10` | `$85E845` | `CODE_85E845` | State handler #10 for DATA_85E82B |
| `11` | `$85E845` | `CODE_85E845` | State handler #11 for DATA_85E82B |
| `12` | `$85E845` | `CODE_85E845` | State handler #12 for DATA_85E82B |

### Table 9: `CODE_85EAED` -> `DATA_85EAF2` ($EAEF -> $EAF2)

- **Subsystem Purpose:** Referee Decision AI, Cards & Whistle State Machine (3 states)
- **Dispatch Site:** `$85:EAEF` (`JMP.w (DATA_85EAF2,x)`)
- **Table Base:** `$85:EAF2` (3 target routines)
- **Index Selector:** `Unknown`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$85EB1D` | `CODE_85EB1D` | State handler #0 for DATA_85EAF2 |
| `1` | `$85EB1D` | `CODE_85EB1D` | State handler #1 for DATA_85EAF2 |
| `2` | `$85EAF8` | `CODE_85EAF8` | State handler #2 for DATA_85EAF2 |

## Bank $86: Match Gameplay Loop & Player Action State Machines

### Table 1: `CODE_868C7F` -> `DATA_868C86` ($8C83 -> $8C86)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:8C83` (`JMP.w (DATA_868C86,x)`)
- **Table Base:** `$86:8C86` (13 target routines)
- **Index Selector:** `LDA.w #DATA_87AEF6`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$868CC0` | `CODE_868CC0` | State handler #0 for DATA_868C86 |
| `1` | `$86911D` | `CODE_86911D` | State handler #1 for DATA_868C86 |
| `2` | `$8691CF` | `CODE_8691CF` | State handler #2 for DATA_868C86 |
| `3` | `$869247` | `CODE_869247` | State handler #3 for DATA_868C86 |
| `4` | `$868CA0` | `CODE_868CA0` | State handler #4 for DATA_868C86 |
| `5` | `$868CB7` | `CODE_868CB7` | State handler #5 for DATA_868C86 |
| `6` | `$86927C` | `CODE_86927C` | State handler #6 for DATA_868C86 |
| `7` | `$8693C9` | `CODE_8693C9` | State handler #7 for DATA_868C86 |
| `8` | `$8694DA` | `CODE_8694DA` | State handler #8 for DATA_868C86 |
| `9` | `$869608` | `CODE_869608` | State handler #9 for DATA_868C86 |
| `10` | `$8691A9` | `CODE_8691A9` | State handler #10 for DATA_868C86 |
| `11` | `$8691A9` | `CODE_8691A9` | State handler #11 for DATA_868C86 |
| `12` | `$8691A9` | `CODE_8691A9` | State handler #12 for DATA_868C86 |

### Table 2: `CODE_868CC0` -> `DATA_868CC7` ($8CC4 -> $8CC7)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:8CC4` (`JMP.w (DATA_868CC7,x)`)
- **Table Base:** `$86:8CC7` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$868CD3` | `CODE_868CD3` | State handler #0 for DATA_868CC7 |
| `1` | `$868EF2` | `CODE_868EF2` | State handler #1 for DATA_868CC7 |
| `2` | `$868F39` | `CODE_868F39` | State handler #2 for DATA_868CC7 |
| `3` | `$868FF6` | `CODE_868FF6` | State handler #3 for DATA_868CC7 |
| `4` | `$869085` | `CODE_869085` | State handler #4 for DATA_868CC7 |
| `5` | `$8690D3` | `CODE_8690D3` | State handler #5 for DATA_868CC7 |

### Table 3: `CODE_868F39` -> `DATA_868F40` ($8F3D -> $8F40)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:8F3D` (`JMP.w (DATA_868F40,x)`)
- **Table Base:** `$86:8F40` (3 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$868F46` | `CODE_868F46` | State handler #0 for DATA_868F40 |
| `1` | `$868F72` | `CODE_868F72` | State handler #1 for DATA_868F40 |
| `2` | `$868F8E` | `CODE_868F8E` | State handler #2 for DATA_868F40 |

### Table 4: `CODE_868FF6` -> `DATA_868FFD` ($8FFA -> $8FFD)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:8FFA` (`JMP.w (DATA_868FFD,x)`)
- **Table Base:** `$86:8FFD` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869001` | `CODE_869001` | State handler #0 for DATA_868FFD |
| `1` | `$86907B` | `CODE_86907B` | State handler #1 for DATA_868FFD |

### Table 5: `CODE_869001` -> `DATA_86900C` ($9009 -> $900C)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (4 states)
- **Dispatch Site:** `$86:9009` (`JMP.w (DATA_86900C,x)`)
- **Table Base:** `$86:900C` (4 target routines)
- **Index Selector:** `LDA.b $FA`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869014` | `CODE_869014` | State handler #0 for DATA_86900C |
| `1` | `$869031` | `CODE_869031` | State handler #1 for DATA_86900C |
| `2` | `$869064` | `CODE_869064` | State handler #2 for DATA_86900C |
| `3` | `$869054` | `CODE_869054` | State handler #3 for DATA_86900C |

### Table 6: `CODE_869085` -> `DATA_86908C` ($9089 -> $908C)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9089` (`JMP.w (DATA_86908C,x)`)
- **Table Base:** `$86:908C` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869090` | `CODE_869090` | State handler #0 for DATA_86908C |
| `1` | `$8690C9` | `CODE_8690C9` | State handler #1 for DATA_86908C |

### Table 7: `CODE_8690D3` -> `DATA_8690DE` ($90DB -> $90DE)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (4 states)
- **Dispatch Site:** `$86:90DB` (`JMP.w (DATA_8690DE,x)`)
- **Table Base:** `$86:90DE` (4 target routines)
- **Index Selector:** `LDA.b $FA`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8690E6` | `CODE_8690E6` | State handler #0 for DATA_8690DE |
| `1` | `$869106` | `CODE_869106` | State handler #1 for DATA_8690DE |
| `2` | `$869110` | `CODE_869110` | State handler #2 for DATA_8690DE |
| `3` | `$8690F9` | `CODE_8690F9` | State handler #3 for DATA_8690DE |

### Table 8: `CODE_86911D` -> `DATA_869124` ($9121 -> $9124)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:9121` (`JMP.w (DATA_869124,x)`)
- **Table Base:** `$86:9124` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869130` | `CODE_869130` | State handler #0 for DATA_869124 |
| `1` | `$86915D` | `CODE_86915D` | State handler #1 for DATA_869124 |
| `2` | `$869141` | `CODE_869141` | State handler #2 for DATA_869124 |
| `3` | `$869171` | `CODE_869171` | State handler #3 for DATA_869124 |
| `4` | `$869193` | `CODE_869193` | State handler #4 for DATA_869124 |
| `5` | `$8691B0` | `CODE_8691B0` | State handler #5 for DATA_869124 |

### Table 9: `CODE_869141` -> `DATA_869148` ($9145 -> $9148)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:9145` (`JMP.w (DATA_869148,x)`)
- **Table Base:** `$86:9148` (3 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86914E` | `CODE_86914E` | State handler #0 for DATA_869148 |
| `1` | `$869151` | `CODE_869151` | State handler #1 for DATA_869148 |
| `2` | `$86915A` | `CODE_86915A` | State handler #2 for DATA_869148 |

### Table 10: `CODE_869171` -> `DATA_869178` ($9175 -> $9178)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9175` (`JMP.w (DATA_869178,x)`)
- **Table Base:** `$86:9178` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86917C` | `CODE_86917C` | State handler #0 for DATA_869178 |
| `1` | `$869187` | `CODE_869187` | State handler #1 for DATA_869178 |

### Table 11: `CODE_869193` -> `DATA_86919A` ($9197 -> $919A)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9197` (`JMP.w (DATA_86919A,x)`)
- **Table Base:** `$86:919A` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86919E` | `CODE_86919E` | State handler #0 for DATA_86919A |
| `1` | `$8691AA` | `CODE_8691AA` | State handler #1 for DATA_86919A |

### Table 12: `CODE_8691CF` -> `DATA_8691D6` ($91D3 -> $91D6)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:91D3` (`JMP.w (DATA_8691D6,x)`)
- **Table Base:** `$86:91D6` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8691A9` | `CODE_8691A9` | State handler #0 for DATA_8691D6 |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_8691D6 |
| `2` | `$8691A9` | `CODE_8691A9` | State handler #2 for DATA_8691D6 |
| `3` | `$8691E2` | `CODE_8691E2` | State handler #3 for DATA_8691D6 |
| `4` | `$869202` | `CODE_869202` | State handler #4 for DATA_8691D6 |
| `5` | `$869225` | `CODE_869225` | State handler #5 for DATA_8691D6 |

### Table 13: `CODE_8691E2` -> `DATA_8691E9` ($91E6 -> $91E9)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:91E6` (`JMP.w (DATA_8691E9,x)`)
- **Table Base:** `$86:91E9` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8691ED` | `CODE_8691ED` | State handler #0 for DATA_8691E9 |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_8691E9 |

### Table 14: `CODE_869202` -> `DATA_869209` ($9206 -> $9209)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9206` (`JMP.w (DATA_869209,x)`)
- **Table Base:** `$86:9209` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86920D` | `CODE_86920D` | State handler #0 for DATA_869209 |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_869209 |

### Table 15: `CODE_869225` -> `DATA_86922C` ($9229 -> $922C)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9229` (`JMP.w (DATA_86922C,x)`)
- **Table Base:** `$86:922C` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869230` | `CODE_869230` | State handler #0 for DATA_86922C |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_86922C |

### Table 16: `CODE_869247` -> `DATA_86924E` ($924B -> $924E)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:924B` (`JMP.w (DATA_86924E,x)`)
- **Table Base:** `$86:924E` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8691A9` | `CODE_8691A9` | State handler #0 for DATA_86924E |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_86924E |
| `2` | `$8691A9` | `CODE_8691A9` | State handler #2 for DATA_86924E |
| `3` | `$86925A` | `CODE_86925A` | State handler #3 for DATA_86924E |
| `4` | `$86925A` | `CODE_86925A` | State handler #4 for DATA_86924E |
| `5` | `$86926B` | `CODE_86926B` | State handler #5 for DATA_86924E |

### Table 17: `CODE_86925A` -> `DATA_869261` ($925E -> $9261)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:925E` (`JMP.w (DATA_869261,x)`)
- **Table Base:** `$86:9261` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869265` | `CODE_869265` | State handler #0 for DATA_869261 |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_869261 |

### Table 18: `CODE_86926B` -> `DATA_869272` ($926F -> $9272)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:926F` (`JMP.w (DATA_869272,x)`)
- **Table Base:** `$86:9272` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869276` | `CODE_869276` | State handler #0 for DATA_869272 |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_869272 |

### Table 19: `CODE_86927C` -> `DATA_869283` ($9280 -> $9283)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:9280` (`JMP.w (DATA_869283,x)`)
- **Table Base:** `$86:9283` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86928F` | `CODE_86928F` | State handler #0 for DATA_869283 |
| `1` | `$8692F2` | `CODE_8692F2` | State handler #1 for DATA_869283 |
| `2` | `$8692C0` | `CODE_8692C0` | State handler #2 for DATA_869283 |
| `3` | `$869314` | `CODE_869314` | State handler #3 for DATA_869283 |
| `4` | `$86937D` | `CODE_86937D` | State handler #4 for DATA_869283 |
| `5` | `$8693AC` | `CODE_8693AC` | State handler #5 for DATA_869283 |

### Table 20: `CODE_8692C0` -> `DATA_8692C7` ($92C4 -> $92C7)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:92C4` (`JMP.w (DATA_8692C7,x)`)
- **Table Base:** `$86:92C7` (3 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8692CD` | `CODE_8692CD` | State handler #0 for DATA_8692C7 |
| `1` | `$8692DE` | `CODE_8692DE` | State handler #1 for DATA_8692C7 |
| `2` | `$8692F1` | `CODE_8692F1` | State handler #2 for DATA_8692C7 |

### Table 21: `CODE_869314` -> `DATA_86931B` ($9318 -> $931B)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9318` (`JMP.w (DATA_86931B,x)`)
- **Table Base:** `$86:931B` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86931F` | `CODE_86931F` | State handler #0 for DATA_86931B |
| `1` | `$869343` | `CODE_869343` | State handler #1 for DATA_86931B |

### Table 22: `CODE_86937D` -> `DATA_869384` ($9381 -> $9384)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9381` (`JMP.w (DATA_869384,x)`)
- **Table Base:** `$86:9384` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869388` | `CODE_869388` | State handler #0 for DATA_869384 |
| `1` | `$86939D` | `CODE_86939D` | State handler #1 for DATA_869384 |

### Table 23: `CODE_8693C9` -> `DATA_8693D0` ($93CD -> $93D0)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:93CD` (`JMP.w (DATA_8693D0,x)`)
- **Table Base:** `$86:93D0` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8693DC` | `CODE_8693DC` | State handler #0 for DATA_8693D0 |
| `1` | `$86941E` | `CODE_86941E` | State handler #1 for DATA_8693D0 |
| `2` | `$869400` | `CODE_869400` | State handler #2 for DATA_8693D0 |
| `3` | `$86942F` | `CODE_86942F` | State handler #3 for DATA_8693D0 |
| `4` | `$869486` | `CODE_869486` | State handler #4 for DATA_8693D0 |
| `5` | `$8694B8` | `CODE_8694B8` | State handler #5 for DATA_8693D0 |

### Table 24: `CODE_869400` -> `DATA_869407` ($9404 -> $9407)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:9404` (`JMP.w (DATA_869407,x)`)
- **Table Base:** `$86:9407` (3 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86940D` | `CODE_86940D` | State handler #0 for DATA_869407 |
| `1` | `$869415` | `CODE_869415` | State handler #1 for DATA_869407 |
| `2` | `$86941D` | `CODE_86941D` | State handler #2 for DATA_869407 |

### Table 25: `CODE_86942F` -> `DATA_869436` ($9433 -> $9436)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9433` (`JMP.w (DATA_869436,x)`)
- **Table Base:** `$86:9436` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86943A` | `CODE_86943A` | State handler #0 for DATA_869436 |
| `1` | `$869461` | `CODE_869461` | State handler #1 for DATA_869436 |

### Table 26: `CODE_869486` -> `DATA_86948D` ($948A -> $948D)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:948A` (`JMP.w (DATA_86948D,x)`)
- **Table Base:** `$86:948D` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869491` | `CODE_869491` | State handler #0 for DATA_86948D |
| `1` | `$8694A9` | `CODE_8694A9` | State handler #1 for DATA_86948D |

### Table 27: `CODE_8694DA` -> `DATA_8694E1` ($94DE -> $94E1)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:94DE` (`JMP.w (DATA_8694E1,x)`)
- **Table Base:** `$86:94E1` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8694ED` | `CODE_8694ED` | State handler #0 for DATA_8694E1 |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_8694E1 |
| `2` | `$869514` | `CODE_869514` | State handler #2 for DATA_8694E1 |
| `3` | `$86952B` | `CODE_86952B` | State handler #3 for DATA_8694E1 |
| `4` | `$869587` | `CODE_869587` | State handler #4 for DATA_8694E1 |
| `5` | `$869605` | `CODE_869605` | State handler #5 for DATA_8694E1 |

### Table 28: `CODE_86952B` -> `DATA_869532` ($952F -> $9532)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:952F` (`JMP.w (DATA_869532,x)`)
- **Table Base:** `$86:9532` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869536` | `CODE_869536` | State handler #0 for DATA_869532 |
| `1` | `$86954E` | `CODE_86954E` | State handler #1 for DATA_869532 |

### Table 29: `CODE_869587` -> `DATA_86958E` ($958B -> $958E)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:958B` (`JMP.w (DATA_86958E,x)`)
- **Table Base:** `$86:958E` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869592` | `CODE_869592` | State handler #0 for DATA_86958E |
| `1` | `$8695B0` | `CODE_8695B0` | State handler #1 for DATA_86958E |

### Table 30: `CODE_869608` -> `DATA_86960F` ($960C -> $960F)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:960C` (`JMP.w (DATA_86960F,x)`)
- **Table Base:** `$86:960F` (6 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86961B` | `CODE_86961B` | State handler #0 for DATA_86960F |
| `1` | `$8691A9` | `CODE_8691A9` | State handler #1 for DATA_86960F |
| `2` | `$86963C` | `CODE_86963C` | State handler #2 for DATA_86960F |
| `3` | `$869653` | `CODE_869653` | State handler #3 for DATA_86960F |
| `4` | `$86969F` | `CODE_86969F` | State handler #4 for DATA_86960F |
| `5` | `$8696D7` | `CODE_8696D7` | State handler #5 for DATA_86960F |

### Table 31: `CODE_869653` -> `DATA_86965A` ($9657 -> $965A)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9657` (`JMP.w (DATA_86965A,x)`)
- **Table Base:** `$86:965A` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86965E` | `CODE_86965E` | State handler #0 for DATA_86965A |
| `1` | `$86967B` | `CODE_86967B` | State handler #1 for DATA_86965A |

### Table 32: `CODE_86969F` -> `DATA_8696A6` ($96A3 -> $96A6)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:96A3` (`JMP.w (DATA_8696A6,x)`)
- **Table Base:** `$86:96A6` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8696AA` | `CODE_8696AA` | State handler #0 for DATA_8696A6 |
| `1` | `$8696C8` | `CODE_8696C8` | State handler #1 for DATA_8696A6 |

### Table 33: `CODE_869BE6` -> `DATA_869BF8` ($9BF5 -> $9BF8)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:9BF5` (`JMP.w (DATA_869BF8,x)`)
- **Table Base:** `$86:9BF8` (13 target routines)
- **Index Selector:** `Unknown`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869C68` | `CODE_869C68` | State handler #0 for DATA_869BF8 |
| `1` | `$869E02` | `CODE_869E02` | State handler #1 for DATA_869BF8 |
| `2` | `$869E48` | `CODE_869E48` | State handler #2 for DATA_869BF8 |
| `3` | `$869E48` | `CODE_869E48` | State handler #3 for DATA_869BF8 |
| `4` | `$869C12` | `CODE_869C12` | State handler #4 for DATA_869BF8 |
| `5` | `$869C60` | `CODE_869C60` | State handler #5 for DATA_869BF8 |
| `6` | `$869EDA` | `CODE_869EDA` | State handler #6 for DATA_869BF8 |
| `7` | `$869F7E` | `CODE_869F7E` | State handler #7 for DATA_869BF8 |
| `8` | `$86A03E` | `CODE_86A03E` | State handler #8 for DATA_869BF8 |
| `9` | `$86A0B5` | `CODE_86A0B5` | State handler #9 for DATA_869BF8 |
| `10` | `$869C67` | `CODE_869C67` | State handler #10 for DATA_869BF8 |
| `11` | `$869C67` | `CODE_869C67` | State handler #11 for DATA_869BF8 |
| `12` | `$869C67` | `CODE_869C67` | State handler #12 for DATA_869BF8 |

### Table 34: `CODE_869C68` -> `DATA_869C6F` ($9C6C -> $9C6F)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9C6C` (`JMP.w (DATA_869C6F,x)`)
- **Table Base:** `$86:9C6F` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869C73` | `CODE_869C73` | State handler #0 for DATA_869C6F |
| `1` | `$869CB6` | `CODE_869CB6` | State handler #1 for DATA_869C6F |

### Table 35: `CODE_869DC9` -> `DATA_869DD0` ($9DCD -> $9DD0)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9DCD` (`JMP.w (DATA_869DD0,x)`)
- **Table Base:** `$86:9DD0` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869DD4` | `CODE_869DD4` | State handler #0 for DATA_869DD0 |
| `1` | `$869DE7` | `CODE_869DE7` | State handler #1 for DATA_869DD0 |

### Table 36: `CODE_869E02` -> `DATA_869E0C` ($9E09 -> $9E0C)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9E09` (`JMP.w (DATA_869E0C,x)`)
- **Table Base:** `$86:9E0C` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869E10` | `CODE_869E10` | State handler #0 for DATA_869E0C |
| `1` | `$869E27` | `CODE_869E27` | State handler #1 for DATA_869E0C |

### Table 37: `CODE_869EDA` -> `DATA_869EE1` ($9EDE -> $9EE1)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9EDE` (`JMP.w (DATA_869EE1,x)`)
- **Table Base:** `$86:9EE1` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869EE5` | `CODE_869EE5` | State handler #0 for DATA_869EE1 |
| `1` | `$869F67` | `CODE_869F67` | State handler #1 for DATA_869EE1 |

### Table 38: `CODE_869F7E` -> `DATA_869F85` ($9F82 -> $9F85)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:9F82` (`JMP.w (DATA_869F85,x)`)
- **Table Base:** `$86:9F85` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$869F89` | `CODE_869F89` | State handler #0 for DATA_869F85 |
| `1` | `$86A027` | `CODE_86A027` | State handler #1 for DATA_869F85 |

### Table 39: `CODE_86A84B` -> `DATA_86A852` ($A84F -> $A852)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (5 states)
- **Dispatch Site:** `$86:A84F` (`JMP.w (DATA_86A852,x)`)
- **Table Base:** `$86:A852` (5 target routines)
- **Index Selector:** `LDA.b $EC`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86A85C` | `CODE_86A85C` | State handler #0 for DATA_86A852 |
| `1` | `$86A85C` | `CODE_86A85C` | State handler #1 for DATA_86A852 |
| `2` | `$86A85C` | `CODE_86A85C` | State handler #2 for DATA_86A852 |
| `3` | `$86A88B` | `CODE_86A88B` | State handler #3 for DATA_86A852 |
| `4` | `$86A88B` | `CODE_86A88B` | State handler #4 for DATA_86A852 |

### Table 40: `CODE_86A935` -> `DATA_86A93C` ($A939 -> $A93C)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (5 states)
- **Dispatch Site:** `$86:A939` (`JMP.w (DATA_86A93C,x)`)
- **Table Base:** `$86:A93C` (5 target routines)
- **Index Selector:** `LDA.b $EC`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86A946` | `CODE_86A946` | State handler #0 for DATA_86A93C |
| `1` | `$86A95C` | `CODE_86A95C` | State handler #1 for DATA_86A93C |
| `2` | `$86A95C` | `CODE_86A95C` | State handler #2 for DATA_86A93C |
| `3` | `$86A95C` | `CODE_86A95C` | State handler #3 for DATA_86A93C |
| `4` | `$86A95C` | `CODE_86A95C` | State handler #4 for DATA_86A93C |

### Table 41: `CODE_86A95D` -> `DATA_86A964` ($A961 -> $A964)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (5 states)
- **Dispatch Site:** `$86:A961` (`JMP.w (DATA_86A964,x)`)
- **Table Base:** `$86:A964` (5 target routines)
- **Index Selector:** `LDA.b $EC`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86A96E` | `CODE_86A96E` | State handler #0 for DATA_86A964 |
| `1` | `$86A991` | `CODE_86A991` | State handler #1 for DATA_86A964 |
| `2` | `$86A991` | `CODE_86A991` | State handler #2 for DATA_86A964 |
| `3` | `$86A991` | `CODE_86A991` | State handler #3 for DATA_86A964 |
| `4` | `$86A991` | `CODE_86A991` | State handler #4 for DATA_86A964 |

### Table 42: `CODE_86A992` -> `DATA_86A999` ($A996 -> $A999)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (5 states)
- **Dispatch Site:** `$86:A996` (`JMP.w (DATA_86A999,x)`)
- **Table Base:** `$86:A999` (5 target routines)
- **Index Selector:** `LDA.b $EC`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86A9A3` | `CODE_86A9A3` | State handler #0 for DATA_86A999 |
| `1` | `$86A9C3` | `CODE_86A9C3` | State handler #1 for DATA_86A999 |
| `2` | `$86A9C3` | `CODE_86A9C3` | State handler #2 for DATA_86A999 |
| `3` | `$86A9C3` | `CODE_86A9C3` | State handler #3 for DATA_86A999 |
| `4` | `$86A9C3` | `CODE_86A9C3` | State handler #4 for DATA_86A999 |

### Table 43: `CODE_86AA3E` -> `DATA_86AA45` ($AA42 -> $AA45)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (5 states)
- **Dispatch Site:** `$86:AA42` (`JMP.w (DATA_86AA45,x)`)
- **Table Base:** `$86:AA45` (5 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AA4F` | `CODE_86AA4F` | State handler #0 for DATA_86AA45 |
| `1` | `$86AA4F` | `CODE_86AA4F` | State handler #1 for DATA_86AA45 |
| `2` | `$86AA4F` | `CODE_86AA4F` | State handler #2 for DATA_86AA45 |
| `3` | `$86AA50` | `CODE_86AA50` | State handler #3 for DATA_86AA45 |
| `4` | `$86AA5C` | `CODE_86AA5C` | State handler #4 for DATA_86AA45 |

### Table 44: `CODE_86ABA1` -> `DATA_86ABA8` ($ABA5 -> $ABA8)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (5 states)
- **Dispatch Site:** `$86:ABA5` (`JMP.w (DATA_86ABA8,x)`)
- **Table Base:** `$86:ABA8` (5 target routines)
- **Index Selector:** `LDA.b $EC`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86ABB2` | `CODE_86ABB2` | State handler #0 for DATA_86ABA8 |
| `1` | `$86ABD0` | `CODE_86ABD0` | State handler #1 for DATA_86ABA8 |
| `2` | `$86ABB2` | `CODE_86ABB2` | State handler #2 for DATA_86ABA8 |
| `3` | `$86ABD0` | `CODE_86ABD0` | State handler #3 for DATA_86ABA8 |
| `4` | `$86ABD0` | `CODE_86ABD0` | State handler #4 for DATA_86ABA8 |

### Table 45: `CODE_86ABD6` -> `DATA_86ABDD` ($ABDA -> $ABDD)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (5 states)
- **Dispatch Site:** `$86:ABDA` (`JMP.w (DATA_86ABDD,x)`)
- **Table Base:** `$86:ABDD` (5 target routines)
- **Index Selector:** `LDA.b $EC`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86ABE7` | `CODE_86ABE7` | State handler #0 for DATA_86ABDD |
| `1` | `$86ABD0` | `CODE_86ABD0` | State handler #1 for DATA_86ABDD |
| `2` | `$86ABE7` | `CODE_86ABE7` | State handler #2 for DATA_86ABDD |
| `3` | `$86ABD0` | `CODE_86ABD0` | State handler #3 for DATA_86ABDD |
| `4` | `$86ABD0` | `CODE_86ABD0` | State handler #4 for DATA_86ABDD |

### Table 46: `CODE_86AC89` -> `DATA_86AC98` ($AC95 -> $AC98)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:AC95` (`JMP.w (DATA_86AC98,x)`)
- **Table Base:** `$86:AC98` (13 target routines)
- **Index Selector:** `LDA.w #DATA_87AEF6`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AD86` | `CODE_86AD86` | State handler #0 for DATA_86AC98 |
| `1` | `$86AF45` | `CODE_86AF45` | State handler #1 for DATA_86AC98 |
| `2` | `$86AF97` | `CODE_86AF97` | State handler #2 for DATA_86AC98 |
| `3` | `$86B085` | `CODE_86B085` | State handler #3 for DATA_86AC98 |
| `4` | `$86ACB2` | `CODE_86ACB2` | State handler #4 for DATA_86AC98 |
| `5` | `$86AD18` | `CODE_86AD18` | State handler #5 for DATA_86AC98 |
| `6` | `$86B0A3` | `CODE_86B0A3` | State handler #6 for DATA_86AC98 |
| `7` | `$86B11D` | `CODE_86B11D` | State handler #7 for DATA_86AC98 |
| `8` | `$86B17F` | `CODE_86B17F` | State handler #8 for DATA_86AC98 |
| `9` | `$86B202` | `CODE_86B202` | State handler #9 for DATA_86AC98 |
| `10` | `$86AD85` | `CODE_86AD85` | State handler #10 for DATA_86AC98 |
| `11` | `$86AD85` | `CODE_86AD85` | State handler #11 for DATA_86AC98 |
| `12` | `$86AD85` | `CODE_86AD85` | State handler #12 for DATA_86AC98 |

### Table 47: `CODE_86ACB2` -> `DATA_86ACB9` ($ACB6 -> $ACB9)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:ACB6` (`JMP.w (DATA_86ACB9,x)`)
- **Table Base:** `$86:ACB9` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86ACC1` | `CODE_86ACC1` | State handler #0 for DATA_86ACB9 |
| `1` | `$869C12` | `CODE_869C12` | State handler #1 for DATA_86ACB9 |
| `2` | `$86AD85` | `CODE_86AD85` | State handler #2 for DATA_86ACB9 |
| `3` | `$86AD85` | `CODE_86AD85` | State handler #3 for DATA_86ACB9 |

### Table 48: `CODE_86AD18` -> `DATA_86AD1F` ($AD1C -> $AD1F)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:AD1C` (`JMP.w (DATA_86AD1F,x)`)
- **Table Base:** `$86:AD1F` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AD27` | `CODE_86AD27` | State handler #0 for DATA_86AD1F |
| `1` | `$869C60` | `CODE_869C60` | State handler #1 for DATA_86AD1F |
| `2` | `$869C60` | `CODE_869C60` | State handler #2 for DATA_86AD1F |
| `3` | `$869C60` | `CODE_869C60` | State handler #3 for DATA_86AD1F |

### Table 49: `CODE_86AD86` -> `DATA_86AD8D` ($AD8A -> $AD8D)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:AD8A` (`JMP.w (DATA_86AD8D,x)`)
- **Table Base:** `$86:AD8D` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AD95` | `CODE_86AD95` | State handler #0 for DATA_86AD8D |
| `1` | `$86ADD9` | `CODE_86ADD9` | State handler #1 for DATA_86AD8D |
| `2` | `$86ADF8` | `CODE_86ADF8` | State handler #2 for DATA_86AD8D |
| `3` | `$86AE16` | `CODE_86AE16` | State handler #3 for DATA_86AD8D |

### Table 50: `CODE_86AF45` -> `DATA_86AF4C` ($AF49 -> $AF4C)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:AF49` (`JMP.w (DATA_86AF4C,x)`)
- **Table Base:** `$86:AF4C` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AF54` | `CODE_86AF54` | State handler #0 for DATA_86AF4C |
| `1` | `$86AF68` | `CODE_86AF68` | State handler #1 for DATA_86AF4C |
| `2` | `$86AF76` | `CODE_86AF76` | State handler #2 for DATA_86AF4C |
| `3` | `$86AF8C` | `CODE_86AF8C` | State handler #3 for DATA_86AF4C |

### Table 51: `CODE_86AF97` -> `DATA_86AF9E` ($AF9B -> $AF9E)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:AF9B` (`JMP.w (DATA_86AF9E,x)`)
- **Table Base:** `$86:AF9E` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AFA6` | `CODE_86AFA6` | State handler #0 for DATA_86AF9E |
| `1` | `$86B010` | `CODE_86B010` | State handler #1 for DATA_86AF9E |
| `2` | `$86B028` | `CODE_86B028` | State handler #2 for DATA_86AF9E |
| `3` | `$86AD85` | `CODE_86AD85` | State handler #3 for DATA_86AF9E |

### Table 52: `CODE_86B085` -> `DATA_86B08C` ($B089 -> $B08C)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B089` (`JMP.w (DATA_86B08C,x)`)
- **Table Base:** `$86:B08C` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86AD85` | `CODE_86AD85` | State handler #0 for DATA_86B08C |
| `1` | `$86B094` | `CODE_86B094` | State handler #1 for DATA_86B08C |
| `2` | `$86B028` | `CODE_86B028` | State handler #2 for DATA_86B08C |
| `3` | `$86AD85` | `CODE_86AD85` | State handler #3 for DATA_86B08C |

### Table 53: `CODE_86B0A3` -> `DATA_86B0AA` ($B0A7 -> $B0AA)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B0A7` (`JMP.w (DATA_86B0AA,x)`)
- **Table Base:** `$86:B0AA` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86B0B2` | `CODE_86B0B2` | State handler #0 for DATA_86B0AA |
| `1` | `$86B0EB` | `CODE_86B0EB` | State handler #1 for DATA_86B0AA |
| `2` | `$86B116` | `CODE_86B116` | State handler #2 for DATA_86B0AA |
| `3` | `$86B116` | `CODE_86B116` | State handler #3 for DATA_86B0AA |

### Table 54: `CODE_86B11D` -> `DATA_86B124` ($B121 -> $B124)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B121` (`JMP.w (DATA_86B124,x)`)
- **Table Base:** `$86:B124` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86B12C` | `CODE_86B12C` | State handler #0 for DATA_86B124 |
| `1` | `$86B150` | `CODE_86B150` | State handler #1 for DATA_86B124 |
| `2` | `$86B178` | `CODE_86B178` | State handler #2 for DATA_86B124 |
| `3` | `$86B178` | `CODE_86B178` | State handler #3 for DATA_86B124 |

### Table 55: `CODE_86B17F` -> `DATA_86B186` ($B183 -> $B186)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B183` (`JMP.w (DATA_86B186,x)`)
- **Table Base:** `$86:B186` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86B18E` | `CODE_86B18E` | State handler #0 for DATA_86B186 |
| `1` | `$86B1B0` | `CODE_86B1B0` | State handler #1 for DATA_86B186 |
| `2` | `$86B18E` | `CODE_86B18E` | State handler #2 for DATA_86B186 |
| `3` | `$86B18E` | `CODE_86B18E` | State handler #3 for DATA_86B186 |

### Table 56: `CODE_86B202` -> `DATA_86B209` ($B206 -> $B209)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B206` (`JMP.w (DATA_86B209,x)`)
- **Table Base:** `$86:B209` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86B211` | `CODE_86B211` | State handler #0 for DATA_86B209 |
| `1` | `$86B22C` | `CODE_86B22C` | State handler #1 for DATA_86B209 |
| `2` | `$86B211` | `CODE_86B211` | State handler #2 for DATA_86B209 |
| `3` | `$86B211` | `CODE_86B211` | State handler #3 for DATA_86B209 |

### Table 57: `CODE_86B74A` -> `DATA_86B751` ($B74E -> $B751)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:B74E` (`JMP.w (DATA_86B751,x)`)
- **Table Base:** `$86:B751` (13 target routines)
- **Index Selector:** `LDY.w #$0020`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86B76C` | `CODE_86B76C` | State handler #0 for DATA_86B751 |
| `1` | `$86B995` | `CODE_86B995` | State handler #1 for DATA_86B751 |
| `2` | `$86BA22` | `CODE_86BA22` | State handler #2 for DATA_86B751 |
| `3` | `$86BA0E` | `CODE_86BA0E` | State handler #3 for DATA_86B751 |
| `4` | `$86B76B` | `CODE_86B76B` | State handler #4 for DATA_86B751 |
| `5` | `$86B76B` | `CODE_86B76B` | State handler #5 for DATA_86B751 |
| `6` | `$86BA5D` | `CODE_86BA5D` | State handler #6 for DATA_86B751 |
| `7` | `$86BAA5` | `CODE_86BAA5` | State handler #7 for DATA_86B751 |
| `8` | `$86BAE6` | `CODE_86BAE6` | State handler #8 for DATA_86B751 |
| `9` | `$86BB4B` | `CODE_86BB4B` | State handler #9 for DATA_86B751 |
| `10` | `$86B76B` | `CODE_86B76B` | State handler #10 for DATA_86B751 |
| `11` | `$86B76B` | `CODE_86B76B` | State handler #11 for DATA_86B751 |
| `12` | `$86B76B` | `CODE_86B76B` | State handler #12 for DATA_86B751 |

### Table 58: `CODE_86B76C` -> `DATA_86B773` ($B770 -> $B773)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B770` (`JMP.w (DATA_86B773,x)`)
- **Table Base:** `$86:B773` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86B77B` | `CODE_86B77B` | State handler #0 for DATA_86B773 |
| `1` | `$86B827` | `CODE_86B827` | State handler #1 for DATA_86B773 |
| `2` | `$86B966` | `CODE_86B966` | State handler #2 for DATA_86B773 |
| `3` | `$86B895` | `CODE_86B895` | State handler #3 for DATA_86B773 |

### Table 59: `CODE_86B995` -> `DATA_86B99C` ($B999 -> $B99C)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (4 states)
- **Dispatch Site:** `$86:B999` (`JMP.w (DATA_86B99C,x)`)
- **Table Base:** `$86:B99C` (4 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BA0C` | `CODE_86BA0C` | State handler #0 for DATA_86B99C |
| `1` | `$86BA09` | `CODE_86BA09` | State handler #1 for DATA_86B99C |
| `2` | `$86BA09` | `CODE_86BA09` | State handler #2 for DATA_86B99C |
| `3` | `$86B9A4` | `CODE_86B9A4` | State handler #3 for DATA_86B99C |

### Table 60: `CODE_86BA5D` -> `DATA_86BA64` ($BA61 -> $BA64)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BA61` (`JMP.w (DATA_86BA64,x)`)
- **Table Base:** `$86:BA64` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BA68` | `CODE_86BA68` | State handler #0 for DATA_86BA64 |
| `1` | `$86BA7F` | `CODE_86BA7F` | State handler #1 for DATA_86BA64 |

### Table 61: `CODE_86BAA5` -> `DATA_86BAAC` ($BAA9 -> $BAAC)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BAA9` (`JMP.w (DATA_86BAAC,x)`)
- **Table Base:** `$86:BAAC` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BAB0` | `CODE_86BAB0` | State handler #0 for DATA_86BAAC |
| `1` | `$86BABD` | `CODE_86BABD` | State handler #1 for DATA_86BAAC |

### Table 62: `CODE_86BAE6` -> `DATA_86BAED` ($BAEA -> $BAED)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BAEA` (`JMP.w (DATA_86BAED,x)`)
- **Table Base:** `$86:BAED` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BAF1` | `CODE_86BAF1` | State handler #0 for DATA_86BAED |
| `1` | `$86BB2A` | `CODE_86BB2A` | State handler #1 for DATA_86BAED |

### Table 63: `CODE_86BB4B` -> `DATA_86BB52` ($BB4F -> $BB52)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BB4F` (`JMP.w (DATA_86BB52,x)`)
- **Table Base:** `$86:BB52` (2 target routines)
- **Index Selector:** `LDA.b $1E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BB56` | `CODE_86BB56` | State handler #0 for DATA_86BB52 |
| `1` | `$86BB88` | `CODE_86BB88` | State handler #1 for DATA_86BB52 |

### Table 64: `CODE_86BE1B` -> `DATA_86BE25` ($BE22 -> $BE25)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:BE22` (`JMP.w (DATA_86BE25,x)`)
- **Table Base:** `$86:BE25` (13 target routines)
- **Index Selector:** `LDA.w #CODE_86BE1B`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BE6B` | `CODE_86BE6B` | State handler #0 for DATA_86BE25 |
| `1` | `$86BF6C` | `CODE_86BF6C` | State handler #1 for DATA_86BE25 |
| `2` | `$86BFC8` | `CODE_86BFC8` | State handler #2 for DATA_86BE25 |
| `3` | `$86BFFE` | `CODE_86BFFE` | State handler #3 for DATA_86BE25 |
| `4` | `$86BE3F` | `CODE_86BE3F` | State handler #4 for DATA_86BE25 |
| `5` | `$86BE61` | `CODE_86BE61` | State handler #5 for DATA_86BE25 |
| `6` | `$86C011` | `CODE_86C011` | State handler #6 for DATA_86BE25 |
| `7` | `$86C016` | `CODE_86C016` | State handler #7 for DATA_86BE25 |
| `8` | `$86C01B` | `CODE_86C01B` | State handler #8 for DATA_86BE25 |
| `9` | `$86C052` | `CODE_86C052` | State handler #9 for DATA_86BE25 |
| `10` | `$86BE69` | `CODE_86BE69` | State handler #10 for DATA_86BE25 |
| `11` | `$86BE6A` | `CODE_86BE6A` | State handler #11 for DATA_86BE25 |
| `12` | `$86BE6A` | `CODE_86BE6A` | State handler #12 for DATA_86BE25 |

### Table 65: `CODE_86BE6B` -> `DATA_86BE76` ($BE73 -> $BE76)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BE73` (`JMP.w (DATA_86BE76,x)`)
- **Table Base:** `$86:BE76` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BE7A` | `CODE_86BE7A` | State handler #0 for DATA_86BE76 |
| `1` | `$86BECC` | `CODE_86BECC` | State handler #1 for DATA_86BE76 |

### Table 66: `CODE_86BF6C` -> `DATA_86BF77` ($BF74 -> $BF77)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BF74` (`JMP.w (DATA_86BF77,x)`)
- **Table Base:** `$86:BF77` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BF7B` | `CODE_86BF7B` | State handler #0 for DATA_86BF77 |
| `1` | `$86BFBC` | `CODE_86BFBC` | State handler #1 for DATA_86BF77 |

### Table 67: `CODE_86BFC8` -> `DATA_86BFCF` ($BFCC -> $BFCF)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:BFCC` (`JMP.w (DATA_86BFCF,x)`)
- **Table Base:** `$86:BFCF` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86BFD3` | `CODE_86BFD3` | State handler #0 for DATA_86BFCF |
| `1` | `$86BFBC` | `CODE_86BFBC` | State handler #1 for DATA_86BFCF |

### Table 68: `CODE_86BFFE` -> `DATA_86C005` ($C002 -> $C005)

- **Subsystem Purpose:** Player Action Sub-State (Execute / Settle Branch)
- **Dispatch Site:** `$86:C002` (`JMP.w (DATA_86C005,x)`)
- **Table Base:** `$86:C005` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86C009` | `CODE_86C009` | State handler #0 for DATA_86C005 |
| `1` | `$86BFBC` | `CODE_86BFBC` | State handler #1 for DATA_86C005 |

### Table 69: `CODE_86C419` -> `DATA_86C426` ($C423 -> $C426)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:C423` (`JMP.w (DATA_86C426,x)`)
- **Table Base:** `$86:C426` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86C441` | `CODE_86C441` | State handler #0 for DATA_86C426 |
| `1` | `$86C46C` | `CODE_86C46C` | State handler #1 for DATA_86C426 |
| `2` | `$86C47C` | `CODE_86C47C` | State handler #2 for DATA_86C426 |
| `3` | `$86C47C` | `CODE_86C47C` | State handler #3 for DATA_86C426 |
| `4` | `$86C440` | `CODE_86C440` | State handler #4 for DATA_86C426 |
| `5` | `$86C440` | `CODE_86C440` | State handler #5 for DATA_86C426 |
| `6` | `$86C47D` | `CODE_86C47D` | State handler #6 for DATA_86C426 |
| `7` | `$86C47E` | `CODE_86C47E` | State handler #7 for DATA_86C426 |
| `8` | `$86C47F` | `CODE_86C47F` | State handler #8 for DATA_86C426 |
| `9` | `$86C480` | `CODE_86C480` | State handler #9 for DATA_86C426 |
| `10` | `$86C440` | `CODE_86C440` | State handler #10 for DATA_86C426 |
| `11` | `$86C440` | `CODE_86C440` | State handler #11 for DATA_86C426 |
| `12` | `$86C440` | `CODE_86C440` | State handler #12 for DATA_86C426 |

### Table 70: `CODE_86C857` -> `DATA_86C867` ($C864 -> $C867)

- **Subsystem Purpose:** Player Action / Ball Handling State Machine (13 states)
- **Dispatch Site:** `$86:C864` (`JMP.w (DATA_86C867,x)`)
- **Table Base:** `$86:C867` (13 target routines)
- **Index Selector:** `LDA.w DATA_81D8ED,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86C887` | `CODE_86C887` | State handler #0 for DATA_86C867 |
| `1` | `$86C8C5` | `CODE_86C8C5` | State handler #1 for DATA_86C867 |
| `2` | `$86C8E3` | `CODE_86C8E3` | State handler #2 for DATA_86C867 |
| `3` | `$86C8E4` | `CODE_86C8E4` | State handler #3 for DATA_86C867 |
| `4` | `$86C881` | `CODE_86C881` | State handler #4 for DATA_86C867 |
| `5` | `$86C881` | `CODE_86C881` | State handler #5 for DATA_86C867 |
| `6` | `$86C8E5` | `CODE_86C8E5` | State handler #6 for DATA_86C867 |
| `7` | `$86C8F3` | `CODE_86C8F3` | State handler #7 for DATA_86C867 |
| `8` | `$86C902` | `CODE_86C902` | State handler #8 for DATA_86C867 |
| `9` | `$86C966` | `CODE_86C966` | State handler #9 for DATA_86C867 |
| `10` | `$86C887` | `CODE_86C887` | State handler #10 for DATA_86C867 |
| `11` | `$86C882` | `CODE_86C882` | State handler #11 for DATA_86C867 |
| `12` | `$86C886` | `CODE_86C886` | State handler #12 for DATA_86C867 |

### Table 71: `CODE_86C902` -> `DATA_86C90E` ($C90B -> $C90E)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:C90B` (`JMP.w (DATA_86C90E,x)`)
- **Table Base:** `$86:C90E` (3 target routines)
- **Index Selector:** `LDA.w $151C,y`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86C914` | `CODE_86C914` | State handler #0 for DATA_86C90E |
| `1` | `$86C933` | `CODE_86C933` | State handler #1 for DATA_86C90E |
| `2` | `$86C947` | `CODE_86C947` | State handler #2 for DATA_86C90E |

### Table 72: `CODE_86C966` -> `DATA_86C972` ($C96F -> $C972)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:C96F` (`JMP.w (DATA_86C972,x)`)
- **Table Base:** `$86:C972` (3 target routines)
- **Index Selector:** `LDA.w $151C,y`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86C978` | `CODE_86C978` | State handler #0 for DATA_86C972 |
| `1` | `$86C997` | `CODE_86C997` | State handler #1 for DATA_86C972 |
| `2` | `$86C9A9` | `CODE_86C9A9` | State handler #2 for DATA_86C972 |

### Table 73: `CODE_86DF21` -> `DATA_86DF29` ($DF26 -> $DF29)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (6 states)
- **Dispatch Site:** `$86:DF26` (`JMP.w (DATA_86DF29,x)`)
- **Table Base:** `$86:DF29` (6 target routines)
- **Index Selector:** `LDA.w $151C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86DF5B` | `CODE_86DF5B` | State handler #0 for DATA_86DF29 |
| `1` | `$86DF5B` | `CODE_86DF5B` | State handler #1 for DATA_86DF29 |
| `2` | `$86DF5B` | `CODE_86DF5B` | State handler #2 for DATA_86DF29 |
| `3` | `$86DF7A` | `CODE_86DF7A` | State handler #3 for DATA_86DF29 |
| `4` | `$86DF35` | `CODE_86DF35` | State handler #4 for DATA_86DF29 |
| `5` | `$86DF5F` | `CODE_86DF5F` | State handler #5 for DATA_86DF29 |

### Table 74: `CODE_86E1FC` -> `DATA_86E208` ($E205 -> $E208)

- **Subsystem Purpose:** Player Control & Ball Possession State Machine (3 states)
- **Dispatch Site:** `$86:E205` (`JMP.w (DATA_86E208,x)`)
- **Table Base:** `$86:E208` (3 target routines)
- **Index Selector:** `LDA.w $151C,y`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$86E21F` | `CODE_86E21F` | State handler #0 for DATA_86E208 |
| `1` | `$86E20E` | `CODE_86E20E` | State handler #1 for DATA_86E208 |
| `2` | `$86E229` | `CODE_86E229` | State handler #2 for DATA_86E208 |

## Bank $8A: AI Tactical Decision Trees, Team Formations & Defensive Pressing

### Table 1: `CODE_8A9B95` -> `DATA_8A9B9C` ($9B99 -> $9B9C)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (7 states)
- **Dispatch Site:** `$8A:9B99` (`JMP.w (DATA_8A9B9C,x)`)
- **Table Base:** `$8A:9B9C` (7 target routines)
- **Index Selector:** `LDA.b $50`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8A9BAB` | `CODE_8A9BAB` | State handler #0 for DATA_8A9B9C |
| `1` | `$8A9BC8` | `CODE_8A9BC8` | State handler #1 for DATA_8A9B9C |
| `2` | `$8A9BD8` | `CODE_8A9BD8` | State handler #2 for DATA_8A9B9C |
| `3` | `$8A9C20` | `CODE_8A9C20` | State handler #3 for DATA_8A9B9C |
| `4` | `$8A9C31` | `CODE_8A9C31` | State handler #4 for DATA_8A9B9C |
| `5` | `$8A9C4D` | `CODE_8A9C4D` | State handler #5 for DATA_8A9B9C |
| `6` | `$8A9BAA` | `CODE_8A9BAA` | State handler #6 for DATA_8A9B9C |

### Table 2: `CODE_8A9E3C` -> `DATA_8A9E3F` ($9E3C -> $9E3F)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:9E3C` (`JMP.w (DATA_8A9E3F,x)`)
- **Table Base:** `$8A:9E3F` (9 target routines)
- **Index Selector:** `LDX.w #$0400`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8A9E52` | `CODE_8A9E52` | State handler #0 for DATA_8A9E3F |
| `1` | `$8A9E80` | `CODE_8A9E80` | State handler #1 for DATA_8A9E3F |
| `2` | `$8A9E51` | `CODE_8A9E51` | State handler #2 for DATA_8A9E3F |
| `3` | `$8A9E51` | `CODE_8A9E51` | State handler #3 for DATA_8A9E3F |
| `4` | `$8A9E9E` | `CODE_8A9E9E` | State handler #4 for DATA_8A9E3F |
| `5` | `$8A9E9E` | `CODE_8A9E9E` | State handler #5 for DATA_8A9E3F |
| `6` | `$8A9E51` | `CODE_8A9E51` | State handler #6 for DATA_8A9E3F |
| `7` | `$8A9E51` | `CODE_8A9E51` | State handler #7 for DATA_8A9E3F |
| `8` | `$8A9F40` | `CODE_8A9F40` | State handler #8 for DATA_8A9E3F |

### Table 3: `CODE_8A9F27` -> `DATA_8A9F2E` ($9F2B -> $9F2E)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:9F2B` (`JMP.w (DATA_8A9F2E,x)`)
- **Table Base:** `$8A:9F2E` (9 target routines)
- **Index Selector:** `Unknown`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8A9F59` | `CODE_8A9F59` | State handler #0 for DATA_8A9F2E |
| `1` | `$8A9FC0` | `CODE_8A9FC0` | State handler #1 for DATA_8A9F2E |
| `2` | `$8AA00F` | `CODE_8AA00F` | State handler #2 for DATA_8A9F2E |
| `3` | `$8AA00F` | `CODE_8AA00F` | State handler #3 for DATA_8A9F2E |
| `4` | `$8AA010` | `CODE_8AA010` | State handler #4 for DATA_8A9F2E |
| `5` | `$8AA010` | `CODE_8AA010` | State handler #5 for DATA_8A9F2E |
| `6` | `$8AA00F` | `CODE_8AA00F` | State handler #6 for DATA_8A9F2E |
| `7` | `$8AA00F` | `CODE_8AA00F` | State handler #7 for DATA_8A9F2E |
| `8` | `$8A9F40` | `CODE_8A9F40` | State handler #8 for DATA_8A9F2E |

### Table 4: `CODE_8AA11E` -> `DATA_8AA125` ($A122 -> $A125)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:A122` (`JMP.w (DATA_8AA125,x)`)
- **Table Base:** `$8A:A125` (9 target routines)
- **Index Selector:** `Unknown`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AA13E` | `CODE_8AA13E` | State handler #0 for DATA_8AA125 |
| `1` | `$8AA197` | `CODE_8AA197` | State handler #1 for DATA_8AA125 |
| `2` | `$8AA1BE` | `CODE_8AA1BE` | State handler #2 for DATA_8AA125 |
| `3` | `$8AA1BE` | `CODE_8AA1BE` | State handler #3 for DATA_8AA125 |
| `4` | `$8AA1BF` | `CODE_8AA1BF` | State handler #4 for DATA_8AA125 |
| `5` | `$8AA1BF` | `CODE_8AA1BF` | State handler #5 for DATA_8AA125 |
| `6` | `$8AA1CA` | `CODE_8AA1CA` | State handler #6 for DATA_8AA125 |
| `7` | `$8AA1E0` | `CODE_8AA1E0` | State handler #7 for DATA_8AA125 |
| `8` | `$8AA137` | `CODE_8AA137` | State handler #8 for DATA_8AA125 |

### Table 5: `CODE_8AA26B` -> `DATA_8AA27E` ($A27B -> $A27E)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:A27B` (`JMP.w (DATA_8AA27E,x)`)
- **Table Base:** `$8A:A27E` (9 target routines)
- **Index Selector:** `Unknown`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AA291` | `CODE_8AA291` | State handler #0 for DATA_8AA27E |
| `1` | `$8AA291` | `CODE_8AA291` | State handler #1 for DATA_8AA27E |
| `2` | `$8AA290` | `CODE_8AA290` | State handler #2 for DATA_8AA27E |
| `3` | `$8AA290` | `CODE_8AA290` | State handler #3 for DATA_8AA27E |
| `4` | `$8AA290` | `CODE_8AA290` | State handler #4 for DATA_8AA27E |
| `5` | `$8AA290` | `CODE_8AA290` | State handler #5 for DATA_8AA27E |
| `6` | `$8AA290` | `CODE_8AA290` | State handler #6 for DATA_8AA27E |
| `7` | `$8AA290` | `CODE_8AA290` | State handler #7 for DATA_8AA27E |
| `8` | `$8AA290` | `CODE_8AA290` | State handler #8 for DATA_8AA27E |

### Table 6: `CODE_8AAAAF` -> `DATA_8AAAB6` ($AAB3 -> $AAB6)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (7 states)
- **Dispatch Site:** `$8A:AAB3` (`JMP.w (DATA_8AAAB6,x)`)
- **Table Base:** `$8A:AAB6` (7 target routines)
- **Index Selector:** `LDA.b $70`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AAAC4` | `CODE_8AAAC4` | State handler #0 for DATA_8AAAB6 |
| `1` | `$8AAAC9` | `CODE_8AAAC9` | State handler #1 for DATA_8AAAB6 |
| `2` | `$8AAAD6` | `CODE_8AAAD6` | State handler #2 for DATA_8AAAB6 |
| `3` | `$8AAB69` | `CODE_8AAB69` | State handler #3 for DATA_8AAAB6 |
| `4` | `$8AAB7B` | `CODE_8AAB7B` | State handler #4 for DATA_8AAAB6 |
| `5` | `$8AABC0` | `CODE_8AABC0` | State handler #5 for DATA_8AAAB6 |
| `6` | `$8AAAC8` | `CODE_8AAAC8` | State handler #6 for DATA_8AAAB6 |

### Table 7: `CODE_8AAC71` -> `DATA_8AAC87` ($AC84 -> $AC87)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:AC84` (`JMP.w (DATA_8AAC87,x)`)
- **Table Base:** `$8A:AC87` (9 target routines)
- **Index Selector:** `LDX.w #$0400`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AAC9A` | `CODE_8AAC9A` | State handler #0 for DATA_8AAC87 |
| `1` | `$8AACD3` | `CODE_8AACD3` | State handler #1 for DATA_8AAC87 |
| `2` | `$8AAC99` | `CODE_8AAC99` | State handler #2 for DATA_8AAC87 |
| `3` | `$8AAC99` | `CODE_8AAC99` | State handler #3 for DATA_8AAC87 |
| `4` | `$8AACF4` | `CODE_8AACF4` | State handler #4 for DATA_8AAC87 |
| `5` | `$8AACF4` | `CODE_8AACF4` | State handler #5 for DATA_8AAC87 |
| `6` | `$8AAC99` | `CODE_8AAC99` | State handler #6 for DATA_8AAC87 |
| `7` | `$8AAC99` | `CODE_8AAC99` | State handler #7 for DATA_8AAC87 |
| `8` | `$8AAC99` | `CODE_8AAC99` | State handler #8 for DATA_8AAC87 |

### Table 8: `CODE_8AAD8B` -> `DATA_8AADA1` ($AD9E -> $ADA1)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:AD9E` (`JMP.w (DATA_8AADA1,x)`)
- **Table Base:** `$8A:ADA1` (9 target routines)
- **Index Selector:** `LDX.w #$0400`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AADB4` | `CODE_8AADB4` | State handler #0 for DATA_8AADA1 |
| `1` | `$8AADCE` | `CODE_8AADCE` | State handler #1 for DATA_8AADA1 |
| `2` | `$8AADB3` | `CODE_8AADB3` | State handler #2 for DATA_8AADA1 |
| `3` | `$8AADB3` | `CODE_8AADB3` | State handler #3 for DATA_8AADA1 |
| `4` | `$8AADDE` | `CODE_8AADDE` | State handler #4 for DATA_8AADA1 |
| `5` | `$8AADDE` | `CODE_8AADDE` | State handler #5 for DATA_8AADA1 |
| `6` | `$8AADB3` | `CODE_8AADB3` | State handler #6 for DATA_8AADA1 |
| `7` | `$8AADB3` | `CODE_8AADB3` | State handler #7 for DATA_8AADA1 |
| `8` | `$8AADB3` | `CODE_8AADB3` | State handler #8 for DATA_8AADA1 |

### Table 9: `CODE_8AAE85` -> `DATA_8AAE93` ($AE90 -> $AE93)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:AE90` (`JMP.w (DATA_8AAE93,x)`)
- **Table Base:** `$8A:AE93` (9 target routines)
- **Index Selector:** `LDX.w #$0400`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AAF05` | `CODE_8AAF05` | State handler #0 for DATA_8AAE93 |
| `1` | `$8AAF80` | `CODE_8AAF80` | State handler #1 for DATA_8AAE93 |
| `2` | `$8AAEA5` | `CODE_8AAEA5` | State handler #2 for DATA_8AAE93 |
| `3` | `$8AAEA5` | `CODE_8AAEA5` | State handler #3 for DATA_8AAE93 |
| `4` | `$8AAFC4` | `CODE_8AAFC4` | State handler #4 for DATA_8AAE93 |
| `5` | `$8AAFC4` | `CODE_8AAFC4` | State handler #5 for DATA_8AAE93 |
| `6` | `$8AAEA5` | `CODE_8AAEA5` | State handler #6 for DATA_8AAE93 |
| `7` | `$8AAEA5` | `CODE_8AAEA5` | State handler #7 for DATA_8AAE93 |
| `8` | `$8AAEA5` | `CODE_8AAEA5` | State handler #8 for DATA_8AAE93 |

### Table 10: `CODE_8AB04B` -> `DATA_8AB066` ($B063 -> $B066)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:B063` (`JMP.w (DATA_8AB066,x)`)
- **Table Base:** `$8A:B066` (9 target routines)
- **Index Selector:** `LDX.w #$0400`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AB079` | `CODE_8AB079` | State handler #0 for DATA_8AB066 |
| `1` | `$8AB0E5` | `CODE_8AB0E5` | State handler #1 for DATA_8AB066 |
| `2` | `$8AB078` | `CODE_8AB078` | State handler #2 for DATA_8AB066 |
| `3` | `$8AB078` | `CODE_8AB078` | State handler #3 for DATA_8AB066 |
| `4` | `$8AB147` | `CODE_8AB147` | State handler #4 for DATA_8AB066 |
| `5` | `$8AB15F` | `CODE_8AB15F` | State handler #5 for DATA_8AB066 |
| `6` | `$8AB129` | `CODE_8AB129` | State handler #6 for DATA_8AB066 |
| `7` | `$8AB137` | `CODE_8AB137` | State handler #7 for DATA_8AB066 |
| `8` | `$8AB078` | `CODE_8AB078` | State handler #8 for DATA_8AB066 |

### Table 11: `CODE_8AB1E0` -> `DATA_8AB1EF` ($B1EC -> $B1EF)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (9 states)
- **Dispatch Site:** `$8A:B1EC` (`JMP.w (DATA_8AB1EF,x)`)
- **Table Base:** `$8A:B1EF` (9 target routines)
- **Index Selector:** `LDA.w #CODE_8AB1E0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AB202` | `CODE_8AB202` | State handler #0 for DATA_8AB1EF |
| `1` | `$8AB202` | `CODE_8AB202` | State handler #1 for DATA_8AB1EF |
| `2` | `$8AB201` | `CODE_8AB201` | State handler #2 for DATA_8AB1EF |
| `3` | `$8AB201` | `CODE_8AB201` | State handler #3 for DATA_8AB1EF |
| `4` | `$8AB20E` | `CODE_8AB20E` | State handler #4 for DATA_8AB1EF |
| `5` | `$8AB20E` | `CODE_8AB20E` | State handler #5 for DATA_8AB1EF |
| `6` | `$8AB201` | `CODE_8AB201` | State handler #6 for DATA_8AB1EF |
| `7` | `$8AB201` | `CODE_8AB201` | State handler #7 for DATA_8AB1EF |
| `8` | `$8AB201` | `CODE_8AB201` | State handler #8 for DATA_8AB1EF |

### Table 12: `CODE_8AE1FF` -> `DATA_8AE20C` ($E209 -> $E20C)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (13 states)
- **Dispatch Site:** `$8A:E209` (`JMP.w (DATA_8AE20C,x)`)
- **Table Base:** `$8A:E20C` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AE227` | `CODE_8AE227` | State handler #0 for DATA_8AE20C |
| `1` | `$8AE258` | `CODE_8AE258` | State handler #1 for DATA_8AE20C |
| `2` | `$8AE280` | `CODE_8AE280` | State handler #2 for DATA_8AE20C |
| `3` | `$8AE280` | `CODE_8AE280` | State handler #3 for DATA_8AE20C |
| `4` | `$8AE226` | `CODE_8AE226` | State handler #4 for DATA_8AE20C |
| `5` | `$8AE226` | `CODE_8AE226` | State handler #5 for DATA_8AE20C |
| `6` | `$8AE280` | `CODE_8AE280` | State handler #6 for DATA_8AE20C |
| `7` | `$8AE280` | `CODE_8AE280` | State handler #7 for DATA_8AE20C |
| `8` | `$8AE281` | `CODE_8AE281` | State handler #8 for DATA_8AE20C |
| `9` | `$8AE2C8` | `CODE_8AE2C8` | State handler #9 for DATA_8AE20C |
| `10` | `$8AE226` | `CODE_8AE226` | State handler #10 for DATA_8AE20C |
| `11` | `$8AE226` | `CODE_8AE226` | State handler #11 for DATA_8AE20C |
| `12` | `$8AE226` | `CODE_8AE226` | State handler #12 for DATA_8AE20C |

### Table 13: `CODE_8AE859` -> `DATA_8AE885` ($E882 -> $E885)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (13 states)
- **Dispatch Site:** `$8A:E882` (`JMP.w (DATA_8AE885,x)`)
- **Table Base:** `$8A:E885` (13 target routines)
- **Index Selector:** `LDA.b $42`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AE8A0` | `CODE_8AE8A0` | State handler #0 for DATA_8AE885 |
| `1` | `$8AE8A3` | `CODE_8AE8A3` | State handler #1 for DATA_8AE885 |
| `2` | `$8AE8AF` | `CODE_8AE8AF` | State handler #2 for DATA_8AE885 |
| `3` | `$8AE8AF` | `CODE_8AE8AF` | State handler #3 for DATA_8AE885 |
| `4` | `$8AE89F` | `CODE_8AE89F` | State handler #4 for DATA_8AE885 |
| `5` | `$8AE89F` | `CODE_8AE89F` | State handler #5 for DATA_8AE885 |
| `6` | `$8AE8B0` | `CODE_8AE8B0` | State handler #6 for DATA_8AE885 |
| `7` | `$8AE8C6` | `CODE_8AE8C6` | State handler #7 for DATA_8AE885 |
| `8` | `$8AE8DF` | `CODE_8AE8DF` | State handler #8 for DATA_8AE885 |
| `9` | `$8AE8E9` | `CODE_8AE8E9` | State handler #9 for DATA_8AE885 |
| `10` | `$8AE89F` | `CODE_8AE89F` | State handler #10 for DATA_8AE885 |
| `11` | `$8AE89F` | `CODE_8AE89F` | State handler #11 for DATA_8AE885 |
| `12` | `$8AE89F` | `CODE_8AE89F` | State handler #12 for DATA_8AE885 |

### Table 14: `CODE_8AE8DF` -> `DATA_8AE8E6` ($E8E3 -> $E8E6)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (1 states)
- **Dispatch Site:** `$8A:E8E3` (`JMP.w (DATA_8AE8E6,x)`)
- **Table Base:** `$8A:E8E6` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AE8E8` | `CODE_8AE8E8` | State handler #0 for DATA_8AE8E6 |

### Table 15: `CODE_8AE8E9` -> `DATA_8AE8F0` ($E8ED -> $E8F0)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (1 states)
- **Dispatch Site:** `$8A:E8ED` (`JMP.w (DATA_8AE8F0,x)`)
- **Table Base:** `$8A:E8F0` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AE8F2` | `CODE_8AE8F2` | State handler #0 for DATA_8AE8F0 |

### Table 16: `CODE_8AEA20` -> `DATA_8AEA2B` ($EA28 -> $EA2B)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (13 states)
- **Dispatch Site:** `$8A:EA28` (`JMP.w (DATA_8AEA2B,x)`)
- **Table Base:** `$8A:EA2B` (13 target routines)
- **Index Selector:** `LDX.w #CODE_8AF132>>16`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AEA64` | `CODE_8AEA64` | State handler #0 for DATA_8AEA2B |
| `1` | `$8AEBB8` | `CODE_8AEBB8` | State handler #1 for DATA_8AEA2B |
| `2` | `$8AEBE8` | `CODE_8AEBE8` | State handler #2 for DATA_8AEA2B |
| `3` | `$8AEBE9` | `CODE_8AEBE9` | State handler #3 for DATA_8AEA2B |
| `4` | `$8AEA45` | `CODE_8AEA45` | State handler #4 for DATA_8AEA2B |
| `5` | `$8AEA4F` | `CODE_8AEA4F` | State handler #5 for DATA_8AEA2B |
| `6` | `$8AEBEA` | `CODE_8AEBEA` | State handler #6 for DATA_8AEA2B |
| `7` | `$8AEC0D` | `CODE_8AEC0D` | State handler #7 for DATA_8AEA2B |
| `8` | `$8AEC35` | `CODE_8AEC35` | State handler #8 for DATA_8AEA2B |
| `9` | `$8AEC73` | `CODE_8AEC73` | State handler #9 for DATA_8AEA2B |
| `10` | `$8AEA5C` | `CODE_8AEA5C` | State handler #10 for DATA_8AEA2B |
| `11` | `$8AEA63` | `CODE_8AEA63` | State handler #11 for DATA_8AEA2B |
| `12` | `$8AEA63` | `CODE_8AEA63` | State handler #12 for DATA_8AEA2B |

### Table 17: `CODE_8AEA64` -> `DATA_8AEA6B` ($EA68 -> $EA6B)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (3 states)
- **Dispatch Site:** `$8A:EA68` (`JMP.w (DATA_8AEA6B,x)`)
- **Table Base:** `$8A:EA6B` (3 target routines)
- **Index Selector:** `LDA.b $48`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AEA71` | `CODE_8AEA71` | State handler #0 for DATA_8AEA6B |
| `1` | `$8AEA9D` | `CODE_8AEA9D` | State handler #1 for DATA_8AEA6B |
| `2` | `$8AEAAD` | `CODE_8AEAAD` | State handler #2 for DATA_8AEA6B |

### Table 18: `CODE_8AF275` -> `DATA_8AF27C` ($F279 -> $F27C)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (13 states)
- **Dispatch Site:** `$8A:F279` (`JMP.w (DATA_8AF27C,x)`)
- **Table Base:** `$8A:F27C` (13 target routines)
- **Index Selector:** `LDA.w #CODE_8AF275>>16`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AF297` | `CODE_8AF297` | State handler #0 for DATA_8AF27C |
| `1` | `$8AF297` | `CODE_8AF297` | State handler #1 for DATA_8AF27C |
| `2` | `$8AF2B0` | `CODE_8AF2B0` | State handler #2 for DATA_8AF27C |
| `3` | `$8AF2B1` | `CODE_8AF2B1` | State handler #3 for DATA_8AF27C |
| `4` | `$8AF296` | `CODE_8AF296` | State handler #4 for DATA_8AF27C |
| `5` | `$8AF296` | `CODE_8AF296` | State handler #5 for DATA_8AF27C |
| `6` | `$8AF2B2` | `CODE_8AF2B2` | State handler #6 for DATA_8AF27C |
| `7` | `$8AF2BC` | `CODE_8AF2BC` | State handler #7 for DATA_8AF27C |
| `8` | `$8AF2C6` | `CODE_8AF2C6` | State handler #8 for DATA_8AF27C |
| `9` | `$8AF2D0` | `CODE_8AF2D0` | State handler #9 for DATA_8AF27C |
| `10` | `$8AF296` | `CODE_8AF296` | State handler #10 for DATA_8AF27C |
| `11` | `$8AF296` | `CODE_8AF296` | State handler #11 for DATA_8AF27C |
| `12` | `$8AF296` | `CODE_8AF296` | State handler #12 for DATA_8AF27C |

### Table 19: `CODE_8AF2B2` -> `DATA_8AF2B9` ($F2B6 -> $F2B9)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (1 states)
- **Dispatch Site:** `$8A:F2B6` (`JMP.w (DATA_8AF2B9,x)`)
- **Table Base:** `$8A:F2B9` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AF2BB` | `CODE_8AF2BB` | State handler #0 for DATA_8AF2B9 |

### Table 20: `CODE_8AF2BC` -> `DATA_8AF2C3` ($F2C0 -> $F2C3)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (1 states)
- **Dispatch Site:** `$8A:F2C0` (`JMP.w (DATA_8AF2C3,x)`)
- **Table Base:** `$8A:F2C3` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AF2C5` | `CODE_8AF2C5` | State handler #0 for DATA_8AF2C3 |

### Table 21: `CODE_8AF2C6` -> `DATA_8AF2CD` ($F2CA -> $F2CD)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (1 states)
- **Dispatch Site:** `$8A:F2CA` (`JMP.w (DATA_8AF2CD,x)`)
- **Table Base:** `$8A:F2CD` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AF2CF` | `CODE_8AF2CF` | State handler #0 for DATA_8AF2CD |

### Table 22: `CODE_8AF2D0` -> `DATA_8AF2D7` ($F2D4 -> $F2D7)

- **Subsystem Purpose:** Team Tactical Formation & CPU AI Decision Tree (1 states)
- **Dispatch Site:** `$8A:F2D4` (`JMP.w (DATA_8AF2D7,x)`)
- **Table Base:** `$8A:F2D7` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8AF2D9` | `CODE_8AF2D9` | State handler #0 for DATA_8AF2D7 |

## Bank $8B: Pitch Geometry, Metatile Streamer & Camera Movement

### Table 1: `CODE_8BB99C` -> `DATA_8BB9C5` ($B9C2 -> $B9C5)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:B9C2` (`JMP.w (DATA_8BB9C5,x)`)
- **Table Base:** `$8B:B9C5` (13 target routines)
- **Index Selector:** `LDA.w $0028,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BBA0C` | `CODE_8BBA0C` | State handler #0 for DATA_8BB9C5 |
| `1` | `$8BBACD` | `CODE_8BBACD` | State handler #1 for DATA_8BB9C5 |
| `2` | `$8BBB16` | `CODE_8BBB16` | State handler #2 for DATA_8BB9C5 |
| `3` | `$8BBB16` | `CODE_8BBB16` | State handler #3 for DATA_8BB9C5 |
| `4` | `$8BB9DF` | `CODE_8BB9DF` | State handler #4 for DATA_8BB9C5 |
| `5` | `$8BB9F4` | `CODE_8BB9F4` | State handler #5 for DATA_8BB9C5 |
| `6` | `$8BBB8E` | `CODE_8BBB8E` | State handler #6 for DATA_8BB9C5 |
| `7` | `$8BBB8E` | `CODE_8BBB8E` | State handler #7 for DATA_8BB9C5 |
| `8` | `$8BBB19` | `CODE_8BBB19` | State handler #8 for DATA_8BB9C5 |
| `9` | `$8BBB2A` | `CODE_8BBB2A` | State handler #9 for DATA_8BB9C5 |
| `10` | `$8BBA09` | `CODE_8BBA09` | State handler #10 for DATA_8BB9C5 |
| `11` | `$8BBA09` | `CODE_8BBA09` | State handler #11 for DATA_8BB9C5 |
| `12` | `$8BBA09` | `CODE_8BBA09` | State handler #12 for DATA_8BB9C5 |

### Table 2: `CODE_8BBF10` -> `DATA_8BBF13` ($BF10 -> $BF13)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:BF10` (`JMP.w (DATA_8BBF13,x)`)
- **Table Base:** `$8B:BF13` (13 target routines)
- **Index Selector:** `LDX.w #CODE_8BC193>>16`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BBF2E` | `CODE_8BBF2E` | State handler #0 for DATA_8BBF13 |
| `1` | `$8BBF2E` | `CODE_8BBF2E` | State handler #1 for DATA_8BBF13 |
| `2` | `$8BBF45` | `CODE_8BBF45` | State handler #2 for DATA_8BBF13 |
| `3` | `$8BBF45` | `CODE_8BBF45` | State handler #3 for DATA_8BBF13 |
| `4` | `$8BBF2D` | `CODE_8BBF2D` | State handler #4 for DATA_8BBF13 |
| `5` | `$8BBF2D` | `CODE_8BBF2D` | State handler #5 for DATA_8BBF13 |
| `6` | `$8BBF45` | `CODE_8BBF45` | State handler #6 for DATA_8BBF13 |
| `7` | `$8BBF45` | `CODE_8BBF45` | State handler #7 for DATA_8BBF13 |
| `8` | `$8BBF45` | `CODE_8BBF45` | State handler #8 for DATA_8BBF13 |
| `9` | `$8BBF45` | `CODE_8BBF45` | State handler #9 for DATA_8BBF13 |
| `10` | `$8BBF2D` | `CODE_8BBF2D` | State handler #10 for DATA_8BBF13 |
| `11` | `$8BBF2D` | `CODE_8BBF2D` | State handler #11 for DATA_8BBF13 |
| `12` | `$8BBF2D` | `CODE_8BBF2D` | State handler #12 for DATA_8BBF13 |

### Table 3: `CODE_8BC20B` -> `DATA_8BC212` ($C20F -> $C212)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:C20F` (`JMP.w (DATA_8BC212,x)`)
- **Table Base:** `$8B:C212` (13 target routines)
- **Index Selector:** `LDA.b $48`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BC23B` | `CODE_8BC23B` | State handler #0 for DATA_8BC212 |
| `1` | `$8BC23B` | `CODE_8BC23B` | State handler #1 for DATA_8BC212 |
| `2` | `$8BC23B` | `CODE_8BC23B` | State handler #2 for DATA_8BC212 |
| `3` | `$8BC23C` | `CODE_8BC23C` | State handler #3 for DATA_8BC212 |
| `4` | `$8BC22C` | `CODE_8BC22C` | State handler #4 for DATA_8BC212 |
| `5` | `$8BC22C` | `CODE_8BC22C` | State handler #5 for DATA_8BC212 |
| `6` | `$8BC23D` | `CODE_8BC23D` | State handler #6 for DATA_8BC212 |
| `7` | `$8BC23E` | `CODE_8BC23E` | State handler #7 for DATA_8BC212 |
| `8` | `$8BC23F` | `CODE_8BC23F` | State handler #8 for DATA_8BC212 |
| `9` | `$8BC240` | `CODE_8BC240` | State handler #9 for DATA_8BC212 |
| `10` | `$8BC22D` | `CODE_8BC22D` | State handler #10 for DATA_8BC212 |
| `11` | `$8BC23A` | `CODE_8BC23A` | State handler #11 for DATA_8BC212 |
| `12` | `$8BC23A` | `CODE_8BC23A` | State handler #12 for DATA_8BC212 |

### Table 4: `CODE_8BC258` -> `DATA_8BC2A5` ($C2A2 -> $C2A5)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:C2A2` (`JMP.w (DATA_8BC2A5,x)`)
- **Table Base:** `$8B:C2A5` (13 target routines)
- **Index Selector:** `LDA.l DATA_A4F859,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BC2C0` | `CODE_8BC2C0` | State handler #0 for DATA_8BC2A5 |
| `1` | `$8BC2D0` | `CODE_8BC2D0` | State handler #1 for DATA_8BC2A5 |
| `2` | `$8BC2D7` | `CODE_8BC2D7` | State handler #2 for DATA_8BC2A5 |
| `3` | `$8BC2D7` | `CODE_8BC2D7` | State handler #3 for DATA_8BC2A5 |
| `4` | `$8BC2BF` | `CODE_8BC2BF` | State handler #4 for DATA_8BC2A5 |
| `5` | `$8BC2BF` | `CODE_8BC2BF` | State handler #5 for DATA_8BC2A5 |
| `6` | `$8BC2D8` | `CODE_8BC2D8` | State handler #6 for DATA_8BC2A5 |
| `7` | `$8BC2EC` | `CODE_8BC2EC` | State handler #7 for DATA_8BC2A5 |
| `8` | `$8BC302` | `CODE_8BC302` | State handler #8 for DATA_8BC2A5 |
| `9` | `$8BC30F` | `CODE_8BC30F` | State handler #9 for DATA_8BC2A5 |
| `10` | `$8BC2BF` | `CODE_8BC2BF` | State handler #10 for DATA_8BC2A5 |
| `11` | `$8BC2BF` | `CODE_8BC2BF` | State handler #11 for DATA_8BC2A5 |
| `12` | `$8BC2BF` | `CODE_8BC2BF` | State handler #12 for DATA_8BC2A5 |

### Table 5: `CODE_8BC4E4` -> `DATA_8BC4EB` ($C4E8 -> $C4EB)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (11 states)
- **Dispatch Site:** `$8B:C4E8` (`JMP.w (DATA_8BC4EB,x)`)
- **Table Base:** `$8B:C4EB` (11 target routines)
- **Index Selector:** `LDA.b $44`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BC501` | `CODE_8BC501` | State handler #0 for DATA_8BC4EB |
| `1` | `$8BC50E` | `CODE_8BC50E` | State handler #1 for DATA_8BC4EB |
| `2` | `$8BC533` | `CODE_8BC533` | State handler #2 for DATA_8BC4EB |
| `3` | `$8BC586` | `CODE_8BC586` | State handler #3 for DATA_8BC4EB |
| `4` | `$8BC5A9` | `CODE_8BC5A9` | State handler #4 for DATA_8BC4EB |
| `5` | `$8BC5EF` | `CODE_8BC5EF` | State handler #5 for DATA_8BC4EB |
| `6` | `$8BC5FD` | `CODE_8BC5FD` | State handler #6 for DATA_8BC4EB |
| `7` | `$8BC619` | `CODE_8BC619` | State handler #7 for DATA_8BC4EB |
| `8` | `$8BC68A` | `CODE_8BC68A` | State handler #8 for DATA_8BC4EB |
| `9` | `$8BC6A7` | `CODE_8BC6A7` | State handler #9 for DATA_8BC4EB |
| `10` | `$8BC6B4` | `CODE_8BC6B4` | State handler #10 for DATA_8BC4EB |

### Table 6: `CODE_8BC89E` -> `DATA_8BC8CD` ($C8CA -> $C8CD)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:C8CA` (`JMP.w (DATA_8BC8CD,x)`)
- **Table Base:** `$8B:C8CD` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BC8E8` | `CODE_8BC8E8` | State handler #0 for DATA_8BC8CD |
| `1` | `$8BC90D` | `CODE_8BC90D` | State handler #1 for DATA_8BC8CD |
| `2` | `$8BC91A` | `CODE_8BC91A` | State handler #2 for DATA_8BC8CD |
| `3` | `$8BC91B` | `CODE_8BC91B` | State handler #3 for DATA_8BC8CD |
| `4` | `$8BC8E7` | `CODE_8BC8E7` | State handler #4 for DATA_8BC8CD |
| `5` | `$8BC8E7` | `CODE_8BC8E7` | State handler #5 for DATA_8BC8CD |
| `6` | `$8BC91C` | `CODE_8BC91C` | State handler #6 for DATA_8BC8CD |
| `7` | `$8BC92C` | `CODE_8BC92C` | State handler #7 for DATA_8BC8CD |
| `8` | `$8BC942` | `CODE_8BC942` | State handler #8 for DATA_8BC8CD |
| `9` | `$8BC942` | `CODE_8BC942` | State handler #9 for DATA_8BC8CD |
| `10` | `$8BC8E7` | `CODE_8BC8E7` | State handler #10 for DATA_8BC8CD |
| `11` | `$8BC8E7` | `CODE_8BC8E7` | State handler #11 for DATA_8BC8CD |
| `12` | `$8BC8E7` | `CODE_8BC8E7` | State handler #12 for DATA_8BC8CD |

### Table 7: `CODE_8BCA78` -> `DATA_8BCA85` ($CA82 -> $CA85)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:CA82` (`JMP.w (DATA_8BCA85,x)`)
- **Table Base:** `$8B:CA85` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BCAA0` | `CODE_8BCAA0` | State handler #0 for DATA_8BCA85 |
| `1` | `$8BCADF` | `CODE_8BCADF` | State handler #1 for DATA_8BCA85 |
| `2` | `$8BCAF7` | `CODE_8BCAF7` | State handler #2 for DATA_8BCA85 |
| `3` | `$8BCAF8` | `CODE_8BCAF8` | State handler #3 for DATA_8BCA85 |
| `4` | `$8BCA9F` | `CODE_8BCA9F` | State handler #4 for DATA_8BCA85 |
| `5` | `$8BCA9F` | `CODE_8BCA9F` | State handler #5 for DATA_8BCA85 |
| `6` | `$8BCAF9` | `CODE_8BCAF9` | State handler #6 for DATA_8BCA85 |
| `7` | `$8BCB17` | `CODE_8BCB17` | State handler #7 for DATA_8BCA85 |
| `8` | `$8BCB36` | `CODE_8BCB36` | State handler #8 for DATA_8BCA85 |
| `9` | `$8BCB44` | `CODE_8BCB44` | State handler #9 for DATA_8BCA85 |
| `10` | `$8BCA9F` | `CODE_8BCA9F` | State handler #10 for DATA_8BCA85 |
| `11` | `$8BCA9F` | `CODE_8BCA9F` | State handler #11 for DATA_8BCA85 |
| `12` | `$8BCA9F` | `CODE_8BCA9F` | State handler #12 for DATA_8BCA85 |

### Table 8: `CODE_8BCC97` -> `DATA_8BCC9E` ($CC9B -> $CC9E)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (5 states)
- **Dispatch Site:** `$8B:CC9B` (`JMP.w (DATA_8BCC9E,x)`)
- **Table Base:** `$8B:CC9E` (5 target routines)
- **Index Selector:** `LDA.b $60`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BCCA8` | `CODE_8BCCA8` | State handler #0 for DATA_8BCC9E |
| `1` | `$8BCCB5` | `CODE_8BCCB5` | State handler #1 for DATA_8BCC9E |
| `2` | `$8BCCC4` | `CODE_8BCCC4` | State handler #2 for DATA_8BCC9E |
| `3` | `$8BCCD8` | `CODE_8BCCD8` | State handler #3 for DATA_8BCC9E |
| `4` | `$8BCCEC` | `CODE_8BCCEC` | State handler #4 for DATA_8BCC9E |

### Table 9: `CODE_8BCE5B` -> `DATA_8BCE62` ($CE5F -> $CE62)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:CE5F` (`JMP.w (DATA_8BCE62,x)`)
- **Table Base:** `$8B:CE62` (13 target routines)
- **Index Selector:** `LDA.w #CODE_8BCE5B>>16`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BCEAD` | `CODE_8BCEAD` | State handler #0 for DATA_8BCE62 |
| `1` | `$8BCEAD` | `CODE_8BCEAD` | State handler #1 for DATA_8BCE62 |
| `2` | `$8BCEAD` | `CODE_8BCEAD` | State handler #2 for DATA_8BCE62 |
| `3` | `$8BCEAD` | `CODE_8BCEAD` | State handler #3 for DATA_8BCE62 |
| `4` | `$8BCE7C` | `CODE_8BCE7C` | State handler #4 for DATA_8BCE62 |
| `5` | `$8BCE7C` | `CODE_8BCE7C` | State handler #5 for DATA_8BCE62 |
| `6` | `$8BCEAD` | `CODE_8BCEAD` | State handler #6 for DATA_8BCE62 |
| `7` | `$8BCEAD` | `CODE_8BCEAD` | State handler #7 for DATA_8BCE62 |
| `8` | `$8BCEAD` | `CODE_8BCEAD` | State handler #8 for DATA_8BCE62 |
| `9` | `$8BCEAD` | `CODE_8BCEAD` | State handler #9 for DATA_8BCE62 |
| `10` | `$8BCE7D` | `CODE_8BCE7D` | State handler #10 for DATA_8BCE62 |
| `11` | `$8BCEAD` | `CODE_8BCEAD` | State handler #11 for DATA_8BCE62 |
| `12` | `$8BCEAD` | `CODE_8BCEAD` | State handler #12 for DATA_8BCE62 |

### Table 10: `CODE_8BD93F` -> `DATA_8BD969` ($D966 -> $D969)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (13 states)
- **Dispatch Site:** `$8B:D966` (`JMP.w (DATA_8BD969,x)`)
- **Table Base:** `$8B:D969` (13 target routines)
- **Index Selector:** `LDA.b $42`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BD984` | `CODE_8BD984` | State handler #0 for DATA_8BD969 |
| `1` | `$8BD987` | `CODE_8BD987` | State handler #1 for DATA_8BD969 |
| `2` | `$8BD9A3` | `CODE_8BD9A3` | State handler #2 for DATA_8BD969 |
| `3` | `$8BD9A4` | `CODE_8BD9A4` | State handler #3 for DATA_8BD969 |
| `4` | `$8BD983` | `CODE_8BD983` | State handler #4 for DATA_8BD969 |
| `5` | `$8BD983` | `CODE_8BD983` | State handler #5 for DATA_8BD969 |
| `6` | `$8BD9A5` | `CODE_8BD9A5` | State handler #6 for DATA_8BD969 |
| `7` | `$8BD9B6` | `CODE_8BD9B6` | State handler #7 for DATA_8BD969 |
| `8` | `$8BD9CA` | `CODE_8BD9CA` | State handler #8 for DATA_8BD969 |
| `9` | `$8BD9CB` | `CODE_8BD9CB` | State handler #9 for DATA_8BD969 |
| `10` | `$8BD983` | `CODE_8BD983` | State handler #10 for DATA_8BD969 |
| `11` | `$8BD983` | `CODE_8BD983` | State handler #11 for DATA_8BD969 |
| `12` | `$8BD983` | `CODE_8BD983` | State handler #12 for DATA_8BD969 |

### Table 11: `CODE_8BDBBB` -> `DATA_8BDBC4` ($DBC1 -> $DBC4)

- **Subsystem Purpose:** Pitch Geometry, Metatile Streamer & Camera Tracking (12 states)
- **Dispatch Site:** `$8B:DBC1` (`JMP.w (DATA_8BDBC4,x)`)
- **Table Base:** `$8B:DBC4` (12 target routines)
- **Index Selector:** `LDA.l $7EE52A`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8BDBDC` | `CODE_8BDBDC` | State handler #0 for DATA_8BDBC4 |
| `1` | `$8BDBDC` | `CODE_8BDBDC` | State handler #1 for DATA_8BDBC4 |
| `2` | `$8BDBDC` | `CODE_8BDBDC` | State handler #2 for DATA_8BDBC4 |
| `3` | `$8BDBDC` | `CODE_8BDBDC` | State handler #3 for DATA_8BDBC4 |
| `4` | `$8BDBDC` | `CODE_8BDBDC` | State handler #4 for DATA_8BDBC4 |
| `5` | `$8BDBDC` | `CODE_8BDBDC` | State handler #5 for DATA_8BDBC4 |
| `6` | `$8BDBDC` | `CODE_8BDBDC` | State handler #6 for DATA_8BDBC4 |
| `7` | `$8BDBDD` | `CODE_8BDBDD` | State handler #7 for DATA_8BDBC4 |
| `8` | `$8BDBDC` | `CODE_8BDBDC` | State handler #8 for DATA_8BDBC4 |
| `9` | `$8BDBDC` | `CODE_8BDBDC` | State handler #9 for DATA_8BDBC4 |
| `10` | `$8BDBDC` | `CODE_8BDBDC` | State handler #10 for DATA_8BDBC4 |
| `11` | `$8BDBDC` | `CODE_8BDBDC` | State handler #11 for DATA_8BDBC4 |

## Bank $8C: Scenario Match Situations & Tournament Progress

### Table 1: `CODE_8C88AB` -> `DATA_8C88B8` ($88B5 -> $88B8)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (13 states)
- **Dispatch Site:** `$8C:88B5` (`JMP.w (DATA_8C88B8,x)`)
- **Table Base:** `$8C:88B8` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8C8920` | `CODE_8C8920` | State handler #0 for DATA_8C88B8 |
| `1` | `$8C88D5` | `CODE_8C88D5` | State handler #1 for DATA_8C88B8 |
| `2` | `$8C88D2` | `CODE_8C88D2` | State handler #2 for DATA_8C88B8 |
| `3` | `$8C88D2` | `CODE_8C88D2` | State handler #3 for DATA_8C88B8 |
| `4` | `$8C88D3` | `CODE_8C88D3` | State handler #4 for DATA_8C88B8 |
| `5` | `$8C88D3` | `CODE_8C88D3` | State handler #5 for DATA_8C88B8 |
| `6` | `$8C8A05` | `CODE_8C8A05` | State handler #6 for DATA_8C88B8 |
| `7` | `$8C89AA` | `CODE_8C89AA` | State handler #7 for DATA_8C88B8 |
| `8` | `$8C88D4` | `CODE_8C88D4` | State handler #8 for DATA_8C88B8 |
| `9` | `$8C88D4` | `CODE_8C88D4` | State handler #9 for DATA_8C88B8 |
| `10` | `$8C88D2` | `CODE_8C88D2` | State handler #10 for DATA_8C88B8 |
| `11` | `$8C88D2` | `CODE_8C88D2` | State handler #11 for DATA_8C88B8 |
| `12` | `$8C88D2` | `CODE_8C88D2` | State handler #12 for DATA_8C88B8 |

### Table 2: `CODE_8C8A77` -> `DATA_8C8A81` ($8A7E -> $8A81)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (7 states)
- **Dispatch Site:** `$8C:8A7E` (`JMP.w (DATA_8C8A81,x)`)
- **Table Base:** `$8C:8A81` (7 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8C8A8F` | `CODE_8C8A8F` | State handler #0 for DATA_8C8A81 |
| `1` | `$8C8AAD` | `CODE_8C8AAD` | State handler #1 for DATA_8C8A81 |
| `2` | `$8C8B2F` | `CODE_8C8B2F` | State handler #2 for DATA_8C8A81 |
| `3` | `$8C8B2F` | `CODE_8C8B2F` | State handler #3 for DATA_8C8A81 |
| `4` | `$8C8ABD` | `CODE_8C8ABD` | State handler #4 for DATA_8C8A81 |
| `5` | `$8C8AE8` | `CODE_8C8AE8` | State handler #5 for DATA_8C8A81 |
| `6` | `$8C8B2F` | `CODE_8C8B2F` | State handler #6 for DATA_8C8A81 |

### Table 3: `CODE_8CAAA6` -> `DATA_8CAAAF` ($AAAC -> $AAAF)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (4 states)
- **Dispatch Site:** `$8C:AAAC` (`JMP.w (DATA_8CAAAF,x)`)
- **Table Base:** `$8C:AAAF` (4 target routines)
- **Index Selector:** `LDA.l $7ED70E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8CAADE` | `CODE_8CAADE` | State handler #0 for DATA_8CAAAF |
| `1` | `$8CAABC` | `CODE_8CAABC` | State handler #1 for DATA_8CAAAF |
| `2` | `$8CAAB7` | `CODE_8CAAB7` | State handler #2 for DATA_8CAAAF |
| `3` | `$8CAABC` | `CODE_8CAABC` | State handler #3 for DATA_8CAAAF |

### Table 4: `CODE_8CAB1B` -> `DATA_8CAB2B` ($AB28 -> $AB2B)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (4 states)
- **Dispatch Site:** `$8C:AB28` (`JMP.w (DATA_8CAB2B,x)`)
- **Table Base:** `$8C:AB2B` (4 target routines)
- **Index Selector:** `LDA.l $7ED70E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8CAB48` | `CODE_8CAB48` | State handler #0 for DATA_8CAB2B |
| `1` | `$8CAB48` | `CODE_8CAB48` | State handler #1 for DATA_8CAB2B |
| `2` | `$8CAB48` | `CODE_8CAB48` | State handler #2 for DATA_8CAB2B |
| `3` | `$8CAB48` | `CODE_8CAB48` | State handler #3 for DATA_8CAB2B |

### Table 5: `CODE_8CAB33` -> `DATA_8CAB3C` ($AB39 -> $AB3C)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (6 states)
- **Dispatch Site:** `$8C:AB39` (`JMP.w (DATA_8CAB3C,x)`)
- **Table Base:** `$8C:AB3C` (6 target routines)
- **Index Selector:** `LDA.l $7ED708`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8CAB49` | `CODE_8CAB49` | State handler #0 for DATA_8CAB3C |
| `1` | `$8CAB50` | `CODE_8CAB50` | State handler #1 for DATA_8CAB3C |
| `2` | `$8CAB94` | `CODE_8CAB94` | State handler #2 for DATA_8CAB3C |
| `3` | `$8CAB7A` | `CODE_8CAB7A` | State handler #3 for DATA_8CAB3C |
| `4` | `$8CAB94` | `CODE_8CAB94` | State handler #4 for DATA_8CAB3C |
| `5` | `$8CAB94` | `CODE_8CAB94` | State handler #5 for DATA_8CAB3C |

### Table 6: `CODE_8CABBE` -> `DATA_8CABFE` ($ABFB -> $ABFE)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (4 states)
- **Dispatch Site:** `$8C:ABFB` (`JMP.w (DATA_8CABFE,x)`)
- **Table Base:** `$8C:ABFE` (4 target routines)
- **Index Selector:** `LDA.w DATA_81F7EB,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8CAC87` | `CODE_8CAC87` | State handler #0 for DATA_8CABFE |
| `1` | `$8CAD1C` | `CODE_8CAD1C` | State handler #1 for DATA_8CABFE |
| `2` | `$8CADB3` | `CODE_8CADB3` | State handler #2 for DATA_8CABFE |
| `3` | `$8CAC38` | `CODE_8CAC38` | State handler #3 for DATA_8CABFE |

### Table 7: `CODE_8CAC06` -> `DATA_8CAC2C` ($AC29 -> $AC2C)

- **Subsystem Purpose:** Scenario Match Conditions & Tournament Progress Logic (6 states)
- **Dispatch Site:** `$8C:AC29` (`JMP.w (DATA_8CAC2C,x)`)
- **Table Base:** `$8C:AC2C` (6 target routines)
- **Index Selector:** `LDA.w DATA_81F7F3,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$8CAC5F` | `CODE_8CAC5F` | State handler #0 for DATA_8CAC2C |
| `1` | `$8CAC99` | `CODE_8CAC99` | State handler #1 for DATA_8CAC2C |
| `2` | `$8CACE0` | `CODE_8CACE0` | State handler #2 for DATA_8CAC2C |
| `3` | `$8CAD68` | `CODE_8CAD68` | State handler #3 for DATA_8CAC2C |
| `4` | `$8CAD85` | `CODE_8CAD85` | State handler #4 for DATA_8CAC2C |
| `5` | `$8CADC7` | `CODE_8CADC7` | State handler #5 for DATA_8CAC2C |

## Bank $A4: UI Screens, Team Selection, Tactics Board & Passwords

### Table 1: `CODE_A49D72` -> `DATA_A49D80` ($9D7D -> $9D80)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (9 states)
- **Dispatch Site:** `$A4:9D7D` (`JMP.w (DATA_A49D80,x)`)
- **Table Base:** `$A4:9D80` (9 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A49D92` | `CODE_A49D92` | State handler #0 for DATA_A49D80 |
| `1` | `$A49DD8` | `CODE_A49DD8` | State handler #1 for DATA_A49D80 |
| `2` | `$A49DDF` | `CODE_A49DDF` | State handler #2 for DATA_A49D80 |
| `3` | `$A49DE0` | `CODE_A49DE0` | State handler #3 for DATA_A49D80 |
| `4` | `$A49DE1` | `CODE_A49DE1` | State handler #4 for DATA_A49D80 |
| `5` | `$A49DFD` | `CODE_A49DFD` | State handler #5 for DATA_A49D80 |
| `6` | `$A49E1B` | `CODE_A49E1B` | State handler #6 for DATA_A49D80 |
| `7` | `$A49E2C` | `CODE_A49E2C` | State handler #7 for DATA_A49D80 |
| `8` | `$A49E1A` | `CODE_A49E1A` | State handler #8 for DATA_A49D80 |

### Table 2: `CODE_A49D92` -> `DATA_A49DA0` ($9D9D -> $9DA0)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (8 states)
- **Dispatch Site:** `$A4:9D9D` (`JMP.w (DATA_A49DA0,x)`)
- **Table Base:** `$A4:9DA0` (8 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A49DCA` | `CODE_A49DCA` | State handler #0 for DATA_A49DA0 |
| `1` | `$A49DB0` | `CODE_A49DB0` | State handler #1 for DATA_A49DA0 |
| `2` | `$A49DB0` | `CODE_A49DB0` | State handler #2 for DATA_A49DA0 |
| `3` | `$A49DCA` | `CODE_A49DCA` | State handler #3 for DATA_A49DA0 |
| `4` | `$A49DB0` | `CODE_A49DB0` | State handler #4 for DATA_A49DA0 |
| `5` | `$A49DCA` | `CODE_A49DCA` | State handler #5 for DATA_A49DA0 |
| `6` | `$A49DB0` | `CODE_A49DB0` | State handler #6 for DATA_A49DA0 |
| `7` | `$A49DCA` | `CODE_A49DCA` | State handler #7 for DATA_A49DA0 |

### Table 3: `CODE_A49EB7` -> `DATA_A49EDC` ($9ED9 -> $9EDC)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:9ED9` (`JMP.w (DATA_A49EDC,x)`)
- **Table Base:** `$A4:9EDC` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A49EF7` | `CODE_A49EF7` | State handler #0 for DATA_A49EDC |
| `1` | `$A49F15` | `CODE_A49F15` | State handler #1 for DATA_A49EDC |
| `2` | `$A49F21` | `CODE_A49F21` | State handler #2 for DATA_A49EDC |
| `3` | `$A49F21` | `CODE_A49F21` | State handler #3 for DATA_A49EDC |
| `4` | `$A49F30` | `CODE_A49F30` | State handler #4 for DATA_A49EDC |
| `5` | `$A49F5D` | `CODE_A49F5D` | State handler #5 for DATA_A49EDC |
| `6` | `$A49F22` | `CODE_A49F22` | State handler #6 for DATA_A49EDC |
| `7` | `$A49F22` | `CODE_A49F22` | State handler #7 for DATA_A49EDC |
| `8` | `$A49F30` | `CODE_A49F30` | State handler #8 for DATA_A49EDC |
| `9` | `$A49F5D` | `CODE_A49F5D` | State handler #9 for DATA_A49EDC |
| `10` | `$A49EF6` | `CODE_A49EF6` | State handler #10 for DATA_A49EDC |
| `11` | `$A49EF6` | `CODE_A49EF6` | State handler #11 for DATA_A49EDC |
| `12` | `$A49EF6` | `CODE_A49EF6` | State handler #12 for DATA_A49EDC |

### Table 4: `CODE_A49F30` -> `DATA_A49F37` ($9F34 -> $9F37)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (2 states)
- **Dispatch Site:** `$A4:9F34` (`JMP.w (DATA_A49F37,x)`)
- **Table Base:** `$A4:9F37` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A49F3B` | `CODE_A49F3B` | State handler #0 for DATA_A49F37 |
| `1` | `$A49F4D` | `CODE_A49F4D` | State handler #1 for DATA_A49F37 |

### Table 5: `CODE_A49F5D` -> `DATA_A49F64` ($9F61 -> $9F64)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (2 states)
- **Dispatch Site:** `$A4:9F61` (`JMP.w (DATA_A49F64,x)`)
- **Table Base:** `$A4:9F64` (2 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A49F68` | `CODE_A49F68` | State handler #0 for DATA_A49F64 |
| `1` | `$A49F75` | `CODE_A49F75` | State handler #1 for DATA_A49F64 |

### Table 6: `CODE_A4A9BC` -> `DATA_A4A9E6` ($A9E3 -> $A9E6)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:A9E3` (`JMP.w (DATA_A4A9E6,x)`)
- **Table Base:** `$A4:A9E6` (13 target routines)
- **Index Selector:** `LDA.b $42`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4AA01` | `CODE_A4AA01` | State handler #0 for DATA_A4A9E6 |
| `1` | `$A4AA04` | `CODE_A4AA04` | State handler #1 for DATA_A4A9E6 |
| `2` | `$A4AA20` | `CODE_A4AA20` | State handler #2 for DATA_A4A9E6 |
| `3` | `$A4AA21` | `CODE_A4AA21` | State handler #3 for DATA_A4A9E6 |
| `4` | `$A4AA00` | `CODE_A4AA00` | State handler #4 for DATA_A4A9E6 |
| `5` | `$A4AA00` | `CODE_A4AA00` | State handler #5 for DATA_A4A9E6 |
| `6` | `$A4AA22` | `CODE_A4AA22` | State handler #6 for DATA_A4A9E6 |
| `7` | `$A4AA33` | `CODE_A4AA33` | State handler #7 for DATA_A4A9E6 |
| `8` | `$A4AA47` | `CODE_A4AA47` | State handler #8 for DATA_A4A9E6 |
| `9` | `$A4AA48` | `CODE_A4AA48` | State handler #9 for DATA_A4A9E6 |
| `10` | `$A4AA00` | `CODE_A4AA00` | State handler #10 for DATA_A4A9E6 |
| `11` | `$A4AA00` | `CODE_A4AA00` | State handler #11 for DATA_A4A9E6 |
| `12` | `$A4AA00` | `CODE_A4AA00` | State handler #12 for DATA_A4A9E6 |

### Table 7: `CODE_A4AB09` -> `DATA_A4AB17` ($AB14 -> $AB17)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:AB14` (`JMP.w (DATA_A4AB17,x)`)
- **Table Base:** `$A4:AB17` (13 target routines)
- **Index Selector:** `LDA.w $0028,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4AB5D` | `CODE_A4AB5D` | State handler #0 for DATA_A4AB17 |
| `1` | `$A4ABB3` | `CODE_A4ABB3` | State handler #1 for DATA_A4AB17 |
| `2` | `$A4ABC9` | `CODE_A4ABC9` | State handler #2 for DATA_A4AB17 |
| `3` | `$A4ABC9` | `CODE_A4ABC9` | State handler #3 for DATA_A4AB17 |
| `4` | `$A4AB46` | `CODE_A4AB46` | State handler #4 for DATA_A4AB17 |
| `5` | `$A4AB31` | `CODE_A4AB31` | State handler #5 for DATA_A4AB17 |
| `6` | `$A4ABCA` | `CODE_A4ABCA` | State handler #6 for DATA_A4AB17 |
| `7` | `$A4ABDF` | `CODE_A4ABDF` | State handler #7 for DATA_A4AB17 |
| `8` | `$A4ABEF` | `CODE_A4ABEF` | State handler #8 for DATA_A4AB17 |
| `9` | `$A4AC0D` | `CODE_A4AC0D` | State handler #9 for DATA_A4AB17 |
| `10` | `$A4AB5B` | `CODE_A4AB5B` | State handler #10 for DATA_A4AB17 |
| `11` | `$A4AB5C` | `CODE_A4AB5C` | State handler #11 for DATA_A4AB17 |
| `12` | `$A4AB5C` | `CODE_A4AB5C` | State handler #12 for DATA_A4AB17 |

### Table 8: `CODE_A4BE53` -> `DATA_A4BE66` ($BE63 -> $BE66)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:BE63` (`JMP.w (DATA_A4BE66,x)`)
- **Table Base:** `$A4:BE66` (13 target routines)
- **Index Selector:** `LDA.w DATA_81D8ED,x`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4BE82` | `CODE_A4BE82` | State handler #0 for DATA_A4BE66 |
| `1` | `$A4BF08` | `CODE_A4BF08` | State handler #1 for DATA_A4BE66 |
| `2` | `$A4BF3F` | `CODE_A4BF3F` | State handler #2 for DATA_A4BE66 |
| `3` | `$A4BF40` | `CODE_A4BF40` | State handler #3 for DATA_A4BE66 |
| `4` | `$A4BE80` | `CODE_A4BE80` | State handler #4 for DATA_A4BE66 |
| `5` | `$A4BE80` | `CODE_A4BE80` | State handler #5 for DATA_A4BE66 |
| `6` | `$A4BF41` | `CODE_A4BF41` | State handler #6 for DATA_A4BE66 |
| `7` | `$A4BF57` | `CODE_A4BF57` | State handler #7 for DATA_A4BE66 |
| `8` | `$A4BF6E` | `CODE_A4BF6E` | State handler #8 for DATA_A4BE66 |
| `9` | `$A4BF6F` | `CODE_A4BF6F` | State handler #9 for DATA_A4BE66 |
| `10` | `$A4BE81` | `CODE_A4BE81` | State handler #10 for DATA_A4BE66 |
| `11` | `$A4BE81` | `CODE_A4BE81` | State handler #11 for DATA_A4BE66 |
| `12` | `$A4BE81` | `CODE_A4BE81` | State handler #12 for DATA_A4BE66 |

### Table 9: `CODE_A4C14A` -> `DATA_A4C151` ($C14E -> $C151)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:C14E` (`JMP.w (DATA_A4C151,x)`)
- **Table Base:** `$A4:C151` (13 target routines)
- **Index Selector:** `LDA.w #CODE_A4C14A>>16`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4C1C9` | `CODE_A4C1C9` | State handler #0 for DATA_A4C151 |
| `1` | `$A4C1C9` | `CODE_A4C1C9` | State handler #1 for DATA_A4C151 |
| `2` | `$A4C1C9` | `CODE_A4C1C9` | State handler #2 for DATA_A4C151 |
| `3` | `$A4C1C9` | `CODE_A4C1C9` | State handler #3 for DATA_A4C151 |
| `4` | `$A4C16B` | `CODE_A4C16B` | State handler #4 for DATA_A4C151 |
| `5` | `$A4C16B` | `CODE_A4C16B` | State handler #5 for DATA_A4C151 |
| `6` | `$A4C1C9` | `CODE_A4C1C9` | State handler #6 for DATA_A4C151 |
| `7` | `$A4C1C9` | `CODE_A4C1C9` | State handler #7 for DATA_A4C151 |
| `8` | `$A4C1C9` | `CODE_A4C1C9` | State handler #8 for DATA_A4C151 |
| `9` | `$A4C1C9` | `CODE_A4C1C9` | State handler #9 for DATA_A4C151 |
| `10` | `$A4C16C` | `CODE_A4C16C` | State handler #10 for DATA_A4C151 |
| `11` | `$A4C1C9` | `CODE_A4C1C9` | State handler #11 for DATA_A4C151 |
| `12` | `$A4C1C9` | `CODE_A4C1C9` | State handler #12 for DATA_A4C151 |

### Table 10: `CODE_A4C33D` -> `DATA_A4C353` ($C350 -> $C353)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:C350` (`JMP.w (DATA_A4C353,x)`)
- **Table Base:** `$A4:C353` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4C36E` | `CODE_A4C36E` | State handler #0 for DATA_A4C353 |
| `1` | `$A4C394` | `CODE_A4C394` | State handler #1 for DATA_A4C353 |
| `2` | `$A4C3A0` | `CODE_A4C3A0` | State handler #2 for DATA_A4C353 |
| `3` | `$A4C3A1` | `CODE_A4C3A1` | State handler #3 for DATA_A4C353 |
| `4` | `$A4C36D` | `CODE_A4C36D` | State handler #4 for DATA_A4C353 |
| `5` | `$A4C36D` | `CODE_A4C36D` | State handler #5 for DATA_A4C353 |
| `6` | `$A4C3A2` | `CODE_A4C3A2` | State handler #6 for DATA_A4C353 |
| `7` | `$A4C3BF` | `CODE_A4C3BF` | State handler #7 for DATA_A4C353 |
| `8` | `$A4C3DE` | `CODE_A4C3DE` | State handler #8 for DATA_A4C353 |
| `9` | `$A4C3E8` | `CODE_A4C3E8` | State handler #9 for DATA_A4C353 |
| `10` | `$A4C36D` | `CODE_A4C36D` | State handler #10 for DATA_A4C353 |
| `11` | `$A4C36D` | `CODE_A4C36D` | State handler #11 for DATA_A4C353 |
| `12` | `$A4C36D` | `CODE_A4C36D` | State handler #12 for DATA_A4C353 |

### Table 11: `CODE_A4C3DE` -> `DATA_A4C3E5` ($C3E2 -> $C3E5)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (1 states)
- **Dispatch Site:** `$A4:C3E2` (`JMP.w (DATA_A4C3E5,x)`)
- **Table Base:** `$A4:C3E5` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4C3E7` | `CODE_A4C3E7` | State handler #0 for DATA_A4C3E5 |

### Table 12: `CODE_A4C3E8` -> `DATA_A4C3EF` ($C3EC -> $C3EF)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (1 states)
- **Dispatch Site:** `$A4:C3EC` (`JMP.w (DATA_A4C3EF,x)`)
- **Table Base:** `$A4:C3EF` (1 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4C3F1` | `CODE_A4C3F1` | State handler #0 for DATA_A4C3EF |

### Table 13: `CODE_A4CB82` -> `DATA_A4CB89` ($CB86 -> $CB89)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (13 states)
- **Dispatch Site:** `$A4:CB86` (`JMP.w (DATA_A4CB89,x)`)
- **Table Base:** `$A4:CB89` (13 target routines)
- **Index Selector:** `LDA.w $140E`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4CBBF` | `CODE_A4CBBF` | State handler #0 for DATA_A4CB89 |
| `1` | `$A4CD31` | `CODE_A4CD31` | State handler #1 for DATA_A4CB89 |
| `2` | `$A4CBA3` | `CODE_A4CBA3` | State handler #2 for DATA_A4CB89 |
| `3` | `$A4CBA3` | `CODE_A4CBA3` | State handler #3 for DATA_A4CB89 |
| `4` | `$A4CBA3` | `CODE_A4CBA3` | State handler #4 for DATA_A4CB89 |
| `5` | `$A4CBA3` | `CODE_A4CBA3` | State handler #5 for DATA_A4CB89 |
| `6` | `$A4CD68` | `CODE_A4CD68` | State handler #6 for DATA_A4CB89 |
| `7` | `$A4CE08` | `CODE_A4CE08` | State handler #7 for DATA_A4CB89 |
| `8` | `$A4CE72` | `CODE_A4CE72` | State handler #8 for DATA_A4CB89 |
| `9` | `$A4CEF9` | `CODE_A4CEF9` | State handler #9 for DATA_A4CB89 |
| `10` | `$A4CBA3` | `CODE_A4CBA3` | State handler #10 for DATA_A4CB89 |
| `11` | `$A4CBA4` | `CODE_A4CBA4` | State handler #11 for DATA_A4CB89 |
| `12` | `$A4CBBE` | `CODE_A4CBBE` | State handler #12 for DATA_A4CB89 |

### Table 14: `CODE_A4D20F` -> `DATA_A4D216` ($D213 -> $D216)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (3 states)
- **Dispatch Site:** `$A4:D213` (`JMP.w (DATA_A4D216,x)`)
- **Table Base:** `$A4:D216` (3 target routines)
- **Index Selector:** `LDA.b $1C`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4D21C` | `CODE_A4D21C` | State handler #0 for DATA_A4D216 |
| `1` | `$A4D23C` | `CODE_A4D23C` | State handler #1 for DATA_A4D216 |
| `2` | `$A4D25E` | `CODE_A4D25E` | State handler #2 for DATA_A4D216 |

### Table 15: `CODE_A4D68F` -> `DATA_A4D696` ($D693 -> $D696)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (14 states)
- **Dispatch Site:** `$A4:D693` (`JMP.w (DATA_A4D696,x)`)
- **Table Base:** `$A4:D696` (14 target routines)
- **Index Selector:** `LDA.b $C0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4D6B2` | `CODE_A4D6B2` | State handler #0 for DATA_A4D696 |
| `1` | `$A4D6C5` | `CODE_A4D6C5` | State handler #1 for DATA_A4D696 |
| `2` | `$A4D6C5` | `CODE_A4D6C5` | State handler #2 for DATA_A4D696 |
| `3` | `$A4D6C5` | `CODE_A4D6C5` | State handler #3 for DATA_A4D696 |
| `4` | `$A4D6C5` | `CODE_A4D6C5` | State handler #4 for DATA_A4D696 |
| `5` | `$A4D6C5` | `CODE_A4D6C5` | State handler #5 for DATA_A4D696 |
| `6` | `$A4D6C5` | `CODE_A4D6C5` | State handler #6 for DATA_A4D696 |
| `7` | `$A4D6B2` | `CODE_A4D6B2` | State handler #7 for DATA_A4D696 |
| `8` | `$A4D6B2` | `CODE_A4D6B2` | State handler #8 for DATA_A4D696 |
| `9` | `$A4D6B2` | `CODE_A4D6B2` | State handler #9 for DATA_A4D696 |
| `10` | `$A4D6B2` | `CODE_A4D6B2` | State handler #10 for DATA_A4D696 |
| `11` | `$A4D6C5` | `CODE_A4D6C5` | State handler #11 for DATA_A4D696 |
| `12` | `$A4D6C5` | `CODE_A4D6C5` | State handler #12 for DATA_A4D696 |
| `13` | `$A4D6C5` | `CODE_A4D6C5` | State handler #13 for DATA_A4D696 |

### Table 16: `CODE_A4D804` -> `DATA_A4D809` ($D806 -> $D809)

- **Subsystem Purpose:** User Interface & Menu Screen Navigation State Machine (15 states)
- **Dispatch Site:** `$A4:D806` (`JMP.w (DATA_A4D809,x)`)
- **Table Base:** `$A4:D809` (15 target routines)
- **Index Selector:** `LDA.b $C0`

| Index | Target Address | Label | Target Routine Purpose |
|:---:|:---|:---|:---|
| `0` | `$A4D868` | `CODE_A4D868` | State handler #0 for DATA_A4D809 |
| `1` | `$A4D918` | `CODE_A4D918` | State handler #1 for DATA_A4D809 |
| `2` | `$A4D918` | `CODE_A4D918` | State handler #2 for DATA_A4D809 |
| `3` | `$A4D918` | `CODE_A4D918` | State handler #3 for DATA_A4D809 |
| `4` | `$A4D918` | `CODE_A4D918` | State handler #4 for DATA_A4D809 |
| `5` | `$A4D918` | `CODE_A4D918` | State handler #5 for DATA_A4D809 |
| `6` | `$A4D827` | `CODE_A4D827` | State handler #6 for DATA_A4D809 |
| `7` | `$A4D8AA` | `CODE_A4D8AA` | State handler #7 for DATA_A4D809 |
| `8` | `$A4D8AA` | `CODE_A4D8AA` | State handler #8 for DATA_A4D809 |
| `9` | `$A4D898` | `CODE_A4D898` | State handler #9 for DATA_A4D809 |
| `10` | `$A4D8AA` | `CODE_A4D8AA` | State handler #10 for DATA_A4D809 |
| `11` | `$A4D8AA` | `CODE_A4D8AA` | State handler #11 for DATA_A4D809 |
| `12` | `$A4D845` | `CODE_A4D845` | State handler #12 for DATA_A4D809 |
| `13` | `$A4D845` | `CODE_A4D845` | State handler #13 for DATA_A4D809 |
| `14` | `$A4D84B` | `CODE_A4D84B` | State handler #14 for DATA_A4D809 |

