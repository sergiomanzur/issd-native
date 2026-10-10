#include "issd_stadium_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 4);
    IssdStadiumGeometry original = {0}, output;
    original.base_layout = 0; original.length = 1792;
    original.width = 576; original.stride = 704;
    FILE *file = fopen(argv[1], "rb"); assert(file);
    for (unsigned i = 0; i < 2; ++i) {
        assert(fread(original.metatiles[i], 1, 8192, file) == 8192);
        assert(fread(original.world_maps[i], 1, 4096, file) == 4096);
    }
    fclose(file);
    IssdStadiumProfile profile = {0}; profile.version = 1;
    profile.length_units = (uint16_t)atoi(argv[3]); profile.width_units = 576;
    assert(issd_stadium_geometry_compile(&original, &profile, &output));
    file = fopen(argv[2], "wb"); assert(file);
    for (unsigned i = 0; i < 2; ++i) {
        assert(fwrite(output.metatiles[i], 1, 8192, file) == 8192);
        assert(fwrite(output.world_maps[i], 1, 4096, file) == 4096);
    }
    fclose(file);
    IssdStadiumGeometry saved = output;
    profile.width_units = 544;
    assert(!issd_stadium_geometry_compile(&original, &profile, &output));
    assert(memcmp(&saved, &output, sizeof saved) == 0);
    return 0;
}
