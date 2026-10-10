#include "issd_stadium_geometry.h"
#include <stdlib.h>
#include <string.h>

static bool sample(const IssdStadiumGeometry *source, unsigned layer,
                   int x, int y, uint8_t word[2]) {
    if (x < 0 || y < 0 || x >= source->stride*4) return false;
    unsigned index = (unsigned)(y >> 8)*source->stride +
        ((unsigned)(y & 224) >> 2) + (unsigned)(x >> 8)*64 + (unsigned)(x & 255)/32;
    if (index >= 4096) return false;
    unsigned offset = source->world_maps[layer][index]*32 +
        (unsigned)(y & 31)/8*8 + (unsigned)(x & 31)/8*2;
    memcpy(word, source->metatiles[layer]+offset, 2);
    return true;
}
bool issd_stadium_geometry_compile(const IssdStadiumGeometry *original,
                                   const IssdStadiumProfile *profile,
                                   IssdStadiumGeometry *output) {
    static const uint16_t lengths[8] = {1792,1856,1984,2048,1920,1920,1792,2176};
    static const uint16_t widths[8] = {576,640,704,640,640,576,704,704};
    if (!original || !profile || !output || profile->version != 1 ||
        profile->base_layout >= 8 || original->base_layout != profile->base_layout ||
        original->length != lengths[profile->base_layout] ||
        original->width != widths[profile->base_layout] ||
        profile->width_units != original->width || profile->length_units < 1536 ||
        profile->length_units > original->length || (profile->length_units & 31) ||
        original->stride < 0x80 || original->stride > 0x340 || (original->stride & 63))
        return false;
    int length = profile->length_units, width = profile->width_units;
    int delta = original->length - length;
    if (!delta) { *output = *original; return true; }
    IssdStadiumGeometry *result = calloc(1, sizeof *result);
    if (!result) return false;
    result->base_layout = original->base_layout;
    result->length = profile->length_units; result->width = profile->width_units;
    result->stride = original->stride;
    for (unsigned layer = 0; layer < 2; ++layer) {
        unsigned used = 0;
        for (unsigned index = 0; index < 4096; ++index) {
            int x0 = (int)((index % original->stride)/64)*256 + (int)(index & 7)*32;
            int y0 = (int)(index/original->stride)*256 + (int)((index & 63)/8)*32;
            uint8_t block[32];
            for (int dy = 0; dy < 32; dy += 8) {
                for (int dx = 0; dx < 32; dx += 8) {
                    int x = x0+dx, y = y0+dy;
                    int u = x-y+64, v = y+4-(layer == 0 ? 240 : 224);
                    bool band = abs(v-width/2) <= 80;
                    bool old_pitch = u >= -8 && u <= original->length+8 && v >= -8 && v <= width+8;
                    bool old_net = band && ((u >= -80 && u <= 0) ||
                        (u >= original->length && u <= original->length+80));
                    unsigned offset = (unsigned)dy+(unsigned)dx/4;
                    if (old_pitch || old_net) {
                        bool pitch = u >= -8 && u <= length+8 && v >= -8 && v <= width+8;
                        bool net = band && ((u >= -80 && u <= 0) ||
                            (u >= length && u <= length+80));
                        int sx;
                        if ((pitch || net) && u <= 384) sx = x;
                        else if ((pitch || net) && u >= length-384) sx = x+delta;
                        else if (pitch && abs(u-length/2) <= 192) sx = x+delta/2;
                        else {
                            int remainder = (u-512)%128;
                            if (remainder < 0) remainder += 128;
                            sx = 512+remainder+y-64;
                        }
                        if (!sample(original, layer, sx, y, block+offset)) {
                            free(result); return false;
                        }
                    } else {
                        unsigned source = original->world_maps[layer][index]*32+offset;
                        memcpy(block+offset, original->metatiles[layer]+source, 2);
                    }
                }
            }
            unsigned identity = 0;
            while (identity < used && memcmp(result->metatiles[layer]+identity*32, block, 32))
                ++identity;
            if (identity == used) {
                if (used == 256) { free(result); return false; }
                memcpy(result->metatiles[layer]+used*32, block, 32);
                ++used;
            }
            result->world_maps[layer][index] = (uint8_t)identity;
        }
    }
    *output = *result;
    free(result);
    return true;
}
