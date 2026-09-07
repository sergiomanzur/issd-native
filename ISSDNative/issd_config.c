#include "issd_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

IssdConfig g_issd_config;

void issd_config_init_defaults(IssdConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    /* Video */
    cfg->window_width = 1280;   /* Modern 720p/1080p default */
    cfg->window_height = 720;
    cfg->fullscreen = false;
    cfg->vsync = true;
    cfg->target_fps = 60;
    cfg->integer_scaling = false;
    cfg->scanlines = false;
    cfg->aspect_ratio = ISSD_ASPECT_4_3;
    cfg->internal_res = ISSD_RES_4X;
    cfg->scaling_filter = ISSD_FILTER_LINEAR;
    cfg->true_widescreen = true;

    /* Audio */
    cfg->audio_freq = 48000;
    cfg->master_volume = 100;
    cfg->music_volume = 100;
    cfg->sfx_volume = 100;

    /* Gameplay */
    cfg->engine_mode = ISSD_MODE_ENHANCED;
    cfg->skip_intro = false;
    cfg->fast_menus = false;

    /* Controls (SDL Scancodes) */
    cfg->key_p1_up = 26;     /* SDL_SCANCODE_W */
    cfg->key_p1_down = 22;   /* SDL_SCANCODE_S */
    cfg->key_p1_left = 4;    /* SDL_SCANCODE_A */
    cfg->key_p1_right = 7;   /* SDL_SCANCODE_D */
    cfg->key_p1_a = 27;      /* SDL_SCANCODE_X / K -> Shoot */
    cfg->key_p1_b = 29;      /* SDL_SCANCODE_Z / J -> Pass */
    cfg->key_p1_x = 25;      /* SDL_SCANCODE_V / I -> Through */
    cfg->key_p1_y = 6;       /* SDL_SCANCODE_C / U -> Dash */
    cfg->key_p1_l = 20;      /* SDL_SCANCODE_Q */
    cfg->key_p1_r = 8;       /* SDL_SCANCODE_E */
    cfg->key_p1_start = 40;  /* SDL_SCANCODE_RETURN */
    cfg->key_p1_select = 44; /* SDL_SCANCODE_SPACE */
}

bool issd_config_load(IssdConfig *cfg, const char *filepath) {
    if (!cfg) return false;
    issd_config_init_defaults(cfg);

    const char *path = (filepath && filepath[0]) ? filepath : "issd_config.json";
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("[Config] No configuration file found at '%s', using defaults.\n", path);
        return false;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        int ival = 0;
        if (sscanf(line, " \"%63[^\"]\" : %d", key, &ival) == 2 ||
            sscanf(line, " \"%63[^\"]\": %d", key, &ival) == 2 ||
            sscanf(line, "\"%63[^\"]\":%d", key, &ival) == 2) {
            
            if (strcmp(key, "window_width") == 0) cfg->window_width = ival;
            else if (strcmp(key, "window_height") == 0) cfg->window_height = ival;
            else if (strcmp(key, "fullscreen") == 0) cfg->fullscreen = (ival != 0);
            else if (strcmp(key, "vsync") == 0) cfg->vsync = (ival != 0);
            else if (strcmp(key, "target_fps") == 0) cfg->target_fps = ival;
            else if (strcmp(key, "integer_scaling") == 0) cfg->integer_scaling = (ival != 0);
            else if (strcmp(key, "scanlines") == 0) cfg->scanlines = (ival != 0);
            else if (strcmp(key, "aspect_ratio") == 0) cfg->aspect_ratio = (IssdAspectRatio)ival;
            else if (strcmp(key, "internal_res") == 0) cfg->internal_res = (IssdInternalResolution)ival;
            else if (strcmp(key, "scaling_filter") == 0) cfg->scaling_filter = (IssdScalingFilter)ival;
            else if (strcmp(key, "true_widescreen") == 0) cfg->true_widescreen = (ival != 0);
            else if (strcmp(key, "audio_freq") == 0) cfg->audio_freq = ival;
            else if (strcmp(key, "master_volume") == 0) cfg->master_volume = ival;
            else if (strcmp(key, "music_volume") == 0) cfg->music_volume = ival;
            else if (strcmp(key, "sfx_volume") == 0) cfg->sfx_volume = ival;
            else if (strcmp(key, "engine_mode") == 0) cfg->engine_mode = (IssdEngineMode)ival;
            else if (strcmp(key, "skip_intro") == 0) cfg->skip_intro = (ival != 0);
            else if (strcmp(key, "fast_menus") == 0) cfg->fast_menus = (ival != 0);
        }
    }

    fclose(f);
    printf("[Config] Loaded configuration from '%s'.\n", path);
    return true;
}

bool issd_config_save(const IssdConfig *cfg, const char *filepath) {
    if (!cfg) return false;
    const char *path = (filepath && filepath[0]) ? filepath : "issd_config.json";
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"window_width\": %d,\n", cfg->window_width);
    fprintf(f, "  \"window_height\": %d,\n", cfg->window_height);
    fprintf(f, "  \"fullscreen\": %d,\n", cfg->fullscreen ? 1 : 0);
    fprintf(f, "  \"vsync\": %d,\n", cfg->vsync ? 1 : 0);
    fprintf(f, "  \"target_fps\": %d,\n", cfg->target_fps);
    fprintf(f, "  \"integer_scaling\": %d,\n", cfg->integer_scaling ? 1 : 0);
    fprintf(f, "  \"scanlines\": %d,\n", cfg->scanlines ? 1 : 0);
    fprintf(f, "  \"aspect_ratio\": %d,\n", (int)cfg->aspect_ratio);
    fprintf(f, "  \"internal_res\": %d,\n", (int)cfg->internal_res);
    fprintf(f, "  \"scaling_filter\": %d,\n", (int)cfg->scaling_filter);
    fprintf(f, "  \"true_widescreen\": %d,\n", cfg->true_widescreen ? 1 : 0);
    fprintf(f, "  \"audio_freq\": %d,\n", cfg->audio_freq);
    fprintf(f, "  \"master_volume\": %d,\n", cfg->master_volume);
    fprintf(f, "  \"music_volume\": %d,\n", cfg->music_volume);
    fprintf(f, "  \"sfx_volume\": %d,\n", cfg->sfx_volume);
    fprintf(f, "  \"engine_mode\": %d,\n", (int)cfg->engine_mode);
    fprintf(f, "  \"skip_intro\": %d,\n", cfg->skip_intro ? 1 : 0);
    fprintf(f, "  \"fast_menus\": %d\n", cfg->fast_menus ? 1 : 0);
    fprintf(f, "}\n");

    fclose(f);
    printf("[Config] Saved configuration to '%s'.\n", path);
    return true;
}
