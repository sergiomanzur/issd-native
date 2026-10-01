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
    if (cfg.gameplay_goalkeeper_ai || cfg.gameplay_player_ai) return 16;
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

    IssdConfig loaded;
    if (!issd_config_load(&loaded, argv[1])) return 9;
    if (strcmp(loaded.rom_path, cfg.rom_path) != 0) return 10;
    if (strcmp(loaded.mods_dir, cfg.mods_dir) != 0) return 11;
    if (loaded.aspect_ratio != ISSD_ASPECT_16_9) return 12;
    if (!loaded.true_widescreen) return 13;
    if (loaded.internal_res != ISSD_RES_1X) return 14;
    if (loaded.master_volume != 70) return 15;
    if (!loaded.gameplay_goalkeeper_ai || !loaded.gameplay_player_ai) return 17;
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
    fputs("player_1_stick_deadzone=-999999999999999999999999999\n"
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
    fputs("master_volume=55\nkey_p1_a=30\n", f);
    fclose(f);
    assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.master_volume == 55 && loaded.key_p1_a == 30);
    assert(loaded.player_profiles[3].schema == 0 && loaded.player_profiles[3].stick_deadzone == 12000);
    assert(loaded.player_profiles[3].bindings[11] == ((UINT64_C(1) << 10) | (UINT64_C(1) << 36)));

    puts("config persistence tests passed");
    return 0;
}
