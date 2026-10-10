#include "issd_stadium_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 4);
    FILE *file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file); assert(length > 0);
    rewind(file);
    uint8_t *rom = malloc((size_t)length); assert(rom);
    assert(fread(rom, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    IssdStadiumGeometry original;
    memset(&original, 0x5a, sizeof original);
    assert(issd_stadium_read_template(rom, (size_t)length, (unsigned)atoi(argv[3]), &original));
    IssdStadiumGeometry saved = original;
    assert(!issd_stadium_read_template(rom, 16, 0, &original));
    assert(!issd_stadium_read_template(rom, (size_t)length, 8, &original));
    assert(memcmp(&original, &saved, sizeof saved) == 0);
    file = fopen(argv[2], "wb"); assert(file);
    for (unsigned i = 0; i < 2; ++i) {
        assert(fwrite(original.metatiles[i], 1, 8192, file) == 8192);
        assert(fwrite(original.world_maps[i], 1, 4096, file) == 4096);
    }
    fclose(file);
    uint8_t vram[65536],palette[512];
    assert(issd_stadium_read_native_resources(rom,(size_t)length,(unsigned)atoi(argv[3]),vram,palette));
    char pixels[4096];snprintf(pixels,sizeof pixels,"%s.pixels",argv[2]);
    file=fopen(pixels,"wb");assert(file);
    assert(fwrite(vram,1,sizeof vram,file)==sizeof vram);
    assert(fwrite(palette,1,sizeof palette,file)==sizeof palette);
    fclose(file);free(rom);
    return 0;
}
