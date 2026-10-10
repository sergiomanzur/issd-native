#include "issd_stadium_rom.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static unsigned offset(unsigned address) {
    return ((address & 0x7f0000) >> 1) | (address & 0x7fff);
}
static unsigned word(const unsigned char *rom, unsigned address) {
    unsigned index = offset(address);
    return rom[index] | ((unsigned)rom[index+1] << 8);
}
int main(void) {
    unsigned char *rom = malloc(0x200000);
    assert(rom);
    memset(rom, 0x5a, 0x200000);
    IssdStadiumProfile profile = {0};
    profile.version = 1; profile.base_layout = 0;
    profile.length_units = 1728; profile.width_units = 576;
    IssdStadiumRom scene = {0};
    assert(issd_stadium_rom_build(&scene, rom, 0x200000, &profile));
    assert(scene.data != rom && scene.size == 0x200000);
    assert(word(scene.data, 0x81ec47) == 1728);
    assert(word(scene.data, 0x81ec49) == 576);
    assert(word(scene.data, 0x81ec67) == 1024);
    assert(word(scene.data, 0x81ec69) == 160);
    assert(word(scene.data, 0x81ef21) == 1184);
    assert(word(scene.data, 0x838b28) == 1736);
    assert(word(scene.data, 0x838e1b) == 864);
    assert(word(rom, 0x81ec47) == 0x5a5a);
    assert(word(scene.data, 0x81ec4b) == 0x5a5a);
    assert(word(scene.data, 0x838eba) == 0x5a5a);
    unsigned char *previous = scene.data;
    profile.base_layout = 8;
    assert(!issd_stadium_rom_build(&scene, rom, 0x200000, &profile));
    assert(scene.data == previous);
    profile.base_layout = 0; profile.length_units = 1664;
    assert(!issd_stadium_rom_build(&scene, rom, 16, &profile));
    assert(scene.data == previous);
    assert(issd_stadium_rom_build(&scene, rom, 0x200000, &profile));
    assert(word(scene.data, 0x81ec47) == 1664);
    issd_stadium_rom_clear(&scene);
    assert(!scene.data && !scene.size);
    issd_stadium_rom_clear(&scene);
    free(rom);
    return 0;
}
