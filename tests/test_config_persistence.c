#include <stdio.h>
#include <string.h>
#include "issd_config.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <config-path>\n", argv[0]);
        return 2;
    }

    IssdConfig cfg;
    issd_config_init_defaults(&cfg);
    cfg.aspect_ratio = ISSD_ASPECT_16_9;
    cfg.true_widescreen = true;
    cfg.internal_res = ISSD_RES_1X;
    cfg.master_volume = 70;
    strcpy(cfg.rom_path, "C:\\Games\\International Superstar Soccer Deluxe (USA).sfc");

    if (!issd_config_save(&cfg, argv[1])) return 3;

    FILE *f = fopen(argv[1], "r");
    if (!f) return 4;
    char contents[4096];
    size_t n = fread(contents, 1, sizeof(contents) - 1, f);
    fclose(f);
    contents[n] = '\0';
    if (contents[0] == '{') return 5;
    if (!strstr(contents, "rom_path=")) return 6;
    if (!strstr(contents, "aspect_ratio=2")) return 7;

    IssdConfig loaded;
    if (!issd_config_load(&loaded, argv[1])) return 8;
    if (strcmp(loaded.rom_path, cfg.rom_path) != 0) return 9;
    if (loaded.aspect_ratio != ISSD_ASPECT_16_9) return 10;
    if (!loaded.true_widescreen) return 11;
    if (loaded.internal_res != ISSD_RES_1X) return 12;
    if (loaded.master_volume != 70) return 13;

    puts("config persistence tests passed");
    return 0;
}
