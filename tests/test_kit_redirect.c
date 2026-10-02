#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_mod.h"

static uint8_t rom[3 * 1024 * 1024];
#define KIT_BASE 0x0483C0u
#define HOME 0x01027Au
#define AWAY 0x0102D0u

static size_t record(unsigned index) { return KIT_BASE + index * 34u; }
static void pointer(size_t table, unsigned team, unsigned index) {
    uint16_t addr = (uint16_t)(0x8000u + record(index) - 0x48000u - 2u);
    rom[table + team * 2u] = (uint8_t)addr;
    rom[table + team * 2u + 1u] = (uint8_t)(addr >> 8);
}
static unsigned pointed_record(size_t table, unsigned team) {
    unsigned addr = rom[table + team * 2u] | (rom[table + team * 2u + 1u] << 8);
    return (0x48000u + addr - 0x8000u + 2u - KIT_BASE) / 34u;
}

int main(void) {
    assert(issd_mod_init());
    assert(issd_mod_scan_and_load("tests/fixtures/mods") > 0);
    issd_mod_enable_from_list("Fixture Pack");
    IssdModPack *pack = NULL;
    for (int i = 0; i < issd_mod_get_pack_count(); i++) {
        IssdModPack *candidate = issd_mod_get_pack(i);
        if (strcmp(candidate->name, "Fixture Pack") == 0) pack = candidate;
    }
    assert(pack);
    pack->team_count = 1;
    memset(&pack->teams[0], 0, sizeof pack->teams[0]);
    IssdModTeam *team = &pack->teams[0];
    team->team_id = 18;
    team->kit_record = 36;
    team->shirt_rgb = 0xFFC8102Eu;
    memset(rom, 0xEE, sizeof rom);
    for (unsigned i = 0; i < 84; i++) {
        rom[record(i) + 30] = 0x20; rom[record(i) + 31] = 1;
        rom[record(i) + 32] = 0x1F; rom[record(i) + 33] = 0;
    }
    pointer(HOME, 18, 17); pointer(HOME, 19, 17);
    pointer(AWAY, 18, 53); pointer(AWAY, 19, 53);
    uint8_t original_home[34], original_away[34];
    memcpy(original_home, rom + record(17), 34);
    memcpy(original_away, rom + record(53), 34);
    issd_mod_result_reset();
    issd_mod_apply_to_rom(rom, sizeof rom);
    assert(pointed_record(HOME, 18) == 36);
    assert(pointed_record(AWAY, 18) == 36);
    assert(pointed_record(HOME, 19) == 17 && pointed_record(AWAY, 19) == 53);
    assert(memcmp(original_home, rom + record(17), 34) == 0);
    assert(memcmp(original_away, rom + record(53), 34) == 0);
    uint8_t first_kit[34]; memcpy(first_kit, rom + record(36), 34);
    team->team_id = 19; team->kit_record = 37; team->shirt_rgb = 0xFF0022CCu;
    issd_mod_apply_to_rom(rom, sizeof rom);
    assert(pointed_record(HOME, 19) == 37 && pointed_record(AWAY, 19) == 37);
    assert(memcmp(first_kit, rom + record(36), 34) == 0);

    /* A bad palette signature or an out-of-range override cannot redirect. */
    team->kit_record = 38; rom[record(38) + 30] = 0;
    issd_mod_result_reset(); issd_mod_apply_to_rom(rom, sizeof rom);
    assert(issd_mod_last_result()->warnings > 0);
    assert(pointed_record(HOME, 19) == 37 && pointed_record(AWAY, 19) == 37);
    team->kit_record = 84;
    rom[record(84) + 30] = 0x20; rom[record(84) + 31] = 1;
    rom[record(84) + 32] = 0x1F; rom[record(84) + 33] = 0;
    issd_mod_result_reset(); issd_mod_apply_to_rom(rom, sizeof rom);
    assert(issd_mod_last_result()->warnings > 0);
    assert(pointed_record(HOME, 19) == 37 && pointed_record(AWAY, 19) == 37);
    /* An override that deliberately aliases another home still warns. */
    team->kit_record = 36;
    issd_mod_result_reset(); issd_mod_apply_to_rom(rom, sizeof rom);
    assert(issd_mod_last_result()->warnings > 0);
    assert(pointed_record(HOME, 19) == 36 && pointed_record(AWAY, 19) == 36);
    puts("kit redirect isolation passed");
    return 0;
}
