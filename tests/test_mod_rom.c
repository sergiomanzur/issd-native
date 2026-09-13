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

    /* --- the six all-star sides have ratings but no roster ------------
     *
     * The select screen offers 42 teams once the seventh group is
     * unlocked, but only 36 of them have players written down: the last
     * six pick theirs out of their group when the match loads. Writing
     * names at team*160 past the 36th would land on whatever follows the
     * table, so it must not happen - while ratings, which are real for
     * all 42, must still go in. */
    {
        IssdModPack *bp = issd_mod_get_pack(fixture);
        assert(bp);
        bp->teams[0].team_id = 40;
        bp->teams[0].formation[0] = '\0';
        issd_mod_result_reset();
        assert(issd_mod_apply_to_rom(rom, sizeof(rom)) > 0);
        assert(rom[name_at(40, 0)] == 0xEE &&
               "an all-star side has no roster to write names into");
        assert(rom[attr_at(40, 0)] != 0xEE &&
               "its ratings are real and must still be written");
        assert(issd_mod_last_result()->warnings > 0 &&
               "and the pack is told its names went nowhere");
        bp->teams[0].team_id = 0;
    }


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

    /* --- kits ---------------------------------------------------------
     *
     * A strip is seventeen colours at $89:83C0 + record * 34, and which
     * record a team wears was measured rather than found. Two things must
     * hold: a cartridge that does not have a kit there is left alone, and
     * the colour a pack asks for is what the lit shade becomes. */
    {
        /* A strip is reached through a table of addresses, not an index, so
         * the test plants a pointer as well as a palette. */
        const size_t KIT_BASE   = 0x0483C0u;
        const size_t KIT_PTRS   = 0x01027Au;
        const size_t rec        = KIT_BASE + 27u * 34u;
        const uint16_t kit_addr = (uint16_t)(0x8000u + (rec - 0x48000u) - 2u);

        issd_mod_init();
        const int n = issd_mod_scan_and_load("tests/fixtures/mods");
        assert(n > 0);
        issd_mod_enable_from_list("Fixture Pack");
        IssdModPack *kp = issd_mod_get_pack(0);
        assert(kp);
        kp->team_count = 1;
        memset(&kp->teams[0], 0, sizeof kp->teams[0]);
        kp->teams[0].team_id = 35;
        kp->teams[0].kit_record = -1;
        kp->teams[0].shirt_rgb  = 0xFFC8102Eu;
        kp->teams[0].shorts_rgb = 0xFF123A6Bu;
        kp->teams[0].socks_rgb  = 0xFFFFFFFFu;

        /* Wrong cartridge: neither a pointer table nor a palette, so
         * nothing is written. */
        memset(rom, 0xEE, sizeof rom);
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);
        for (unsigned w = 7; w < 15; w++)
            assert(rom[rec + w * 2] == 0xEE &&
                   "a cartridge without kits there is left alone");
        assert(issd_mod_last_result()->warnings > 0);

        /* Now with the bytes the real cartridge has: team 35's entry in the
         * table of addresses, pointing two bytes before its palette, and the
         * two constants every palette ends with. */
        rom[KIT_PTRS + 35 * 2]     = (uint8_t)(kit_addr & 0xFF);
        rom[KIT_PTRS + 35 * 2 + 1] = (uint8_t)(kit_addr >> 8);
        rom[rec + 30] = 0x20; rom[rec + 31] = 0x01;
        rom[rec + 32] = 0x1F; rom[rec + 33] = 0x00;
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);

        /* #C8102E lit: red 200 -> 24 of 31, green 16 -> 1, blue 46 -> 5. */
        const uint16_t lit = (uint16_t)(rom[rec + 9 * 2] |
                                        (rom[rec + 9 * 2 + 1] << 8));
        assert((lit & 31) == 200 * 31 / 255);
        assert(((lit >> 5) & 31) == 16 * 31 / 255);
        assert(((lit >> 10) & 31) == 46 * 31 / 255);

        /* The other two shades are darker, which is how the cartridge
         * shades its own strips. */
        const uint16_t mid = (uint16_t)(rom[rec + 8 * 2] |
                                        (rom[rec + 8 * 2 + 1] << 8));
        const uint16_t dark = (uint16_t)(rom[rec + 7 * 2] |
                                         (rom[rec + 7 * 2 + 1] << 8));
        assert((dark & 31) < (mid & 31) && (mid & 31) < (lit & 31));

        /* Socks are two shades, not three: words 13 and 14 and no further.
         * Word 15 is the first of the two constants every kit ends with,
         * and writing over it would be writing over the check itself. */
        assert(rom[rec + 13 * 2] != 0xEE && rom[rec + 14 * 2] != 0xEE);
        assert(rom[rec + 30] == 0x20 && rom[rec + 31] == 0x01 &&
               "the tail the check reads must survive the paint");

        /* A team whose entry the cartridge does not have is reported,
         * not guessed at. */
        kp->teams[0].team_id = 2;
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);
        assert(issd_mod_last_result()->warnings > 0);

        /* Naming the record directly overrides the table. */
        kp->teams[0].kit_record = 27;
        rom[rec + 9 * 2] = 0xEE; rom[rec + 9 * 2 + 1] = 0xEE;
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);
        assert(!(rom[rec + 9 * 2] == 0xEE && rom[rec + 9 * 2 + 1] == 0xEE) &&
               "kit_record must reach a team the table cannot");
    }

    /* --- adding a team rather than replacing one ----------------------
     *
     * The seventh group's six cells are real slots; what stops them
     * holding a squad is the compare at $80:CF2A, which sends anything
     * from 36 up to the code that assembles a side from its group. A pack
     * that adds a team raises that compare by exactly the number of teams
     * added - never further, or a slot with no roster behind it would read
     * the pointer table's dead entry - writes a roster into free space at
     * the end of bank $87, and points the table at it. */
    {
        const size_t GATE_A = 0x004F2Eu, GATE_B = 0x004F50u;
        const size_t PTRS = 0x038138u, PTRS_COPY = 0x0398AEu;
        const size_t ROSTERS = 0x03FC40u;

        issd_mod_init();
        assert(issd_mod_scan_and_load("tests/fixtures/mods") > 0);
        issd_mod_enable_from_list("Fixture Pack");
        IssdModPack *ap = issd_mod_get_pack(0);
        assert(ap);
        ap->team_count = 1;
        memset(&ap->teams[0], 0, sizeof ap->teams[0]);
        ap->teams[0].new_team = true;
        ap->teams[0].assigned_slot = -1;
        ap->teams[0].kit_record = -1;
        ap->teams[0].player_count = 2;
        snprintf(ap->teams[0].name, sizeof ap->teams[0].name, "Chivas");
        snprintf(ap->teams[0].players[0].name,
                 sizeof ap->teams[0].players[0].name, "Rangel");
        snprintf(ap->teams[0].players[1].name,
                 sizeof ap->teams[0].players[1].name, "Mozo");

        /* A cartridge whose squad loader is not where it was measured is
         * refused outright rather than written to. */
        memset(rom, 0xEE, sizeof rom);
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);
        assert(issd_mod_last_result()->errors > 0);
        assert(rom[GATE_A] == 0xEE && "the gate is left alone");
        assert(ap->teams[0].assigned_slot < 0 && "and no slot handed out");

        /* Now with the bytes the real cartridge has. */
        rom[GATE_A - 1] = 0xE0; rom[GATE_A] = 0x48; rom[GATE_A + 1] = 0x00;
        rom[GATE_B - 1] = 0xE0; rom[GATE_B] = 0x48; rom[GATE_B + 1] = 0x00;
        rom[PTRS] = 0x8E; rom[PTRS + 1] = 0x81;      /* entry 0 = $818E */
        rom[0x02A567u - 1] = 0xA2; rom[0x02A567u] = 6;

        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);

        assert(ap->teams[0].assigned_slot == 36 &&
               "an added team takes the first free slot");
        /* One team added, so the gate moves to 37 doubled and no further:
         * slot 37 upwards must still assemble from its group. */
        const uint16_t gate =
            (uint16_t)(rom[GATE_A] | (rom[GATE_A + 1] << 8));
        assert(gate == 37 * 2);
        assert((uint16_t)(rom[GATE_B] | (rom[GATE_B + 1] << 8)) == 37 * 2 &&
               "both sides of the match use the same loader");

        /* The table now names the block, in both of its copies. */
        const uint16_t addr = (uint16_t)(0x8000u + (ROSTERS - 0x38000u));
        const size_t entry = 36 * 2;
        assert((uint16_t)(rom[PTRS + entry] |
                          (rom[PTRS + entry + 1] << 8)) == addr);
        assert((uint16_t)(rom[PTRS_COPY + entry] |
                          (rom[PTRS_COPY + entry + 1] << 8)) == addr);

        /* And the names are in the block, not in the 36-team table. */
        assert(rom[ROSTERS + 0] == 0x79 && rom[ROSTERS + 1] == 0x82);  /* R a */
        assert(rom[name_at(36, 0)] == 0xEE &&
               "an added team must not write past the cartridge's table");

        /* Its ratings, shape and strip go where every team's do. */
        assert(rom[attr_at(36, 0)] != 0xEE);

        /* Asking for more than the seventh group holds is reported, and
         * the ones that fit still work. */
        ap->team_count = ISSD_MAX_ADDED_TEAMS + 1;
        for (int i = 1; i < ap->team_count; i++) {
            ap->teams[i] = ap->teams[0];
            ap->teams[i].assigned_slot = -1;
        }
        memset(rom + ROSTERS, 0xEE, ISSD_MAX_ADDED_TEAMS * 160);
        rom[GATE_A] = 0x48; rom[GATE_A + 1] = 0x00;
        rom[GATE_B] = 0x48; rom[GATE_B + 1] = 0x00;
        issd_mod_result_reset();
        issd_mod_apply_to_rom(rom, sizeof rom);
        assert(issd_mod_last_result()->warnings > 0);
        assert(ap->teams[ISSD_MAX_ADDED_TEAMS - 1].assigned_slot ==
               36 + ISSD_MAX_ADDED_TEAMS - 1);
        assert(ap->teams[ISSD_MAX_ADDED_TEAMS].assigned_slot < 0 &&
               "the one that does not fit gets no slot");
        assert((uint16_t)(rom[GATE_A] | (rom[GATE_A + 1] << 8)) ==
               (36 + ISSD_MAX_ADDED_TEAMS) * 2);
    }

    puts("mod rom tests passed");
    return 0;
}
