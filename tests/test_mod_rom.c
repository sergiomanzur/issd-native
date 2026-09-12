/* Mod packs patch the cartridge image, so the encoding has to be exact: the
 * game does not read ASCII and its ratings are 4 bit fields, not percentages.
 * Runs on a synthetic buffer - no ROM needed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_mod.h"
#include "issd_formation.h"

#define ROM_NAME_BASE 229774u
#define ROM_ATTR_BASE 327680u
#define PLAYERS       20
#define NAME_BYTES     8
#define ATTR_BYTES     7

/* Thirty-six 16-bit pointers at $8B:EF48, each naming a 31-byte team shape
 * record in bank $8B. The test plants its own pointer so the record lands
 * somewhere it can be checked without needing a real cartridge. */
#define FORMATION_PTRS   0x05EF48u
#define FORMATION_BANK   0x0Bu
#define TEST_RECORD_ADDR 0x9000u
#define TEST_RECORD_OFF  ((size_t)FORMATION_BANK * 0x8000u + \
                          (TEST_RECORD_ADDR - 0x8000u))

/* Big enough to hold both tables. */
static uint8_t rom[3 * 1024 * 1024];

static size_t name_at(int team, int player) {
    return ROM_NAME_BASE + ((size_t)team * PLAYERS + player) * NAME_BYTES;
}
static size_t attr_at(int team, int player) {
    return ROM_ATTR_BASE + ((size_t)team * PLAYERS + player) * ATTR_BYTES;
}

static uint8_t rating_nibble_for_50(void) { return (uint8_t)(2u + (50u*7u+49u)/99u); }

int main(void) {
    assert(issd_mod_init());
    /* The repo ships mods/ next to the binary; tests run from the repo root. */
    int packs = issd_mod_scan_and_load("tests/fixtures/mods");
    if (packs <= 0) {
        puts("mod rom tests skipped: no fixture packs");
        return 0;
    }

    /* Packs are off until asked for, so a clean install runs vanilla. This
     * test is about the encoding one pack produces, so it enables that one
     * and leaves the others alone - stacking has its own test. */
    int fixture = -1;
    for (int i = 0; i < packs; i++) {
        const IssdModPack *p = issd_mod_get_pack(i);
        if (p && strcmp(p->name, "Fixture Pack") == 0) fixture = i;
    }
    assert(fixture >= 0);
    issd_mod_set_pack_enabled(fixture, true);

    memset(rom, 0xEE, sizeof(rom));
    /* Byte 5 of each attribute record is an index we do not understand and
     * must survive untouched. */
    rom[attr_at(0, 0) + 5] = 0xA5;
    /* Team 0's shape record is reached through the pointer table, never by
     * arithmetic, so the test can put it wherever it likes. */
    rom[FORMATION_PTRS + 0] = (uint8_t)(TEST_RECORD_ADDR & 0xFF);
    rom[FORMATION_PTRS + 1] = (uint8_t)(TEST_RECORD_ADDR >> 8);

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
    /* The cartridge only ever uses nibbles 2..9 across its own 720 players,
     * so an authored 0..99 maps onto that band. Mapping onto the full 0..15
     * put every normal football rating at 11..14: above anything the game
     * ships and squashed into so few steps that a squad came out identical. */
    assert((a[0] >> 4) == 9);     /* acceleration 99 -> the ceiling in use */
    assert((a[0] & 0x0F) == 2);   /* speed 0 -> the floor, and never wraps */
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

    /* --- the team shape, written through the pointer table ---
     *
     * The fixture asks for a 4-2-3-1 played attacking. The cartridge cannot
     * print a four-number label, so the screen says 4-5-1 - which is how a
     * 4-2-3-1 would have been written down in 1995 anyway. */
    const uint8_t *rec = rom + TEST_RECORD_OFF;
    const IssdFormation *want = issd_formation_find("4-2-3-1");
    assert(want != NULL);
    IssdFormation shaped;
    assert(issd_formation_apply_tactics(&shaped, want, "attacking"));

    assert(rec[0] == shaped.label);
    assert(strcmp(issd_formation_label_text(rec[0]), "4-5-1") == 0);
    for (int s = 0; s < ISSD_FORMATION_SLOTS; s++) {
        assert((int8_t)rec[1 + s * 2] == shaped.slots[s].depth);
        assert((int8_t)rec[2 + s * 2] == shaped.slots[s].width);
        assert(rec[21 + s] == shaped.slots[s].role);
    }
    /* Exactly 31 bytes: the next record must not be trampled. */
    assert(rom[TEST_RECORD_OFF + 31] == 0xEE);

    /* An unknown formation leaves the team's own shape alone rather than
     * writing a broken record. */
    IssdModPack *pack = issd_mod_get_pack(fixture);
    assert(pack != NULL);
    memset(rom + TEST_RECORD_OFF, 0xEE, 32);
    strcpy(pack->teams[0].formation, "6-6-6");
    assert(issd_mod_apply_to_rom(rom, sizeof(rom)) > 0);
    assert(rom[TEST_RECORD_OFF] == 0xEE);

    /* No formation named at all is the same: nothing written. */
    pack->teams[0].formation[0] = '\0';
    assert(issd_mod_apply_to_rom(rom, sizeof(rom)) > 0);
    assert(rom[TEST_RECORD_OFF] == 0xEE);

    /* --- a team beyond the cartridge's table must be refused, not written
     *     past the end of the roster data --- */
    assert(rom[name_at(40, 0)] == 0xEE);   /* 36 teams: 40 is out of range */

    /* --- a ROM too small to hold the tables is refused outright --- */
    assert(issd_mod_apply_to_rom(rom, 1024) == 0);

    /* The band boundaries are published in docs/MODDING.md so a pack
     * author can see what a rating will actually become. Pin them here so
     * the table and the code cannot drift apart in silence. */
    {
        const struct { uint8_t rating, stored; } bands[] = {
            {0, 2}, {7, 2}, {8, 3}, {21, 3}, {22, 4}, {35, 4},
            {36, 5}, {49, 5}, {50, 6}, {63, 6}, {64, 7}, {77, 7},
            {78, 8}, {91, 8}, {92, 9}, {99, 9},
        };
        IssdModPack *bp = issd_mod_get_pack(fixture);
        for (unsigned i = 0; i < sizeof bands / sizeof bands[0]; i++) {
            bp->teams[0].players[0].attributes.acceleration = bands[i].rating;
            issd_mod_apply_to_rom(rom, sizeof(rom));
            assert((rom[attr_at(0, 0)] >> 4) == bands[i].stored);
        }
    }

    /* --- stadiums -------------------------------------------------
     *
     * Eight fixed 7-byte name fields, right-aligned the way the cartridge
     * stores its own, and eight (length, width) pairs. */
    {
        const size_t NAME = 0x3CA9Bu, PITCH = 0x017AEDu;
        IssdModPack *sp = issd_mod_get_pack(fixture);
        memset(rom + NAME, 0xEE, 8 * 7);
        memset(rom + PITCH, 0xEE, 8 * 2);

        sp->stadium_count = 1;
        sp->stadiums[0].stadium_id = 7;
        snprintf(sp->stadiums[0].name, sizeof sp->stadiums[0].name, "AKRON");
        sp->stadiums[0].pitch_length = 115;
        sp->stadiums[0].pitch_width = 74;
        issd_mod_apply_to_rom(rom, sizeof(rom));

        const uint8_t *f = rom + NAME + 7 * 7;
        assert(f[0] == 0x00 && f[1] == 0x00);        /* right-aligned */
        assert(f[2] == 0x68 && f[3] == 0x72);        /* A K */
        assert(f[4] == 0x79 && f[5] == 0x76 && f[6] == 0x75);   /* R O N */
        assert(rom[PITCH + 7 * 2 + 0] == 115);
        assert(rom[PITCH + 7 * 2 + 1] == 74);
        /* Its neighbours are untouched: the fields are packed end to end. */
        assert(rom[NAME + 6 * 7] == 0xEE);
        assert(rom[PITCH + 6 * 2] == 0xEE);

        /* A stadium the cartridge does not have is refused, not written
         * past the end of the table. */
        sp->stadiums[0].stadium_id = 9;
        issd_mod_apply_to_rom(rom, sizeof(rom));
        assert(rom[NAME + 8 * 7] == 0xEE);

        /* Sizes outside the range the cartridge itself uses are clamped
         * rather than trusted. */
        sp->stadiums[0].stadium_id = 7;
        sp->stadiums[0].pitch_length = 250;
        sp->stadiums[0].pitch_width = 10;
        issd_mod_apply_to_rom(rom, sizeof(rom));
        assert(rom[PITCH + 7 * 2 + 0] == 140);
        assert(rom[PITCH + 7 * 2 + 1] == 64);
        sp->stadium_count = 0;
    }

    /* --- more than eight stadiums ---------------------------------
     *
     * The tables move into free space and the instructions that index
     * them are re-pointed. Everything is checked against the bytes it is
     * expected to hold first, so a cartridge that does not match is left
     * alone rather than corrupted. */
    {
        const size_t COUNT_OP = 0x121F6Du, NAME_OP = 0x03448Bu;
        const size_t FREE82 = 0x017B5Du, FREE87 = 0x03FAC8u;
        IssdModPack *xp = issd_mod_get_pack(fixture);

        /* A cartridge whose stadium code is not where we measured it must
         * be refused outright. */
        memset(rom, 0xEE, sizeof rom);
        xp->stadium_slots = 12;
        xp->stadium_count = 0;
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof(rom));
        assert(issd_mod_last_result()->errors > 0);
        assert(rom[COUNT_OP] == 0xEE && "a mismatched cartridge is not written");

        /* Now with the bytes the real cartridge has. */
        rom[COUNT_OP - 1] = 0xC9; rom[COUNT_OP] = 8; rom[COUNT_OP + 1] = 0;
        rom[NAME_OP - 1] = 0x69; rom[NAME_OP] = 0x9B; rom[NAME_OP + 1] = 0xCA;
        const size_t ops[4] = { 0x121FA0u, 0x12200Au, 0x12201Du, 0x12203Fu };
        const uint16_t was[4] = { 0xFADDu, 0xFAEDu, 0xFAEEu, 0xFAFDu };
        for (int i = 0; i < 4; i++) {
            rom[ops[i] - 1] = 0xBF;
            rom[ops[i]] = (uint8_t)(was[i] & 0xFF);
            rom[ops[i] + 1] = (uint8_t)(was[i] >> 8);
        }
        memset(rom + FREE82, 0xFF, 16 * 6);
        memset(rom + FREE87, 0xFF, 16 * 7);

        /* Expanding rewrites the very bytes it checks, so it runs once per
         * apply against a pristine image - which is what issd_mod_reapply
         * does. Ask for the new slot in the same pass. */
        xp->stadium_count = 1;
        xp->stadiums[0].stadium_id = 8;
        snprintf(xp->stadiums[0].name, sizeof xp->stadiums[0].name, "AKRON");
        xp->stadiums[0].pitch_length = 115;
        xp->stadiums[0].pitch_width = 74;

        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof(rom));
        assert(rom[COUNT_OP] == 12 && "the count literal is raised");
        for (int i = 0; i < 4; i++) {
            const uint16_t now = (uint16_t)(rom[ops[i]] | (rom[ops[i]+1] << 8));
            assert(now != was[i] && "each table reference is re-pointed");
        }
        const uint16_t nm = (uint16_t)(rom[NAME_OP] | (rom[NAME_OP+1] << 8));
        assert(nm != 0xCA9B && "the name base is re-pointed");

        /* A slot past the cartridge's own eight really was written. */
        const uint8_t *n8 = rom + FREE87 + 8 * 7;
        assert(n8[2] == 0x68 && n8[3] == 0x72);      /* A K */

        /* One the expansion did not reach for is refused. */
        xp->stadiums[0].stadium_id = 15;
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof(rom));
        assert(issd_mod_last_result()->warnings > 0);
        xp->stadium_count = 0;
        xp->stadium_slots = 0;
    }

    puts("mod rom tests passed");
    return 0;
}
