#include "issd_config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

IssdConfig g_issd_config;
static char s_default_config_path[1024] = "issd_native.cfg";

void issd_config_set_default_path(const char *filepath) {
    if (!filepath || !filepath[0]) return;
    snprintf(s_default_config_path, sizeof(s_default_config_path), "%s", filepath);
}

const char *issd_config_get_default_path(void) {
    return s_default_config_path;
}

void issd_config_init_defaults(IssdConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    cfg->window_width = 1280;
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

    cfg->audio_freq = 48000;
    cfg->master_volume = 100;
    cfg->music_volume = 100;
    cfg->sfx_volume = 100;

    cfg->engine_mode = ISSD_MODE_ENHANCED;
    cfg->skip_intro = false;
    cfg->fast_menus = false;

    cfg->key_p1_up = 26;
    cfg->key_p1_down = 22;
    cfg->key_p1_left = 4;
    cfg->key_p1_right = 7;
    cfg->key_p1_a = 27;
    cfg->key_p1_b = 29;
    cfg->key_p1_x = 25;
    cfg->key_p1_y = 6;
    cfg->key_p1_l = 20;
    cfg->key_p1_r = 8;
    cfg->key_p1_start = 40;
    cfg->key_p1_select = 44;
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static void copy_config_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", src);
    char *value = trim(temp);
    size_t len = strlen(value);
    while (len && (value[len - 1] == ',' || isspace((unsigned char)value[len - 1]))) {
        value[--len] = '\0';
    }
    bool quoted = len >= 2 && value[0] == '"' && value[len - 1] == '"';
    if (quoted) {
        value[len - 1] = '\0';
        value++;
    }

    size_t out = 0;
    for (size_t i = 0; value[i] && out + 1 < dst_size; i++) {
        if (quoted && value[i] == '\\' && value[i + 1]) {
            i++;
            if (value[i] == 'n') dst[out++] = '\n';
            else if (value[i] == 't') dst[out++] = '\t';
            else dst[out++] = value[i];
        } else {
            dst[out++] = value[i];
        }
    }
    dst[out] = '\0';
}

static bool parse_config_line(char *line, char *key, size_t key_size, char **value_out) {
    char *s = trim(line);
    if (!*s || *s == '#' || *s == ';' || *s == '{' || *s == '}') return false;

    char *sep = strchr(s, '=');
    if (!sep) sep = strchr(s, ':');
    if (!sep) return false;

    *sep = '\0';
    char *raw_key = trim(s);
    char *raw_value = trim(sep + 1);
    size_t key_len = strlen(raw_key);
    if (key_len >= 2 && raw_key[0] == '"' && raw_key[key_len - 1] == '"') {
        raw_key[key_len - 1] = '\0';
        raw_key++;
    }

    snprintf(key, key_size, "%s", raw_key);
    *value_out = raw_value;
    return key[0] != '\0';
}

static void apply_config_value(IssdConfig *cfg, const char *key, const char *value) {
    int ival = atoi(value);

    if (strcmp(key, "rom_path") == 0) copy_config_string(cfg->rom_path, sizeof(cfg->rom_path), value);
    else if (strcmp(key, "window_width") == 0) cfg->window_width = ival;
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
    else if (strcmp(key, "key_p1_up") == 0) cfg->key_p1_up = ival;
    else if (strcmp(key, "key_p1_down") == 0) cfg->key_p1_down = ival;
    else if (strcmp(key, "key_p1_left") == 0) cfg->key_p1_left = ival;
    else if (strcmp(key, "key_p1_right") == 0) cfg->key_p1_right = ival;
    else if (strcmp(key, "key_p1_a") == 0) cfg->key_p1_a = ival;
    else if (strcmp(key, "key_p1_b") == 0) cfg->key_p1_b = ival;
    else if (strcmp(key, "key_p1_x") == 0) cfg->key_p1_x = ival;
    else if (strcmp(key, "key_p1_y") == 0) cfg->key_p1_y = ival;
    else if (strcmp(key, "key_p1_l") == 0) cfg->key_p1_l = ival;
    else if (strcmp(key, "key_p1_r") == 0) cfg->key_p1_r = ival;
    else if (strcmp(key, "key_p1_start") == 0) cfg->key_p1_start = ival;
    else if (strcmp(key, "key_p1_select") == 0) cfg->key_p1_select = ival;
}

bool issd_config_load(IssdConfig *cfg, const char *filepath) {
    if (!cfg) return false;
    issd_config_init_defaults(cfg);

    const char *path = (filepath && filepath[0]) ? filepath : s_default_config_path;
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("[Config] No configuration file found at '%s', using defaults.\n", path);
        return false;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        char *value = NULL;
        if (parse_config_line(line, key, sizeof(key), &value)) {
            apply_config_value(cfg, key, value);
        }
    }

    fclose(f);
    printf("[Config] Loaded configuration from '%s'.\n", path);
    return true;
}

bool issd_config_save(const IssdConfig *cfg, const char *filepath) {
    if (!cfg) return false;
    const char *path = (filepath && filepath[0]) ? filepath : s_default_config_path;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "# ISSD Native configuration\n");
    fprintf(f, "# ROM files are not distributed with this project. Select or provide your own valid cartridge dump.\n");
    fprintf(f, "rom_path=%s\n", cfg->rom_path);
    fprintf(f, "window_width=%d\n", cfg->window_width);
    fprintf(f, "window_height=%d\n", cfg->window_height);
    fprintf(f, "fullscreen=%d\n", cfg->fullscreen ? 1 : 0);
    fprintf(f, "vsync=%d\n", cfg->vsync ? 1 : 0);
    fprintf(f, "target_fps=%d\n", cfg->target_fps);
    fprintf(f, "integer_scaling=%d\n", cfg->integer_scaling ? 1 : 0);
    fprintf(f, "scanlines=%d\n", cfg->scanlines ? 1 : 0);
    fprintf(f, "aspect_ratio=%d\n", (int)cfg->aspect_ratio);
    fprintf(f, "internal_res=%d\n", (int)cfg->internal_res);
    fprintf(f, "scaling_filter=%d\n", (int)cfg->scaling_filter);
    fprintf(f, "true_widescreen=%d\n", cfg->true_widescreen ? 1 : 0);
    fprintf(f, "audio_freq=%d\n", cfg->audio_freq);
    fprintf(f, "master_volume=%d\n", cfg->master_volume);
    fprintf(f, "music_volume=%d\n", cfg->music_volume);
    fprintf(f, "sfx_volume=%d\n", cfg->sfx_volume);
    fprintf(f, "engine_mode=%d\n", (int)cfg->engine_mode);
    fprintf(f, "skip_intro=%d\n", cfg->skip_intro ? 1 : 0);
    fprintf(f, "fast_menus=%d\n", cfg->fast_menus ? 1 : 0);
    fprintf(f, "key_p1_up=%d\n", cfg->key_p1_up);
    fprintf(f, "key_p1_down=%d\n", cfg->key_p1_down);
    fprintf(f, "key_p1_left=%d\n", cfg->key_p1_left);
    fprintf(f, "key_p1_right=%d\n", cfg->key_p1_right);
    fprintf(f, "key_p1_a=%d\n", cfg->key_p1_a);
    fprintf(f, "key_p1_b=%d\n", cfg->key_p1_b);
    fprintf(f, "key_p1_x=%d\n", cfg->key_p1_x);
    fprintf(f, "key_p1_y=%d\n", cfg->key_p1_y);
    fprintf(f, "key_p1_l=%d\n", cfg->key_p1_l);
    fprintf(f, "key_p1_r=%d\n", cfg->key_p1_r);
    fprintf(f, "key_p1_start=%d\n", cfg->key_p1_start);
    fprintf(f, "key_p1_select=%d\n", cfg->key_p1_select);

    fclose(f);
    printf("[Config] Saved configuration to '%s'.\n", path);
    return true;
}
