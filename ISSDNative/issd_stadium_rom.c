#include "issd_stadium_rom.h"
#include <stdlib.h>
#include <string.h>

static size_t offset(unsigned address) {
    return ((address & 0x7f0000) >> 1) | (address & 0x7fff);
}
static uint16_t word(const uint8_t *rom, unsigned address) {
    size_t index = offset(address);
    return (uint16_t)(rom[index] | (unsigned)rom[index+1] << 8);
}
static void put_word(uint8_t *rom, unsigned address, unsigned value) {
    size_t index = offset(address);
    rom[index] = (uint8_t)value;
    rom[index+1] = (uint8_t)(value >> 8);
}
void issd_stadium_rom_clear(IssdStadiumRom *scene) {
    if (!scene) return;
    free(scene->data);
    memset(scene, 0, sizeof *scene);
}
bool issd_stadium_rom_build(IssdStadiumRom *scene, const uint8_t *canonical,
                            size_t size, const IssdStadiumProfile *profile) {
    static const uint16_t lengths[8] = {1792,1856,1984,2048,1920,1920,1792,2176};
    static const uint16_t widths[8] = {576,640,704,640,640,576,704,704};
    /* Keep the last valid view on failure. Introduction layout 8 is excluded.
     * Width and camera envelopes remain restricted by the registry contract. */
    if (!scene || !canonical || !profile || profile->version != 1 ||
        profile->base_layout > 7 || size < 0x200000 ||
        profile->length_units < 1536 || (profile->length_units & 31) ||
        profile->length_units > lengths[profile->base_layout] ||
        profile->width_units != widths[profile->base_layout]) return false;
    uint8_t *view = malloc(size);
    if (!view) return false;
    memcpy(view, canonical, size);
    unsigned base = profile->base_layout;
    unsigned length = profile->length_units, width = profile->width_units;
    unsigned delta = lengths[base] - length;
    put_word(view, 0x81ec47 + base*4, length);
    put_word(view, 0x81ec49 + base*4, width);
    put_word(view, 0x81ec67 + base*4, (length+width)/2-128);
    put_word(view, 0x81ec69 + base*4, width/2-128);
    put_word(view, 0x81ef21 + base*4, (length+width)/2+32);
    /* The right goal target translates with the right pitch end. Left target
     * and vertical framing retain the fixed-origin, unchanged-width template. */
    unsigned right = word(canonical, 0x81eedd + base*8 + 4);
    if (right < delta) { free(view); return false; }
    put_word(view, 0x81eedd + base*8 + 4, right-delta);
    put_word(view, 0x838b28, length+8);
    put_word(view, 0x838e1b, length/2);
    put_word(view, 0x8b82b1, (length+width)/2+32);
    /* Penalty depth/transverse comparisons and the separately audited
     * presentation threshold are deliberately retained from canonical ROM. */
    free(scene->data);
    scene->data = view;
    scene->size = size;
    return true;
}
