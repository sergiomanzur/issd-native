#ifndef ISSD_CONFIG_H
#define ISSD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define ISSD_CONFIG_ROM_PATH_MAX 1024
#define ISSD_CONFIG_MODS_DIR_MAX 1024
#define ISSD_PROFILE_PLAYERS 4
#define ISSD_PROFILE_BINDINGS 12
#define ISSD_TOUCH_CONTROLS 11
#define ISSD_PROFILE_CUSTOM 3
#define ISSD_INPUT_SOURCE_MASK ((UINT64_C(0x1fffff) & ~(UINT64_C(1) << 5)) | (UINT64_C(0x3f) << 32))

enum { ISSD_VISUAL_ORIGINAL, ISSD_VISUAL_SHARP, ISSD_VISUAL_ENHANCED, ISSD_VISUAL_CUSTOM };
typedef enum { ISSD_CAMERA_CLASSIC, ISSD_CAMERA_TACTICAL,
               ISSD_CAMERA_TACTICAL_WIDE } IssdCameraMode;

enum { ISSD_MATCH_ORIGINAL, ISSD_MATCH_CLASSIC, ISSD_MATCH_CASUAL, ISSD_MATCH_CUSTOM };
typedef struct {
    int duration; /* 0/1/2: 3/5/7 minutes; hidden duration 3 is unsupported */
    int difficulty; /* five original levels, 0..4 */
    int offside, fouls, cards, extra_time; /* cartridge encoding: 0 on, 1 off */
} IssdMatchRules;

typedef struct {
    int schema; /* 0 CLASSIC, 1 FIFA, 2 PES, 3 CUSTOM */
    int stick_deadzone;   /* 0..30000, signed SDL stick axis threshold */
    int trigger_deadzone; /* 0..30000, unsigned SDL trigger axis threshold */
    uint64_t bindings[ISSD_PROFILE_BINDINGS]; /* indexed by native SNES bit */
} IssdPlayerProfile;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ISSD_ASPECT_4_3 = 0,    /* Authentic CRT 4:3 */
    ISSD_ASPECT_8_7 = 1,    /* 1:1 Pixel Aspect Ratio */
    ISSD_ASPECT_16_9 = 2,   /* 16:9 Widescreen Viewport */
    ISSD_ASPECT_16_10 = 3,  /* 16:10 PC Widescreen Viewport */
    ISSD_ASPECT_21_9 = 4,   /* 21:9 Ultrawide Cinematic */
    ISSD_ASPECT_INTEGER = 5, /* Perfect Integer Scale */
    /* 320x224. The cartridge culls objects outside x,y in [-32,288), so a
     * 32 pixel margin per side is the widest view in which every visible
     * player is still simulated, animated and in the game's own draw list.
     * Wider presets can continue learned locomotion but unlearned actions may hold.
     * Appended rather than renumbered so saved configs keep their meaning. */
    ISSD_ASPECT_AUTHENTIC = 6
} IssdAspectRatio;
#define ISSD_ASPECT_COUNT 7

typedef enum {
    ISSD_RES_1X = 0,        /* Native 256 x 224 */
    ISSD_RES_2X = 1,        /* 512 x 448 */
    ISSD_RES_3X = 2,        /* 768 x 672 */
    ISSD_RES_4X = 3,        /* 1024 x 896 */
    ISSD_RES_6X = 4,        /* 1536 x 1344 */
    ISSD_RES_8X_4K = 5      /* 2048 x 1792; historical API name, not output 4K */
} IssdInternalResolution;

typedef enum {
    ISSD_FILTER_NEAREST = 0, /* Sharp Pixels (Nearest Neighbor) */
    ISSD_FILTER_LINEAR = 1,  /* Smooth (Bilinear) */
    ISSD_FILTER_CRT = 2,     /* CRT Scanlines Filter */
    ISSD_FILTER_SHARP = 3    /* Integer nearest prescale, then linear presentation */
} IssdScalingFilter;

typedef enum {
    ISSD_MODE_CLASSIC = 0,  /* Cycle-accurate SNES timing */
    ISSD_MODE_ENHANCED = 1  /* Stable 60Hz, slowdown eliminated */
} IssdEngineMode;

typedef struct {
    /* Video & Presentation */
    int window_width;
    int window_height;
    bool fullscreen;
    bool vsync;
    int target_fps;         /* 60, 120, 144, 165, 240, 0 (uncapped) */
    bool integer_scaling;
    bool scanlines;
    IssdAspectRatio aspect_ratio;
    IssdInternalResolution internal_res;
    IssdScalingFilter scaling_filter;
    bool true_widescreen;
    IssdCameraMode camera_mode;
    int output_resolution; /* 0 current/auto, 1 720p, 2 1080p, 3 1440p, 4 2160p */
    int overlay_scale;     /* 0 automatic, 1..4 output-pixel UI scale */
    bool ball_outline; /* Deprecated: always disabled; retained for source compatibility. */
    bool color_boost; /* Modest saturation on the game framebuffer only. */
    int crt_strength; /* 0/25/50/75/100 percent; 100 preserves legacy CRT. */
    bool ball_shadow;
    bool player_markers;
    bool player_names;
    bool enhanced_running_animation; /* Presentation-only intermediate running poses. */
    int radar_scale;       /* 1..3 native radar enlargement */
    int hud_scale;         /* 1..3 selected-player label size */
    int radar_position;    /* bottom center/left/right, top left/right */
    int radar_opacity;     /* 25..100 percent, enlarged radar background */

    /* Audio */
    int audio_freq;
    int master_volume; /* 0 - 100 */
    int music_volume;  /* 0 - 100 */
    int sfx_volume;    /* 0 - 100 */

    /* Gameplay & Engine */
    IssdEngineMode engine_mode;
    bool skip_intro;
    bool fast_menus;
    bool debug_unhooked_code;
    bool gameplay_goalkeeper_ai;
    bool gameplay_player_ai;
    bool gameplay_bug_fixes; /* Master switch for verified original-game fixes. */
    int match_preset; /* Original leaves the cartridge's settings untouched */
    IssdMatchRules match_custom;

    /* Name of the active mod pack, empty for vanilla. Stored by name rather
     * than index so adding or removing a pack cannot silently select a
     * different one. */
    /* Mods stack, so both of these are '|' separated lists in the order
     * they are applied: the last one wins where two of them collide.
     * Empty is vanilla. The old single-value keys are still read, so a
     * config written by an earlier build keeps working. */
    char active_mod_packs[512];    /* roster and formation packs, by name */
    char hd_texture_packs[512];    /* tile pack directories under mods/ */

    /* Controls (P1 Scancodes / Buttons) */
    int key_p1_up;
    int key_p1_down;
    int key_p1_left;
    int key_p1_right;
    int key_p1_a;      /* Shoot / Confirm */
    int key_p1_b;      /* Pass / Cancel */
    int key_p1_x;      /* Lob / Through */
    int key_p1_y;      /* Dash / Long pass */
    int key_p1_l;      /* Tactics */
    int key_p1_r;      /* Strategy */
    int key_p1_start;  /* Pause */
    int key_p1_select; /* Select */
    IssdPlayerProfile player_profiles[ISSD_PROFILE_PLAYERS];
    int touch_x[ISSD_TOUCH_CONTROLS]; /* normalized center 0..1000; -1 = automatic */
    int touch_y[ISSD_TOUCH_CONTROLS];
    int touch_size[ISSD_TOUCH_CONTROLS]; /* percentage 50..200 */

    /* ROM */
    char rom_path[ISSD_CONFIG_ROM_PATH_MAX];

    /* Mods */
    char mods_dir[ISSD_CONFIG_MODS_DIR_MAX];
} IssdConfig;

extern IssdConfig g_issd_config;

void issd_config_init_defaults(IssdConfig *cfg);
/* Presentation-only presets; preserve display, engine, controls and mods. */
void issd_config_visual_preset(IssdConfig *cfg, int preset);
int issd_config_visual_preset_id(const IssdConfig *cfg);
/* Replace bindings with a named preset, retaining this player's thresholds. */
void issd_config_player_preset(IssdConfig *cfg, int player, int schema);

/* True when Steam launched us on a Steam Deck. Seeds first-run defaults
 * only; a config file on disk always wins. */
bool issd_config_is_steam_deck(void);
bool issd_config_load(IssdConfig *cfg, const char *filepath);
bool issd_config_save(const IssdConfig *cfg, const char *filepath);
void issd_config_set_default_path(const char *filepath);
const char *issd_config_get_default_path(void);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_CONFIG_H */
