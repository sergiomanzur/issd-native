#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "issd_config.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <config-path>\n", argv[0]);
        return 2;
    }

    IssdConfig cfg;
    issd_config_init_defaults(&cfg);
    assert(cfg.camera_mode == ISSD_CAMERA_CLASSIC);
    cfg.camera_mode = ISSD_CAMERA_TACTICAL_WIDE;
    assert(!cfg.integer_scaling && cfg.output_resolution == 0 && cfg.overlay_scale == 0);
    assert(!cfg.ball_outline && !cfg.ball_shadow && !cfg.player_markers && !cfg.player_names);
    assert(!cfg.enhanced_running_animation);
    assert(!cfg.color_boost && cfg.crt_strength==100);
    assert(cfg.radar_scale == 1);
    assert(cfg.hud_scale == 1 && cfg.radar_position == 0 && cfg.radar_opacity == 75);
    IssdConfig preset = cfg;
    preset.output_resolution = 4;
    preset.gameplay_bug_fixes = true;
    preset.master_volume = 35;
    strcpy(preset.active_mod_packs, "test roster");
    for (int p = ISSD_VISUAL_ORIGINAL; p <= ISSD_VISUAL_ENHANCED; ++p) {
        issd_config_visual_preset(&preset, p);
        assert(issd_config_visual_preset_id(&preset) == p);
        assert(preset.enhanced_running_animation == (p == ISSD_VISUAL_ENHANCED));
        assert(!preset.color_boost && !preset.ball_outline && preset.crt_strength==100);
        preset.enhanced_running_animation = !preset.enhanced_running_animation;
        assert(issd_config_visual_preset_id(&preset) == ISSD_VISUAL_CUSTOM);
        issd_config_visual_preset(&preset, p);
        assert(preset.output_resolution == 4 && preset.gameplay_bug_fixes);
        assert(preset.master_volume == 35 && !strcmp(preset.active_mod_packs, "test roster"));
    }
    assert(preset.radar_scale == 2 && preset.radar_position == 2 && !preset.ball_outline);
    preset.player_names = false;
    assert(issd_config_visual_preset_id(&preset) == ISSD_VISUAL_CUSTOM);
    IssdConfig unchanged = preset;
    issd_config_visual_preset(&preset, ISSD_VISUAL_CUSTOM);
    assert(!memcmp(&unchanged, &preset, sizeof(preset)));
    cfg.integer_scaling = true;
    cfg.output_resolution = 4;
    cfg.overlay_scale = 3;
    cfg.scaling_filter = ISSD_FILTER_SHARP;
    cfg.ball_outline = cfg.ball_shadow = cfg.player_markers = cfg.player_names = true;
    cfg.radar_scale = 3;
    cfg.hud_scale = 3;
    cfg.radar_position = 4;
    cfg.radar_opacity = 50;
    if (cfg.gameplay_goalkeeper_ai || cfg.gameplay_player_ai || cfg.gameplay_bug_fixes) return 16;
    for (int i = 0; i < ISSD_PROFILE_PLAYERS; i++) {
        assert(cfg.player_profiles[i].schema == 0);
        assert(cfg.player_profiles[i].stick_deadzone == 12000);
        assert(cfg.player_profiles[i].trigger_deadzone == 12000);
        assert(cfg.player_profiles[i].bindings[4] == ((UINT64_C(1) << 11) | (UINT64_C(1) << 32)));
        cfg.player_profiles[i].stick_deadzone = 5000 + i * 1000;
        cfg.player_profiles[i].trigger_deadzone = 7000 + i * 1000;
        issd_config_player_preset(&cfg, i, i % 3);
    }
    cfg.player_profiles[3].schema = ISSD_PROFILE_CUSTOM;
    cfg.player_profiles[3].bindings[8] = (UINT64_C(1) << 20) | (UINT64_C(1) << 37);
    cfg.player_profiles[3].bindings[0] = 0;
    cfg.touch_x[0] = 333;
    cfg.touch_y[0] = 777;
    cfg.touch_size[0] = 150;
    cfg.gameplay_goalkeeper_ai = true;
    cfg.gameplay_player_ai = true;
    cfg.gameplay_bug_fixes = true;
    cfg.aspect_ratio = ISSD_ASPECT_16_9;
    cfg.true_widescreen = true;
    cfg.internal_res = ISSD_RES_1X;
    cfg.master_volume = 70;
    strcpy(cfg.rom_path, "C:\\Games\\International Superstar Soccer Deluxe (USA).sfc");
    strcpy(cfg.mods_dir, "C:\\Games\\ISSD Mods");

    if (!issd_config_save(&cfg, argv[1])) return 3;

    FILE *f = fopen(argv[1], "r");
    if (!f) return 4;
    char contents[8192];
    size_t n = fread(contents, 1, sizeof(contents) - 1, f);
    fclose(f);
    contents[n] = '\0';
    if (contents[0] == '{') return 5;
    if (!strstr(contents, "rom_path=")) return 6;
    if (!strstr(contents, "mods_dir=")) return 7;
    if (!strstr(contents, "aspect_ratio=2")) return 8;
    assert(strstr(contents, "enhanced_running_animation=0\n"));
    assert(strstr(contents, "color_boost=0\n"));
    assert(strstr(contents, "crt_strength=100\n"));
    assert(strstr(contents, "ball_outline=0\n"));

    IssdConfig loaded;
    if (!issd_config_load(&loaded, argv[1])) return 9;
    assert(loaded.integer_scaling && loaded.output_resolution == 4 && loaded.overlay_scale == 3);
    assert(loaded.camera_mode == ISSD_CAMERA_TACTICAL_WIDE);
    assert(loaded.scaling_filter == ISSD_FILTER_SHARP && loaded.radar_scale == 3);
    assert(loaded.hud_scale == 3 && loaded.radar_position == 4 && loaded.radar_opacity == 50);
    assert(!loaded.ball_outline && loaded.ball_shadow && loaded.player_markers && loaded.player_names);
    assert(!loaded.enhanced_running_animation);
    loaded.enhanced_running_animation = true;
    loaded.camera_mode = ISSD_CAMERA_TACTICAL;
    assert(issd_config_save(&loaded, argv[1]));
    assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.enhanced_running_animation);
    assert(loaded.camera_mode == ISSD_CAMERA_TACTICAL);
    if (strcmp(loaded.rom_path, cfg.rom_path) != 0) return 10;
    if (strcmp(loaded.mods_dir, cfg.mods_dir) != 0) return 11;
    if (loaded.aspect_ratio != ISSD_ASPECT_16_9) return 12;
    if (!loaded.true_widescreen) return 13;
    if (loaded.internal_res != ISSD_RES_1X) return 14;
    if (loaded.master_volume != 70) return 15;
    if (!loaded.gameplay_goalkeeper_ai || !loaded.gameplay_player_ai || !loaded.gameplay_bug_fixes) return 17;
    for (int i = 0; i < ISSD_PROFILE_PLAYERS; i++) {
        assert(loaded.player_profiles[i].schema == cfg.player_profiles[i].schema);
        assert(loaded.player_profiles[i].stick_deadzone == cfg.player_profiles[i].stick_deadzone);
        assert(loaded.player_profiles[i].trigger_deadzone == cfg.player_profiles[i].trigger_deadzone);
        assert(!memcmp(loaded.player_profiles[i].bindings, cfg.player_profiles[i].bindings,
                       sizeof(cfg.player_profiles[i].bindings)));
    }
    assert(loaded.touch_x[0] == 333 && loaded.touch_y[0] == 777 && loaded.touch_size[0] == 150);
    assert(loaded.touch_x[10] == -1 && loaded.touch_size[10] == 100);

    f = fopen(argv[1], "w");
    assert(f);
    fputs("camera_mode=999\noutput_resolution=999\noverlay_scale=-4\nradar_scale=999\nscaling_filter=99\n"
          "hud_scale=999\nradar_position=-3\nradar_opacity=-100\n"
          "player_1_stick_deadzone=-999999999999999999999999999\n"
          "player_1_trigger_deadzone=999999999999999999999999999\n"
          "player_2_stick_deadzone=oops\n"
          "player_2_trigger_deadzone=8000junk\n"
          "player_1_bind_0=18446744073709551615\n"
          "player_1_bind_1=18446744073709551616\n"
          "player_1_bind_2=-1\n"
          "player_1_bind_3=8junk\n"
          "player_2_bind_8=0\n"
          "player_2_schema=2\n"
          "player_999999999999999999999999999999_bind_9999=1\n"
          "player_1_bind_-1=1\n"
          "touch_0_x=-100\ntouch_0_y=5000\ntouch_0_size=999\n"
          "touch_10_size=-999\ntouch_1_x=oops\n", f);
    fclose(f);
    assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.output_resolution == 4 && loaded.overlay_scale == 0 && loaded.radar_scale == 3);
    assert(loaded.scaling_filter == ISSD_FILTER_SHARP);
    assert(loaded.hud_scale == 3 && loaded.radar_position == 0 && loaded.radar_opacity == 25);
    assert(loaded.player_profiles[0].stick_deadzone == 0);
    assert(loaded.player_profiles[0].trigger_deadzone == 30000);
    assert(loaded.player_profiles[1].stick_deadzone == 12000);
    assert(loaded.player_profiles[1].trigger_deadzone == 12000);
    assert(loaded.player_profiles[0].bindings[0] == ISSD_INPUT_SOURCE_MASK);
    assert(loaded.player_profiles[0].bindings[1] == cfg.player_profiles[0].bindings[1]);
    assert(loaded.player_profiles[0].bindings[2] == (UINT64_C(1) << 4));
    assert(loaded.player_profiles[0].bindings[3] == (UINT64_C(1) << 6));
    assert(loaded.player_profiles[1].schema == 2 && loaded.player_profiles[1].bindings[8] == 0);
    assert(loaded.player_profiles[1].bindings[1] & (UINT64_C(1) << 1));
    assert(loaded.touch_x[0] == -1 && loaded.touch_y[0] == 1000 && loaded.touch_size[0] == 200);
    assert(loaded.touch_size[10] == 50 && loaded.touch_x[1] == -1);

    /* Old files without profile/touch keys retain the original CLASSIC mapping. */
    f = fopen(argv[1], "w");
    assert(f);
    fputs("master_volume=55\nkey_p1_a=30\nscaling_filter=2\ninteger_scaling=1\n", f);
    fclose(f);
    assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.master_volume == 55 && loaded.key_p1_a == 30);
    assert(loaded.scaling_filter == ISSD_FILTER_CRT && loaded.integer_scaling);
    assert(loaded.output_resolution == 0 && loaded.overlay_scale == 0 && loaded.radar_scale == 1);
    assert(!loaded.ball_outline && !loaded.ball_shadow && !loaded.player_markers && !loaded.player_names);
    assert(!loaded.enhanced_running_animation);
    /* Malformed new settings do not overwrite defaults; old filter values
     * remain stable across a fresh load and save. */
    f = fopen(argv[1], "w");
    assert(f);
    fputs("output_resolution=oops\noverlay_scale=4junk\nradar_scale=oops\n"
          "ball_outline=oops\nball_shadow=oops\nplayer_markers=oops\nplayer_names=oops\n"
          "scaling_filter=oops\ninternal_res=99\naspect_ratio=-5\n"
          "enhanced_running_animation=1junk\n", f);
    fclose(f);
    assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.output_resolution == 0 && loaded.overlay_scale == 0 && loaded.radar_scale == 1);
    assert(!loaded.ball_outline && !loaded.ball_shadow && !loaded.player_markers && !loaded.player_names);
    assert(loaded.scaling_filter == ISSD_FILTER_LINEAR && loaded.internal_res == ISSD_RES_8X_4K);
    assert(loaded.aspect_ratio == ISSD_ASPECT_4_3);
    assert(!loaded.enhanced_running_animation);
    const char *running_values[] = {"-99", "999999999999999999999999", "oops", "0", "1"};
    const bool running_expected[] = {false, true, false, false, true};
    for (int i = 0; i < 5; ++i) {
        f = fopen(argv[1], "w"); assert(f);
        fprintf(f, "enhanced_running_animation=%s\n", running_values[i]);
        fclose(f);
        assert(issd_config_load(&loaded, argv[1]));
        assert(loaded.enhanced_running_animation == running_expected[i]);
    }
    const char *strength_values[]={"-999","999999999999999999999","oops","1junk","37","63","0","25","50","75","100"};
    const int strength_expected[]={0,100,100,100,25,75,0,25,50,75,100};
    for(int i=0;i<11;i++) {
        f=fopen(argv[1],"w"); assert(f);
        fprintf(f,"ball_outline=1\ncolor_boost=1\ncrt_strength=%s\n",strength_values[i]);
        fclose(f);
        assert(issd_config_load(&loaded,argv[1]));
        assert(!loaded.ball_outline && loaded.color_boost && loaded.crt_strength==strength_expected[i]);
        assert(issd_config_save(&loaded,argv[1]));
        assert(issd_config_load(&loaded,argv[1]));
        assert(!loaded.ball_outline && loaded.color_boost && loaded.crt_strength==strength_expected[i]);
    }
    for (int filter = 0; filter <= 2; ++filter) {
        loaded.scaling_filter = (IssdScalingFilter)filter;
        assert(issd_config_save(&loaded, argv[1]));
        assert(issd_config_load(&loaded, argv[1]));
        assert((int)loaded.scaling_filter == filter);
    }
    assert(loaded.player_profiles[3].schema == 0 && loaded.player_profiles[3].stick_deadzone == 12000);
    assert(loaded.player_profiles[3].bindings[11] == ((UINT64_C(1) << 10) | (UINT64_C(1) << 36)));

    assert(!loaded.gameplay_bug_fixes); /* old config preserves original behavior */
    puts("config persistence tests passed");
    return 0;
}
