#ifndef ISSD_CONFIG_H
#define ISSD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define ISSD_CONFIG_ROM_PATH_MAX 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ISSD_ASPECT_4_3 = 0,    /* Authentic CRT 4:3 */
    ISSD_ASPECT_8_7 = 1,    /* 1:1 Pixel Aspect Ratio */
    ISSD_ASPECT_16_9 = 2,   /* 16:9 Widescreen Viewport */
    ISSD_ASPECT_16_10 = 3,  /* 16:10 PC Widescreen Viewport */
    ISSD_ASPECT_21_9 = 4,   /* 21:9 Ultrawide Cinematic */
    ISSD_ASPECT_INTEGER = 5 /* Perfect Integer Scale */
} IssdAspectRatio;

typedef enum {
    ISSD_RES_1X = 0,        /* Native 256 x 224 */
    ISSD_RES_2X = 1,        /* 512 x 448 (SD) */
    ISSD_RES_3X = 2,        /* 768 x 672 (720p HD) */
    ISSD_RES_4X = 3,        /* 1024 x 896 (1080p FHD) */
    ISSD_RES_6X = 4,        /* 1536 x 1344 (1440p QHD) */
    ISSD_RES_8X_4K = 5      /* 2048 x 1792 (4K UHD) */
} IssdInternalResolution;

typedef enum {
    ISSD_FILTER_NEAREST = 0, /* Sharp Pixels (Nearest Neighbor) */
    ISSD_FILTER_LINEAR = 1,  /* Smooth (Bilinear) */
    ISSD_FILTER_CRT = 2      /* CRT Scanlines Filter */
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

    /* Audio */
    int audio_freq;
    int master_volume; /* 0 - 100 */
    int music_volume;  /* 0 - 100 */
    int sfx_volume;    /* 0 - 100 */

    /* Gameplay & Engine */
    IssdEngineMode engine_mode;
    bool skip_intro;
    bool fast_menus;

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

    /* ROM */
    char rom_path[ISSD_CONFIG_ROM_PATH_MAX];
} IssdConfig;

extern IssdConfig g_issd_config;

void issd_config_init_defaults(IssdConfig *cfg);
bool issd_config_load(IssdConfig *cfg, const char *filepath);
bool issd_config_save(const IssdConfig *cfg, const char *filepath);
void issd_config_set_default_path(const char *filepath);
const char *issd_config_get_default_path(void);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_CONFIG_H */
