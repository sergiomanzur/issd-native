#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_mod.h"
#include "issd_stadium.h"
#include "issd_stadium_assets.h"

uint8_t g_ram[0x20000];

static int load(const char *directory, const char *name, const char *profile) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.json", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "{\"name\":\"%s\",\"stadiums\":[{\"stadium_id\":8,"
                  "\"stadium_profile\":%s}]}", name, profile);
    fclose(file);
    return issd_mod_load_pack(path);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    issd_mod_init();
    int first = load(argv[1], "first", "{\"version\":1,\"base_layout\":0,"
        "\"geometry\":{\"length_units\":1728,\"width_units\":576}}");
    assert(first >= 0);
    issd_mod_set_pack_enabled(first, true);
    assert(issd_stadium_rebuild_registry());
    const IssdStadiumProfile *profile = issd_stadium_profile(8);
    assert(profile && profile->length_units == 1728 && profile->base_layout == 0);
    uint8_t original[32], changed[32];
    issd_stadium_gameplay_digest(original);
    unsigned generation = issd_stadium_generation();
    assert(issd_stadium_rebuild_registry());
    assert(issd_stadium_generation() == generation);
    int second = load(argv[1], "second", "{\"version\":1,\"base_layout\":0,"
        "\"geometry\":{\"length_units\":1664,\"width_units\":576}}");
    assert(second >= 0);
    issd_mod_set_pack_enabled(second, true);
    assert(issd_stadium_rebuild_registry());
    assert(issd_stadium_profile(8)->length_units == 1664);
    issd_stadium_gameplay_digest(changed);
    assert(memcmp(original, changed, 32) != 0);
    assert(issd_stadium_generation() > generation);
    int bad = load(argv[1], "bad", "{\"version\":1,\"base_layout\":0,"
        "\"geometry\":{\"length_units\":1664.0,\"width_units\":576}}");
    assert(bad < 0); /* No float-to-integer coercion in native parsing. */
    bad = load(argv[1], "version", "{\"version\":2,\"base_layout\":0,"
        "\"geometry\":{\"length_units\":1664,\"width_units\":576}}");
    assert(bad < 0);
    int missing = load(argv[1], "missing", "{\"version\":1,\"base_layout\":0,"
        "\"geometry\":{\"length_units\":1600,\"width_units\":576},"
        "\"artwork\":\"missing/stadium.json\"}");
    assert(missing >= 0);
    issd_mod_set_pack_enabled(missing, true);
    assert(!issd_stadium_rebuild_registry());
    assert(issd_stadium_profile(8)->length_units == 1664);
    issd_stadium_gameplay_digest(original);
    assert(memcmp(original, changed, 32) == 0);
    issd_mod_set_pack_enabled(missing, false);
    int art = load(argv[1], "artpack", "{\"version\":1,\"base_layout\":0,"
        "\"geometry\":{\"length_units\":1600,\"width_units\":576},"
        "\"artwork\":\"art/stadium.json\"}");
    assert(art >= 0); issd_mod_set_pack_enabled(art, true);
    assert(issd_stadium_rebuild_registry());
    const IssdStadiumAssets *assets = issd_stadium_assets(8);
    assert(assets && assets->writes[0].data[0] == 0);
    generation = issd_stadium_generation();
    issd_stadium_gameplay_digest(original);
    assert(issd_stadium_rebuild_registry());
    assert(issd_stadium_assets(8) == assets);
    assert(issd_stadium_generation() == generation);
    char tiles[1024]; snprintf(tiles,sizeof tiles,"%s/art/tiles.bin",argv[1]);
    FILE *file = fopen(tiles,"wb"); assert(file);
    for (unsigned i = 0; i < 32; ++i) fputc(i ? 0 : 1,file);
    fclose(file);
    assert(issd_stadium_rebuild_registry());
    assert(issd_stadium_assets(8)->writes[0].data[0] == 1);
    assert(issd_stadium_generation() > generation);
    issd_stadium_gameplay_digest(changed);
    assert(!memcmp(original,changed,32)); /* Cosmetic changes stay out of saves. */
    issd_stadium_clear();
    assert(!issd_stadium_has_profiles());
    assert(!issd_stadium_profile(8));
    puts("native stadium profile tests passed");
    return 0;
}
