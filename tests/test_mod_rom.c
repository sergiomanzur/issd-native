/* Mod packs patch the cartridge image, so the encoding has to be exact: the
 * game does not read ASCII and its ratings are 4 bit fields, not percentages.
 * Runs on a synthetic buffer - no ROM needed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_mod.h"

#define ROM_NAME_BASE 229774u
#define ROM_ATTR_BASE 327680u
#define PLAYERS       20
#define NAME_BYTES     8
#define ATTR_BYTES     7

/* Big enough to hold both tables. */
static uint8_t rom[3 * 1024 * 1024];

static size_t name_at(int team, int player) {
    return ROM_NAME_BASE + ((size_t)team * PLAYERS + player) * NAME_BYTES;
}
static size_t attr_at(int team, int player) {
    return ROM_ATTR_BASE + ((size_t)team * PLAYERS + player) * ATTR_BYTES;
}

static uint8_t rating_nibble_for_50(void) { return (uint8_t)((50u*15u+49u)/99u); }

int main(void) {
    assert(issd_mod_init());
    /* The repo ships mods/ next to the binary; tests run from the repo root. */
    int packs = issd_mod_scan_and_load("tests/fixtures/mods");
    if (packs <= 0) {
        puts("mod rom tests skipped: no fixture packs");
        return 0;
    }

    memset(rom, 0xEE, sizeof(rom));
    /* Byte 5 of each attribute record is an index we do not understand and
     * must survive untouched. */
    rom[attr_at(0, 0) + 5] = 0xA5;

    int patched = issd_mod_apply_to_rom(rom, sizeof(rom));
    assert(patched > 0);

    /* --- name encoding: letters from 0x68, space is 0x00 padding --- */
    const uint8_t *n = rom + name_at(0, 0);
    assert(n[0] == 0x68);   /* 'A' */
    assert(n[1] == 0x83);   /* 'b' */
    assert(n[2] == 0x84);   /* 'c' */
    assert(n[3] == 0x00);   /* padded, renders as a space */
    assert(n[7] == 0x00);

    /* A name longer than the field must not run into the next player. */
    const uint8_t *long_name = rom + name_at(0, 1);
    for (int i = 0; i < NAME_BYTES; i++) assert(long_name[i] != 0xEE);
    assert(rom[name_at(0, 2)] != 0xEE);   /* player 2 written, not clobbered */

    /* --- attributes are nibble pairs, 0..99 quantised to 0..15 --- */
    const uint8_t *a = rom + attr_at(0, 0);
    assert((a[0] >> 4) == 15);    /* acceleration 99 -> max */
    assert((a[0] & 0x0F) == 0);   /* speed 0 -> min, must not wrap to max */
    assert(a[5] == 0xA5);         /* untouched index byte preserved */

    /* Position lives in the high nibble of byte 4 and must be written, not
     * preserved: a pack naming a goalkeeper has to make him one. Codes read
     * off the cartridge, where slots 0 and 11 carry 1 on all 36 teams. */
    assert((a[4] >> 4) == 1);                          /* slot 0 is GK */
    assert((rom[attr_at(0, 2) + 4] >> 4) == 6);        /* slot 2 is FW */
    assert((a[4] & 0x0F) == rating_nibble_for_50());   /* stamina still packed */

    /* skin tone in the high nibble, hair style in the low one */
    assert((a[6] >> 4) == 1);
    assert((a[6] & 0x0F) == 13);

    /* --- a team beyond the cartridge's table must be refused, not written
     *     past the end of the roster data --- */
    assert(rom[name_at(40, 0)] == 0xEE);   /* 36 teams: 40 is out of range */

    /* --- a ROM too small to hold the tables is refused outright --- */
    assert(issd_mod_apply_to_rom(rom, 1024) == 0);

    puts("mod rom tests passed");
    return 0;
}
