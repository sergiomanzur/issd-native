/* Applies loaded mod packs to the cartridge image.
 *
 * Rosters live in ROM, not RAM: the recompiled code is baked into C, but the
 * name and attribute tables are read from the ROM image at runtime. So a mod
 * is applied by patching that image once, after it is read and before the
 * engine starts. The previous implementation wrote nothing at all - it parsed
 * the JSON, printed "Injected custom team" and returned true - which is why
 * mods loaded cleanly and changed nothing.
 *
 * Offsets and encodings come from deps/ISSD-web-editor (MIT), a working ROM
 * editor for this exact cartridge, cross-checked against the ROM itself.
 */
#include "issd_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Layout of the roster tables: squads of 20 players, each with an 8 byte
 * name and 7 bytes of attributes, both flat arrays indexed by
 * team * 20 + player. */
#define ROM_TEAMS             36
#define ROM_PLAYERS_PER_TEAM  20
#define ROM_NAME_BYTES         8
#define ROM_ATTR_BYTES         7
/* The editor overstates both table bases by exactly 512 bytes. Walking the
 * name table back from its value reaches real names 64 slots earlier, and
 * from the true start it runs exactly 720 slots - 36 squads of 20, which is
 * the count the editor assumed but could not reach from its own offset.
 * Reading from the wrong base still lands inside the table, so it silently
 * edits the wrong team rather than failing.
 *
 * The attribute base is confirmed the same 512 lower: at 0x50000 squad slot 0
 * carries a distinct position value for all 36 teams - the goalkeeper - which
 * the editor offset does not show. */
#define ROM_NAME_BASE     229774u
#define ROM_ATTR_BASE     327680u

/* The cartridge does not use ASCII. 0x00 renders as a space and doubles as
 * padding; letters run from 0x68. Generated from the editor's dictionary. */
static const struct { uint8_t code; char ch; } kCharset[] = {
    { 0x00, '.' }, { 0x68, 'A' }, { 0x69, 'B' }, { 0x6A, 'C' }, { 0x6B, 'D' }, { 0x6C, 'E' },
    { 0x6D, 'F' }, { 0x6E, 'G' }, { 0x6F, 'H' }, { 0x70, 'I' }, { 0x71, 'J' }, { 0x72, 'K' },
    { 0x73, 'L' }, { 0x74, 'M' }, { 0x75, 'N' }, { 0x76, 'O' }, { 0x77, 'P' }, { 0x78, 'Q' },
    { 0x79, 'R' }, { 0x7A, 'S' }, { 0x7B, 'T' }, { 0x7C, 'U' }, { 0x7D, 'V' }, { 0x7E, 'W' },
    { 0x7F, 'X' }, { 0x80, 'Y' }, { 0x81, 'Z' }, { 0x82, 'a' }, { 0x83, 'b' }, { 0x84, 'c' },
    { 0x85, 'd' }, { 0x86, 'e' }, { 0x87, 'f' }, { 0x88, 'g' }, { 0x89, 'h' }, { 0x8A, 'i' },
    { 0x8B, 'j' }, { 0x8C, 'k' }, { 0x8D, 'l' }, { 0x8E, 'm' }, { 0x8F, 'n' }, { 0x90, 'o' },
    { 0x91, 'p' }, { 0x92, 'q' }, { 0x93, 'r' }, { 0x94, 's' }, { 0x95, 't' }, { 0x96, 'u' },
    { 0x97, 'v' }, { 0x98, 'w' }, { 0x99, 'x' }, { 0x9A, 'y' }, { 0x9B, 'z' },
};

static uint8_t encode_char(char c) {
    if (c == ' ') return 0x00;                 /* padding renders as a space */
    for (unsigned i = 0; i < sizeof(kCharset) / sizeof(kCharset[0]); i++)
        if (kCharset[i].ch == c) return kCharset[i].code;
    return 0x00;                               /* unrepresentable -> blank */
}

/* Attributes are 4 bit fields. The cartridge stores 0..15 and the game shows
 * 1..16, so there are sixteen levels, not a hundred. Mod files are authored on
 * the familiar 0..99 scale, so they are quantised here rather than silently
 * truncated - a 99 and an 80 must not collapse to the same nibble by accident,
 * and a value of 0 must not wrap to the maximum. */
static uint8_t rating_to_nibble(uint8_t rating) {
    if (rating > 99) rating = 99;
    unsigned n = ((unsigned)rating * 15u + 49u) / 99u;   /* 0..99 -> 0..15 */
    return (uint8_t)(n > 15 ? 15 : n);
}

static void patch_name(uint8_t *rom, size_t base, const char *name) {
    for (int i = 0; i < ROM_NAME_BYTES; i++) {
        char c = name[i];
        if (c == '\0') {                       /* pad the rest of the field */
            for (; i < ROM_NAME_BYTES; i++) rom[base + i] = 0x00;
            return;
        }
        rom[base + i] = encode_char(c);
    }
}

/* Byte 5 is an attribute-block index the game uses for something we have not
 * characterised, so it is read-modify-write: only the fields a mod actually
 * describes are touched. */
static void patch_attributes(uint8_t *rom, size_t base, const IssdModPlayer *p) {
    const IssdPlayerAttributes *a = &p->attributes;
    rom[base + 0] = (uint8_t)((rating_to_nibble(a->acceleration) << 4) |
                               rating_to_nibble(a->speed));
    rom[base + 1] = (uint8_t)((rating_to_nibble(a->shooting)     << 4) |
                               rating_to_nibble(a->technique));
    rom[base + 2] = (uint8_t)((rating_to_nibble(a->balance)      << 4) |
                               rating_to_nibble(a->intelligence));
    rom[base + 3] = (uint8_t)((rating_to_nibble(a->dribbling)    << 4) |
                               rating_to_nibble(a->jumping));
    /* High nibble is a positional rating, low nibble is stamina. */
    rom[base + 4] = (uint8_t)((rom[base + 4] & 0xF0) |
                               rating_to_nibble(a->stamina));
    /* rom[base + 5] deliberately preserved. */
    rom[base + 6] = (uint8_t)(((p->skin_tone & 0x0F) << 4) |
                               (p->hair_style & 0x0F));
}

/* Switching packs at runtime has to start from the untouched cartridge:
 * patches are destructive, so applying a second pack over the first would
 * leave whichever fields the second does not mention still holding the
 * first one's values. A pristine copy is kept for that. */
static uint8_t *s_live;
static uint8_t *s_pristine;
static size_t   s_size;

void issd_mod_rom_set_image(uint8_t *rom, size_t rom_size) {
  s_live = rom; s_size = rom_size;
  free(s_pristine);
  s_pristine = (uint8_t *)malloc(rom_size);
  if (s_pristine) memcpy(s_pristine, rom, rom_size);
}

int issd_mod_reapply(void) {
  if (!s_live || !s_pristine) return 0;
  memcpy(s_live, s_pristine, s_size);      /* back to vanilla first */
  return issd_mod_apply_to_rom(s_live, s_size);
}

int issd_mod_apply_to_rom(uint8_t *rom, size_t rom_size) {
    if (!rom) return 0;

    const size_t name_end = ROM_NAME_BASE +
        (size_t)ROM_TEAMS * ROM_PLAYERS_PER_TEAM * ROM_NAME_BYTES;
    const size_t attr_end = ROM_ATTR_BASE +
        (size_t)ROM_TEAMS * ROM_PLAYERS_PER_TEAM * ROM_ATTR_BYTES;
    if (rom_size < name_end || rom_size < attr_end) {
        fprintf(stderr,
                "[ModLoader] ROM is %zu bytes, too small to hold the roster "
                "tables (need %zu). Not patching.\n",
                rom_size, attr_end > name_end ? attr_end : name_end);
        return 0;
    }

    int players_patched = 0, teams_patched = 0;

    for (int pi = 0; pi < issd_mod_get_pack_count(); pi++) {
        IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack || !pack->is_active) continue;

        for (int ti = 0; ti < pack->team_count; ti++) {
            const IssdModTeam *team = &pack->teams[ti];
            if (team->team_id >= ROM_TEAMS) {
                fprintf(stderr,
                        "[ModLoader] '%s': team_id %u is out of range. The "
                        "cartridge indexes a fixed table of %d teams, so a "
                        "team can be replaced but not added.\n",
                        pack->name, team->team_id, ROM_TEAMS);
                continue;
            }

            int n = team->player_count;
            if (n > ROM_PLAYERS_PER_TEAM) {
                fprintf(stderr,
                        "[ModLoader] '%s': team %u lists %d players; only the "
                        "first %d fit the cartridge's squad slots.\n",
                        pack->name, team->team_id, n, ROM_PLAYERS_PER_TEAM);
                n = ROM_PLAYERS_PER_TEAM;
            }

            const size_t slot = (size_t)team->team_id * ROM_PLAYERS_PER_TEAM;
            for (int p = 0; p < n; p++) {
                patch_name(rom, ROM_NAME_BASE + (slot + p) * ROM_NAME_BYTES,
                           team->players[p].name);
                patch_attributes(rom, ROM_ATTR_BASE + (slot + p) * ROM_ATTR_BYTES,
                                 &team->players[p]);
                players_patched++;
            }
            teams_patched++;
            printf("[ModLoader] Patched team %u '%s' (%d players)\n",
                   team->team_id, team->name, n);
        }
    }

    if (players_patched)
        printf("[ModLoader] Applied %d player(s) across %d team(s) to the "
               "cartridge image.\n", players_patched, teams_patched);
    return players_patched;
}
