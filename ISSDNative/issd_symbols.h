#ifndef ISSD_SYMBOLS_H
#define ISSD_SYMBOLS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ISSD Native — Global Semantic Symbol Directory & Hardware Aliases
 * ============================================================================
 * This header provides semantic names, memory offsets, routine aliases, and
 * enumeration constants for International Superstar Soccer Deluxe (SNES USA).
 * It enables any AI model or developer to quickly read, navigate, and modify
 * the game engine across Rendering, Menus, Simulation, and Audio.
 * ============================================================================ */

/* ----------------------------------------------------------------------------
 * 1. CORE ENGINE ROUTINE ALIASES
 * ----------------------------------------------------------------------------
 * Maps native 24-bit SNES code addresses and recompiled symbols to human-readable
 * semantic identifiers.
 * ---------------------------------------------------------------------------- */

/* Entry Vectors & Main Loop */
#define ISSD_ADDR_RESET_VECTOR                  0x808000u /* Reset Vector: Boot & initialization sequence */
#define ISSD_ADDR_NMI_HANDLER                   0x8080E0u /* NMI Handler: Vertical blanking & DMA execution */
#define ISSD_ADDR_IRQ_HANDLER                   0x8081A4u /* IRQ Handler: Raster line interrupts */
#define ISSD_ADDR_MAIN_GAME_LOOP                0x80844Cu /* Main engine tick & subsystem dispatcher */

/* Asset Decompression & Memory Streaming */
#define ISSD_ADDR_DECOMPRESS_VRAM_ASSET         0x80B527u /* Decompresses asset package to VRAM or WRAM */
#define ISSD_ADDR_DECOMPRESS_BLOCK_LOOP         0x80BBA6u /* Inner decompression transfer setup */
#define ISSD_ADDR_RAM_DECOMPRESS_TRAMPOLINE     0x001E40u /* Dynamic WRAM MVN trampoline: transfers decompressed blocks */
#define ISSD_ADDR_PITCH_STREAM_ROW              0x8B85E3u /* Streams metatile rows to PPU tilemap during camera panning */
#define ISSD_ADDR_PITCH_STREAM_COL              0x8B86E9u /* Streams metatile columns to PPU tilemap */
#define ISSD_ADDR_CONVERT_WORLD_TO_METATILE     0x8B87E7u /* Converts world-space pixel coords to metatile map byte */
#define ISSD_ADDR_SELECT_PPU_QUADRANT           0x8B8838u /* Selects wrapped 512x512 PPU nametable quadrant */

/* Palette Engine & Direct Page Trampoline */
#define ISSD_ADDR_COPY_PALETTE_TO_MIRROR        0x80A976u /* Copies single palette chunk from ROM Bank $89 to $7E:2C00 */
#define ISSD_ADDR_COPY_PALETTE_MULTI_TO_MIRROR  0x80A9CAu /* Copies multiple palette chunks to $7E:2C00 */
#define ISSD_ADDR_DP_PALETTE_TRAMPOLINE         0x000008u /* Dynamic Direct Page MVN trampoline for palette animation */

/* Audio & SPC-700 Handshake */
#define ISSD_ADDR_PLAY_BGM_TRACK                0x80BF05u /* Queues and plays background music track via APU port */
#define ISSD_ADDR_STREAM_VOICE_SAMPLE           0x80BF76u /* Initiates streamed BRR announcer voice sample upload */
#define ISSD_ADDR_PLAY_SFX                      0x808DE8u /* Queues sound effect command */

/* Menu & Scene Transition Dispatchers */
#define ISSD_ADDR_TITLE_SCREEN_INIT             0x8BD295u /* Sets up golden soccer ball and title screen OAM/palettes */
#define ISSD_ADDR_MAIN_MENU_INIT                0x80AF94u /* Dispatches 8-button main menu initialization */
#define ISSD_ADDR_MAIN_MENU_LOAD_ICONS          0xA4A946u /* Loads menu icons and button graphics */
#define ISSD_ADDR_SCENARIO_MENU_INIT            0x80B062u /* Initializes scenario mode selection screen */


/* ----------------------------------------------------------------------------
 * 2. WRAM MEMORY MAP & VARIABLE OFFSETS
 * ----------------------------------------------------------------------------
 * Confirmed memory addresses in Low WRAM ($0000..$1FFF) and High WRAM ($7E:xxxx, $7F:xxxx).
 * ---------------------------------------------------------------------------- */

/* Direct Page ($00..$FF) */
#define RAM_ISSD_DP_TRAMPOLINE_ENTRY            0x0008u /* Start of Direct Page palette MVN trampoline */
#define RAM_ISSD_JOYPAD1_LO                     0x0010u /* Controller 1 raw input low byte */
#define RAM_ISSD_JOYPAD2_LO                     0x0012u /* Controller 2 raw input low byte */
#define RAM_ISSD_JOYPAD4_LO                     0x0014u /* Controller 4 raw input low byte (multitap) */
#define RAM_ISSD_HELD_BUTTONS_P1                0x0020u /* Controller 1 held buttons (16-bit) */
#define RAM_ISSD_HELD_BUTTONS_P2                0x0022u /* Controller 2 held buttons (16-bit) */
#define RAM_ISSD_PRESSED_BUTTONS_P1             0x0028u /* Controller 1 newly pressed buttons (16-bit) */
#define RAM_ISSD_PRESSED_BUTTONS_P2             0x002Au /* Controller 2 newly pressed buttons (16-bit) */
#define RAM_ISSD_CURRENT_GAME_MODE1             0x0032u /* Primary game mode state machine */
#define RAM_ISSD_NMI_BUSY_FLAG                  0x0034u /* NMI busy / execution lock */
#define RAM_ISSD_NMI_FRAME_COUNTER              0x003Cu /* Vertical blank frame counter */
#define RAM_ISSD_INIDISP_SHADOW                 0x003Eu /* Screen display / brightness shadow register */
#define RAM_ISSD_MOSAIC_SHADOW                  0x0040u /* Mosaic effect shadow register */
#define RAM_ISSD_CURRENT_GAME_MODE2             0x0070u /* Secondary sub-mode state machine */

/* Low WRAM ($0100..$1FFF) */
#define RAM_ISSD_OAM_BUFFER                     0x01E0u /* OAM sprite buffer mirror (512 bytes) */
#define RAM_ISSD_OAM_HI_BUFFER                  0x03E0u /* High OAM bit buffer (32 bytes) */
#define RAM_ISSD_P1_TEAM_ID                     0x0DA0u /* Player 1 selected Team ID */
#define RAM_ISSD_P1_SCORE                       0x0DA2u /* Player 1 match score */
#define RAM_ISSD_P1_GOALIE_SKILL                0x0DE2u /* Player 1 goalkeeper skill rating */
#define RAM_ISSD_P2_TEAM_ID                     0x0EA0u /* Player 2 selected Team ID */
#define RAM_ISSD_P2_SCORE                       0x0EA2u /* Player 2 match score */
#define RAM_ISSD_P2_GOALIE_SKILL                0x0EE2u /* Player 2 goalkeeper skill rating */
#define RAM_ISSD_NUM_PLAYERS_SETTING            0x11E6u /* Number of active human players */
#define RAM_ISSD_LAYER1_X_SCROLL                0x13A0u /* Background Layer 1 X scroll (pitch) */
#define RAM_ISSD_LAYER1_Y_SCROLL                0x13B0u /* Background Layer 1 Y scroll */
#define RAM_ISSD_LAYER2_X_SCROLL                0x13C0u /* Background Layer 2 X scroll (stadium) */
#define RAM_ISSD_LAYER2_Y_SCROLL                0x13D0u /* Background Layer 2 Y scroll */
#define RAM_ISSD_LAYER3_X_SCROLL                0x13E0u /* Background Layer 3 X scroll (HUD) */
#define RAM_ISSD_LAYER3_Y_SCROLL                0x13F0u /* Background Layer 3 Y scroll */
#define RAM_ISSD_MATCH_TIMER_SECONDS            0x16D0u /* Match elapsed time in seconds */
#define RAM_ISSD_MATCH_TIMER_MINUTES            0x16D1u /* Match elapsed time in minutes */
#define RAM_ISSD_DISPLAYED_TIMER_SECONDS        0x16D2u /* Clock seconds rendered on HUD */
#define RAM_ISSD_DISPLAYED_TIMER_MINUTES        0x16D3u /* Clock minutes rendered on HUD */
#define RAM_ISSD_CURRENT_SPLASH_SCREEN          0x19A6u /* ID of current full-screen splash banner */
#define RAM_ISSD_WRAM_MVN_TRAMPOLINE            0x1E40u /* Location of dynamic MVN decompression routine */
#define RAM_ISSD_WEATHER_SETTING                0x1E4Cu /* Active weather (0=Snow, 1=Fine, 2=Rain) */
#define RAM_ISSD_GAME_LEVEL_SETTING             0x1E54u /* AI Difficulty level (0=Easy, 1=Medium, 2=Hard) */
#define RAM_ISSD_REFEREE_SETTING                0x1E58u /* Match referee strictness profile */
#define RAM_ISSD_TIME_OF_DAY_SETTING            0x1E5Cu /* Time of day (0=Day, 1=Dusk, 2=Night) */
#define RAM_ISSD_STADIUM_SETTING                0x1FA2u /* Selected Stadium ID (0..7) */

/* High WRAM ($7E:2000..$7F:FFFF) */
#define RAM_ISSD_PALETTE_MIRROR                 0x7E2C00u /* CGRAM palette mirror (512 bytes = 256 colors) */
#define RAM_ISSD_DMA_VRAM_UPLOAD_TABLE          0x7E3200u /* Staged VRAM upload queue processed during V-Blank */
#define RAM_ISSD_METATILE_DEFS_BG1              0x7F8000u /* 32x32 Metatile definitions for BG1 pitch */
#define RAM_ISSD_METATILE_DEFS_BG2              0x7FA000u /* 32x32 Metatile definitions for BG2 stadium */
#define RAM_ISSD_WORLD_METATILE_MAP_BG1         0x7FD000u /* World-space metatile byte ID grid for BG1 */
#define RAM_ISSD_WORLD_METATILE_MAP_BG2         0x7FE000u /* World-space metatile byte ID grid for BG2 */
#define RAM_ISSD_STADIUM_STRIDE                 0x7FFFCCu /* Stadium horizontal stride width */


/* ----------------------------------------------------------------------------
 * 3. GAME STATE MACHINE ENUMS
 * ----------------------------------------------------------------------------
 * Mode 1 (High-Level Scene) and Mode 2 (Sub-State) values.
 * ---------------------------------------------------------------------------- */

typedef enum {
    ISSD_SYSMODE1_BOOT_INIT        = 0x00, /* Konami screen, copyright, memory reset */
    ISSD_SYSMODE1_TITLE_SEQUENCE   = 0x01, /* Title screen, spinning globe, attract intro */
    ISSD_SYSMODE1_DEMO_MATCH       = 0x03, /* CPU attract mode exhibition game */
    ISSD_SYSMODE1_MENU_AND_GAME    = 0x06, /* Main menu, team setup, and live match */
} IssdSystemMode1;

typedef enum {
    ISSD_SYSMODE2_IDLE             = 0x00, /* Idle / transition state */
    ISSD_SYSMODE2_MENU_NAVIGATION  = 0x02, /* Active cursor navigation in menus */
    ISSD_SYSMODE2_MENU_INIT        = 0x04, /* Initializing menu screen assets */
    ISSD_SYSMODE2_MATCH_SIMULATION = 0x08, /* Live match simulation & player physics */
    ISSD_SYSMODE2_PAUSE_MENU       = 0x0A, /* In-game pause overlay */
    ISSD_SYSMODE2_MATCH_INIT       = 0x0C, /* Loading match assets, pitch, and rosters */
} IssdSystemMode2;


/* ----------------------------------------------------------------------------
 * 4. TEAMS, STADIUMS, AND ENVIRONMENTAL ENUMS
 * ---------------------------------------------------------------------------- */

typedef enum {
    ISSD_TEAM_ITALY             = 0x0000,
    ISSD_TEAM_HOLLAND           = 0x0002,
    ISSD_TEAM_ENGLAND           = 0x0004,
    ISSD_TEAM_NORWAY            = 0x0006,
    ISSD_TEAM_SPAIN             = 0x0008,
    ISSD_TEAM_IRELAND           = 0x000A,
    ISSD_TEAM_PORTUGAL          = 0x000C,
    ISSD_TEAM_DENMARK           = 0x000E,
    ISSD_TEAM_GERMANY           = 0x0010,
    ISSD_TEAM_FRANCE            = 0x0012,
    ISSD_TEAM_BELGIUM           = 0x0014,
    ISSD_TEAM_SWEDEN            = 0x0016,
    ISSD_TEAM_ROMANIA           = 0x0018,
    ISSD_TEAM_BULGARIA          = 0x001A,
    ISSD_TEAM_RUSSIA            = 0x001C,
    ISSD_TEAM_SWITZERLAND       = 0x001E,
    ISSD_TEAM_GREECE            = 0x0020,
    ISSD_TEAM_CROATIA           = 0x0022,
    ISSD_TEAM_AUSTRIA           = 0x0024,
    ISSD_TEAM_WALES             = 0x0026,
    ISSD_TEAM_SCOTLAND          = 0x0028,
    ISSD_TEAM_NORTH_IRELAND     = 0x002A,
    ISSD_TEAM_CZECH_REPUBLIC    = 0x002C,
    ISSD_TEAM_POLAND            = 0x002E,
    ISSD_TEAM_JAPAN             = 0x0030,
    ISSD_TEAM_SOUTH_KOREA       = 0x0032,
    ISSD_TEAM_TURKEY            = 0x0034,
    ISSD_TEAM_NIGERIA           = 0x0036,
    ISSD_TEAM_CAMEROON          = 0x0038,
    ISSD_TEAM_MOROCCO           = 0x003A,
    ISSD_TEAM_BRAZIL            = 0x003C,
    ISSD_TEAM_ARGENTINA         = 0x003E,
    ISSD_TEAM_COLOMBIA          = 0x0040,
    ISSD_TEAM_MEXICO            = 0x0042,
    ISSD_TEAM_USA               = 0x0044,
    ISSD_TEAM_URUGUAY           = 0x0046,
    ISSD_TEAM_ALL_STAR          = 0x0048,
    ISSD_TEAM_EURO_STAR_A       = 0x004A,
    ISSD_TEAM_EURO_STAR_B       = 0x004C,
    ISSD_TEAM_ASIAN_STAR        = 0x004E,
    ISSD_TEAM_AFRICAN_STAR      = 0x0050,
    ISSD_TEAM_ALL_AMERICAN_STAR = 0x0052,
    ISSD_TEAM_CHALLENGE_MODE    = 0x0054
} IssdTeamId;

typedef enum {
    ISSD_STADIUM_JAPAN          = 0x0000,
    ISSD_STADIUM_USA            = 0x0001,
    ISSD_STADIUM_SPAIN          = 0x0002,
    ISSD_STADIUM_ITALY          = 0x0003,
    ISSD_STADIUM_ENGLAND        = 0x0004,
    ISSD_STADIUM_GERMANY        = 0x0005,
    ISSD_STADIUM_BRAZIL         = 0x0006,
    ISSD_STADIUM_NIGERIA        = 0x0007
} IssdStadiumId;

typedef enum {
    ISSD_WEATHER_SNOW           = 0x0000,
    ISSD_WEATHER_FINE           = 0x0001,
    ISSD_WEATHER_RAIN           = 0x0002
} IssdWeather;

typedef enum {
    ISSD_TIME_DAY               = 0x0000,
    ISSD_TIME_DUSK              = 0x0001,
    ISSD_TIME_NIGHT             = 0x0002
} IssdTimeOfDay;


/* ----------------------------------------------------------------------------
 * 5. AUDIO & VOICE SAMPLES
 * ---------------------------------------------------------------------------- */

typedef enum {
    ISSD_BGM_KONAMI_SCREEN      = 0x0000,
    ISSD_BGM_STAFF_ROLL         = 0x0004,
    ISSD_BGM_SCENARIO_CLEAR     = 0x000C,
    ISSD_BGM_INTRO_CUTSCENE     = 0x0010,
    ISSD_BGM_MAIN_MENU          = 0x0014,
    ISSD_BGM_INTERNATIONAL_CUP  = 0x0018,
    ISSD_BGM_WORLD_SERIES       = 0x001C,
    ISSD_BGM_SCENARIO_MENU      = 0x0020,
    ISSD_BGM_TRAINING_MENU      = 0x0024,
    ISSD_BGM_PRACTICE_MATCH     = 0x0028,
    ISSD_BGM_GAME_OVER          = 0x0038,
    ISSD_BGM_SHORT_LEAGUE       = 0x003C
} IssdBgmTrack;

typedef enum {
    ISSD_VOICE_NONE             = 0x0000,
    ISSD_VOICE_CORNER_KICK      = 0x0001,
    ISSD_VOICE_GOAL_KICK        = 0x0002,
    ISSD_VOICE_THROW_IN         = 0x0003,
    ISSD_VOICE_FREE_KICK        = 0x0004,
    ISSD_VOICE_PENALTY_KICK     = 0x0005,
    ISSD_VOICE_OFFSIDE          = 0x0006,
    ISSD_VOICE_REPLAY           = 0x0007,
    ISSD_VOICE_HALF_TIME        = 0x0008,
    ISSD_VOICE_INJURY_TIME      = 0x0009,
    ISSD_VOICE_GREAT_SAVE       = 0x000A,
    ISSD_VOICE_ON_THE_VOLLEY    = 0x0010,
    ISSD_VOICE_YOU_LOSE         = 0x0013,
    ISSD_VOICE_YOU_WIN          = 0x0014,
    ISSD_VOICE_MATCH_DRAW       = 0x0015,
    ISSD_VOICE_PLAYER_CHANGE    = 0x0016,
    ISSD_VOICE_KICKOFF          = 0x0027,
    ISSD_VOICE_FOUL             = 0x0028,
    ISSD_VOICE_YELLOW_CARD      = 0x0029,
    ISSD_VOICE_RED_CARD         = 0x002A,
    ISSD_VOICE_HES_OFF          = 0x002B,
    ISSD_VOICE_HE_SHOOTS        = 0x002C,
    ISSD_VOICE_GOAL             = 0x002F,
    ISSD_VOICE_OWN_GOAL         = 0x0032,
    ISSD_VOICE_TIME_UP          = 0x0043,
    ISSD_VOICE_GOOOOOAL         = 0x0046,
    ISSD_VOICE_DOG_BARK         = 0x0048
} IssdAnnouncerVoice;

typedef enum {
    ISSD_STREAMED_NONE          = 0x0000,
    ISSD_STREAMED_TITLE_DROP    = 0x000Cu /* "International Superstar Soccer Deluxe!" voice drop */
} IssdStreamedSample;

#ifdef __cplusplus
}
#endif

#endif /* ISSD_SYMBOLS_H */
