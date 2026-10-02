#include "issd_config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>

IssdConfig g_issd_config;
static char s_default_config_path[1024] = "issd_native.cfg";

void issd_config_player_preset(IssdConfig *cfg, int player, int schema) {
    if (!cfg || player < 0 || player >= ISSD_PROFILE_PLAYERS || schema < 0 || schema > 2) return;
    IssdPlayerProfile *p = &cfg->player_profiles[player];
    p->schema = schema;
    memset(p->bindings, 0, sizeof(p->bindings));
#define B(source, bit) (p->bindings[bit] |= UINT64_C(1) << (source))
    B(0, 0); B(3, 9); B(4, 2); B(6, 3);
    B(11, 4); B(32, 4); B(12, 5); B(33, 5);
    B(13, 6); B(34, 6); B(14, 7); B(35, 7);
    B(9, 10); B(36, 11); B(37, 1);
    B(1, schema == 2 ? 1 : 8); B(2, schema == 2 ? 8 : 1);
    B(10, schema == 0 ? 11 : 1);
#undef B
}

void issd_config_set_default_path(const char *filepath) {
    if (!filepath || !filepath[0]) return;
    snprintf(s_default_config_path, sizeof(s_default_config_path), "%s", filepath);
}

const char *issd_config_get_default_path(void) {
    return s_default_config_path;
}

void issd_config_visual_preset(IssdConfig *cfg, int preset) {
    if (!cfg || preset < ISSD_VISUAL_ORIGINAL || preset > ISSD_VISUAL_ENHANCED) return;
    bool enhanced = preset == ISSD_VISUAL_ENHANCED;
    cfg->aspect_ratio = enhanced ? ISSD_ASPECT_16_9 : ISSD_ASPECT_4_3;
    cfg->true_widescreen = true;
    cfg->scaling_filter = preset == ISSD_VISUAL_ORIGINAL ? ISSD_FILTER_NEAREST : ISSD_FILTER_SHARP;
    cfg->internal_res = ISSD_RES_1X;
    cfg->integer_scaling = false;
    cfg->scanlines = false;
    cfg->ball_outline = cfg->ball_shadow = cfg->player_markers = cfg->player_names = enhanced;
    cfg->radar_scale = enhanced ? 2 : 1;
    cfg->hud_scale = 1;
    cfg->radar_position = enhanced ? 2 : 0;
    cfg->radar_opacity = 75;
}

int issd_config_visual_preset_id(const IssdConfig *cfg) {
    if (!cfg) return ISSD_VISUAL_CUSTOM;
    for (int p = ISSD_VISUAL_ORIGINAL; p <= ISSD_VISUAL_ENHANCED; ++p) {
        IssdConfig expected = *cfg;
        issd_config_visual_preset(&expected, p);
        if (cfg->aspect_ratio == expected.aspect_ratio &&
            cfg->true_widescreen == expected.true_widescreen &&
            cfg->scaling_filter == expected.scaling_filter &&
            cfg->internal_res == expected.internal_res &&
            cfg->integer_scaling == expected.integer_scaling &&
            cfg->scanlines == expected.scanlines &&
            cfg->ball_outline == expected.ball_outline && cfg->ball_shadow == expected.ball_shadow &&
            cfg->player_markers == expected.player_markers && cfg->player_names == expected.player_names &&
            cfg->radar_scale == expected.radar_scale && cfg->hud_scale == expected.hud_scale &&
            cfg->radar_position == expected.radar_position && cfg->radar_opacity == expected.radar_opacity)
            return p;
    }
    return ISSD_VISUAL_CUSTOM;
}

/* Steam Deck's panel is 1280x800, so the 16:10 preset is its exact native
 * aspect. Steam exports SteamDeck=1 to launched titles; SteamOS alone is
 * not enough because a desktop SteamOS install can be any resolution.
 * This only ever seeds first-run defaults - an existing config file is
 * loaded afterwards and wins. */
bool issd_config_is_steam_deck(void) {
#ifdef _WIN32
    return false;
#else
    const char *deck = getenv("SteamDeck");
    return deck && deck[0] == '1';
#endif
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
    cfg->radar_scale = 1;
    cfg->hud_scale = 1;
    cfg->radar_position = 0;
    cfg->radar_opacity = 75;

    cfg->audio_freq = 48000;
    cfg->master_volume = 100;
    cfg->music_volume = 100;
    cfg->sfx_volume = 100;

    cfg->engine_mode = ISSD_MODE_ENHANCED;
    cfg->skip_intro = false;
    cfg->fast_menus = false;
    cfg->debug_unhooked_code = false;
    cfg->gameplay_goalkeeper_ai = false;
    cfg->gameplay_player_ai = false;
    cfg->gameplay_bug_fixes = false;
    cfg->match_preset = ISSD_MATCH_ORIGINAL;
    cfg->match_custom = (IssdMatchRules){1, 2, 0, 0, 0, 1};
    cfg->active_mod_packs[0] = 0;
    cfg->hd_texture_packs[0] = 0;
    snprintf(cfg->mods_dir, sizeof(cfg->mods_dir), "%s", "mods");

    if (issd_config_is_steam_deck()) {
        cfg->aspect_ratio = ISSD_ASPECT_16_10;   /* 1280x800 native */
        cfg->true_widescreen = true;
        cfg->fullscreen = true;
        cfg->window_width = 1280;
        cfg->window_height = 800;
    }

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
    for (int i = 0; i < ISSD_PROFILE_PLAYERS; i++) {
        cfg->player_profiles[i].stick_deadzone = 12000;
        cfg->player_profiles[i].trigger_deadzone = 12000;
        issd_config_player_preset(cfg, i, 0);
    }
    for (int i = 0; i < ISSD_TOUCH_CONTROLS; i++) {
        cfg->touch_x[i] = cfg->touch_y[i] = -1;
        cfg->touch_size[i] = 100;
    }
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

static bool number_end(const char *end) {
    while (isspace((unsigned char)*end)) end++;
    if (*end == ',') end++;
    while (isspace((unsigned char)*end)) end++;
    return *end == '\0';
}

static bool parse_int(const char *value, int *out) {
    char *end;
    errno = 0;
    long long n = strtoll(value, &end, 10);
    if (end == value || !number_end(end)) return false;
    /* strtoll saturates on overflow; narrowing happens only after clamping. */
    *out = n < INT_MIN ? INT_MIN : n > INT_MAX ? INT_MAX : (int)n;
    return true;
}

static bool parse_mask(const char *value, uint64_t *out) {
    while (isspace((unsigned char)*value)) value++;
    if (*value == '-') return false;
    char *end;
    errno = 0;
    unsigned long long n = strtoull(value, &end, 10);
    if (end == value || errno == ERANGE || !number_end(end)) return false;
    *out = (uint64_t)n & ISSD_INPUT_SOURCE_MASK;
    return true;
}

static int clamp(int n, int low, int high) { return n < low ? low : n > high ? high : n; }

static void apply_config_value(IssdConfig *cfg, const char *key, const char *value,
                               uint16_t seen[ISSD_PROFILE_PLAYERS]) {
    int ival = 0;
    bool valid_int = parse_int(value, &ival);
    char field[64];
    const char *rule_keys[] = {"match_preset", "match_duration", "match_difficulty",
        "match_offside", "match_fouls", "match_cards", "match_extra_time"};
    int *rule_values[] = {&cfg->match_preset, &cfg->match_custom.duration,
        &cfg->match_custom.difficulty, &cfg->match_custom.offside,
        &cfg->match_custom.fouls, &cfg->match_custom.cards, &cfg->match_custom.extra_time};
    const int rule_max[] = {3, 2, 4, 1, 1, 1, 1};
    for (unsigned i = 0; i < sizeof(rule_max)/sizeof(rule_max[0]); ++i) {
        if (!strcmp(key, rule_keys[i])) {
            if (valid_int) *rule_values[i] = clamp(ival, 0, rule_max[i]);
            return;
        }
    }
    for (int p = 0; p < ISSD_PROFILE_PLAYERS; p++) {
        IssdPlayerProfile *profile = &cfg->player_profiles[p];
        snprintf(field, sizeof(field), "player_%d_schema", p + 1);
        if (!strcmp(key, field)) { if (valid_int) profile->schema = clamp(ival, 0, 3); return; }
        snprintf(field, sizeof(field), "player_%d_stick_deadzone", p + 1);
        if (!strcmp(key, field)) { if (valid_int) profile->stick_deadzone = clamp(ival, 0, 30000); return; }
        snprintf(field, sizeof(field), "player_%d_trigger_deadzone", p + 1);
        if (!strcmp(key, field)) { if (valid_int) profile->trigger_deadzone = clamp(ival, 0, 30000); return; }
        for (int b = 0; b < ISSD_PROFILE_BINDINGS; b++) {
            snprintf(field, sizeof(field), "player_%d_bind_%d", p + 1, b);
            if (!strcmp(key, field)) {
                if (parse_mask(value, &profile->bindings[b])) seen[p] |= 1u << b;
                return;
            }
        }
    }
    for (int i = 0; i < ISSD_TOUCH_CONTROLS; i++) {
        snprintf(field, sizeof(field), "touch_%d_x", i);
        if (!strcmp(key, field)) { if (valid_int) cfg->touch_x[i] = clamp(ival, -1, 1000); return; }
        snprintf(field, sizeof(field), "touch_%d_y", i);
        if (!strcmp(key, field)) { if (valid_int) cfg->touch_y[i] = clamp(ival, -1, 1000); return; }
        snprintf(field, sizeof(field), "touch_%d_size", i);
        if (!strcmp(key, field)) { if (valid_int) cfg->touch_size[i] = clamp(ival, 50, 200); return; }
    }
    /* Presentation values reject malformed numbers and clamp before narrowing.
     * Missing keys retain defaults, including legacy files with no UI settings. */
    if (!strcmp(key, "output_resolution")) { if (valid_int) cfg->output_resolution = clamp(ival, 0, 4); return; }
    if (!strcmp(key, "overlay_scale")) { if (valid_int) cfg->overlay_scale = clamp(ival, 0, 4); return; }
    if (!strcmp(key, "radar_scale")) { if (valid_int) cfg->radar_scale = clamp(ival, 1, 3); return; }
    if (!strcmp(key, "hud_scale")) { if (valid_int) cfg->hud_scale = clamp(ival, 1, 3); return; }
    if (!strcmp(key, "radar_position")) { if (valid_int) cfg->radar_position = clamp(ival, 0, 4); return; }
    if (!strcmp(key, "radar_opacity")) { if (valid_int) cfg->radar_opacity = clamp(ival, 25, 100); return; }
    if (!strcmp(key, "ball_outline")) { if (valid_int) cfg->ball_outline = ival != 0; return; }
    if (!strcmp(key, "ball_shadow")) { if (valid_int) cfg->ball_shadow = ival != 0; return; }
    if (!strcmp(key, "player_markers")) { if (valid_int) cfg->player_markers = ival != 0; return; }
    if (!strcmp(key, "player_names")) { if (valid_int) cfg->player_names = ival != 0; return; }
    if (!strcmp(key, "scaling_filter")) { if (valid_int) cfg->scaling_filter = (IssdScalingFilter)clamp(ival, 0, 3); return; }
    if (!strcmp(key, "internal_res")) { if (valid_int) cfg->internal_res = (IssdInternalResolution)clamp(ival, 0, 5); return; }
    if (!strcmp(key, "aspect_ratio")) { if (valid_int) cfg->aspect_ratio = (IssdAspectRatio)clamp(ival, 0, ISSD_ASPECT_COUNT - 1); return; }

    if (strcmp(key, "rom_path") == 0) copy_config_string(cfg->rom_path, sizeof(cfg->rom_path), value);
    else if (strcmp(key, "mods_dir") == 0) copy_config_string(cfg->mods_dir, sizeof(cfg->mods_dir), value);
    else if (strcmp(key, "window_width") == 0) cfg->window_width = ival;
    else if (strcmp(key, "window_height") == 0) cfg->window_height = ival;
    else if (strcmp(key, "fullscreen") == 0) cfg->fullscreen = (ival != 0);
    else if (strcmp(key, "vsync") == 0) cfg->vsync = (ival != 0);
    else if (strcmp(key, "target_fps") == 0) cfg->target_fps = ival;
    else if (strcmp(key, "integer_scaling") == 0) cfg->integer_scaling = (ival != 0);
    else if (strcmp(key, "scanlines") == 0) cfg->scanlines = (ival != 0);
    else if (strcmp(key, "true_widescreen") == 0) cfg->true_widescreen = (ival != 0);
    else if (strcmp(key, "audio_freq") == 0) cfg->audio_freq = ival;
    else if (strcmp(key, "master_volume") == 0) cfg->master_volume = ival;
    else if (strcmp(key, "music_volume") == 0) cfg->music_volume = ival;
    else if (strcmp(key, "sfx_volume") == 0) cfg->sfx_volume = ival;
    else if (strcmp(key, "engine_mode") == 0) cfg->engine_mode = (IssdEngineMode)ival;
    else if (strcmp(key, "skip_intro") == 0) cfg->skip_intro = (ival != 0);
    else if (strcmp(key, "fast_menus") == 0) cfg->fast_menus = (ival != 0);
    else if (strcmp(key, "debug_unhooked_code") == 0) cfg->debug_unhooked_code = (ival != 0);
    else if (strcmp(key, "gameplay_goalkeeper_ai") == 0) cfg->gameplay_goalkeeper_ai = (ival != 0);
    else if (strcmp(key, "gameplay_bug_fixes") == 0) cfg->gameplay_bug_fixes = (ival != 0);
    else if (strcmp(key, "gameplay_player_ai") == 0) cfg->gameplay_player_ai = (ival != 0);
    else if (strcmp(key, "active_mod_packs") == 0) { strncpy(cfg->active_mod_packs, value, sizeof(cfg->active_mod_packs)-1); cfg->active_mod_packs[sizeof(cfg->active_mod_packs)-1]=0; }
    else if (strcmp(key, "hd_texture_packs") == 0) { strncpy(cfg->hd_texture_packs, value, sizeof(cfg->hd_texture_packs)-1); cfg->hd_texture_packs[sizeof(cfg->hd_texture_packs)-1]=0; }
    /* Written by builds that only supported one pack of each kind. */
    else if (strcmp(key, "active_mod_pack") == 0) { strncpy(cfg->active_mod_packs, value, sizeof(cfg->active_mod_packs)-1); cfg->active_mod_packs[sizeof(cfg->active_mod_packs)-1]=0; }
    else if (strcmp(key, "hd_texture_pack") == 0) { strncpy(cfg->hd_texture_packs, value, sizeof(cfg->hd_texture_packs)-1); cfg->hd_texture_packs[sizeof(cfg->hd_texture_packs)-1]=0; }
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
    uint16_t seen[ISSD_PROFILE_PLAYERS] = {0};
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        char *value = NULL;
        if (parse_config_line(line, key, sizeof(key), &value)) {
            apply_config_value(cfg, key, value, seen);
        }
    }

    fclose(f);
    /* Schema-only and unordered files receive preset defaults for missing bindings. */
    for (int p = 0; p < ISSD_PROFILE_PLAYERS; p++) {
        IssdConfig preset;
        int schema = cfg->player_profiles[p].schema;
        issd_config_player_preset(&preset, p, schema == 3 ? 0 : schema);
        for (int b = 0; b < ISSD_PROFILE_BINDINGS; b++)
            if (!(seen[p] & (1u << b))) cfg->player_profiles[p].bindings[b] = preset.player_profiles[p].bindings[b];
    }
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
    fprintf(f, "mods_dir=%s\n", cfg->mods_dir);
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
    fprintf(f, "output_resolution=%d\n", clamp(cfg->output_resolution, 0, 4));
    fprintf(f, "overlay_scale=%d\n", clamp(cfg->overlay_scale, 0, 4));
    fprintf(f, "ball_outline=%d\n", cfg->ball_outline ? 1 : 0);
    fprintf(f, "ball_shadow=%d\n", cfg->ball_shadow ? 1 : 0);
    fprintf(f, "player_markers=%d\n", cfg->player_markers ? 1 : 0);
    fprintf(f, "player_names=%d\n", cfg->player_names ? 1 : 0);
    fprintf(f, "radar_scale=%d\n", clamp(cfg->radar_scale, 1, 3));
    fprintf(f, "hud_scale=%d\n", clamp(cfg->hud_scale, 1, 3));
    fprintf(f, "radar_position=%d\n", clamp(cfg->radar_position, 0, 4));
    fprintf(f, "radar_opacity=%d\n", clamp(cfg->radar_opacity, 25, 100));
    fprintf(f, "audio_freq=%d\n", cfg->audio_freq);
    fprintf(f, "master_volume=%d\n", cfg->master_volume);
    fprintf(f, "music_volume=%d\n", cfg->music_volume);
    fprintf(f, "sfx_volume=%d\n", cfg->sfx_volume);
    fprintf(f, "engine_mode=%d\n", (int)cfg->engine_mode);
    fprintf(f, "skip_intro=%d\n", cfg->skip_intro ? 1 : 0);
    fprintf(f, "fast_menus=%d\n", cfg->fast_menus ? 1 : 0);
    fprintf(f, "debug_unhooked_code=%d\n", cfg->debug_unhooked_code ? 1 : 0);
    fprintf(f, "gameplay_goalkeeper_ai=%d\n", cfg->gameplay_goalkeeper_ai ? 1 : 0);
    fprintf(f, "gameplay_player_ai=%d\n", cfg->gameplay_player_ai ? 1 : 0);
    fprintf(f, "gameplay_bug_fixes=%d\n", cfg->gameplay_bug_fixes ? 1 : 0);
    fprintf(f, "match_preset=%d\nmatch_duration=%d\nmatch_difficulty=%d\n",
            cfg->match_preset, cfg->match_custom.duration, cfg->match_custom.difficulty);
    fprintf(f, "match_offside=%d\nmatch_fouls=%d\nmatch_cards=%d\nmatch_extra_time=%d\n",
            cfg->match_custom.offside, cfg->match_custom.fouls,
            cfg->match_custom.cards, cfg->match_custom.extra_time);
    fprintf(f, "active_mod_packs=%s\n", cfg->active_mod_packs);
    fprintf(f, "hd_texture_packs=%s\n", cfg->hd_texture_packs);
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
    for (int p = 0; p < ISSD_PROFILE_PLAYERS; p++) {
        const IssdPlayerProfile *profile = &cfg->player_profiles[p];
        fprintf(f, "player_%d_schema=%d\n", p + 1, clamp(profile->schema, 0, 3));
        fprintf(f, "player_%d_stick_deadzone=%d\n", p + 1, clamp(profile->stick_deadzone, 0, 30000));
        fprintf(f, "player_%d_trigger_deadzone=%d\n", p + 1, clamp(profile->trigger_deadzone, 0, 30000));
        for (int b = 0; b < ISSD_PROFILE_BINDINGS; b++)
            fprintf(f, "player_%d_bind_%d=%" PRIu64 "\n", p + 1, b, profile->bindings[b] & ISSD_INPUT_SOURCE_MASK);
    }
    for (int i = 0; i < ISSD_TOUCH_CONTROLS; i++) {
        fprintf(f, "touch_%d_x=%d\n", i, clamp(cfg->touch_x[i], -1, 1000));
        fprintf(f, "touch_%d_y=%d\n", i, clamp(cfg->touch_y[i], -1, 1000));
        fprintf(f, "touch_%d_size=%d\n", i, clamp(cfg->touch_size[i], 50, 200));
    }

    fclose(f);
    printf("[Config] Saved configuration to '%s'.\n", path);
    return true;
}
