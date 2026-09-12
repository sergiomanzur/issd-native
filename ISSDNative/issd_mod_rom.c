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
#include "issd_formation.h"
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

/* Team shapes live apart from the roster, behind a pointer table.
 *
 * $8B:EF48 holds thirty-six 16-bit pointers, one per team, into bank $8B;
 * each names a 31-byte record holding the label the team select screen
 * prints, ten home positions, and ten role bytes. Found by recording every
 * cartridge offset a run touches and diffing runs that differed only by
 * which team was highlighted - two teams three apart in the roster read two
 * pointers six bytes apart, which is what gave the table away.
 *
 * The records themselves are packed end to end from $8B:EF9E, but nothing
 * requires that, so the pointer is always followed rather than computed. */
#define ROM_FORMATION_PTRS          0x05EF48u
#define ROM_FORMATION_BANK          0x0Bu
#define ROM_FORMATION_RECORD_BYTES  31u

/* Stadiums.
 *
 * Names are eight fixed 7-byte fields, right-aligned with 0x00 padding -
 * "  JAPAN", "ENGLAND" - running from 0x3CA9B to exactly where the word
 * STADIUM begins, which is why a ninth will not fit here.
 *
 * Pitch dimensions are eight (length, width) byte pairs. Found by
 * searching for the lengths the select screen prints - 114, 118, 126, 130,
 * 122, 122, 114, 138 - which occur in that order exactly once in the
 * cartridge. The code that reads them is at $82:FADD, reached by
 * LDA $82FADD,X with X twice the stadium index.
 *
 * They are what the screens print, and nothing else. Running the same
 * match on a 138x90 pitch and a 115x74 one, same inputs, leaves WRAM
 * byte-identical 1400 frames into play: the playfield does not change
 * size. Worth stating plainly, because the obvious reading of a field
 * called pitch_length is that it moves the touchlines.
 *
 * The count is a literal: LDA $4C / CMP #$0008 near $A2:9F6A. Raising it
 * would need every one of these tables extended, and they are packed
 * against their neighbours, so a stadium is replaced rather than added -
 * the same deal the 36 teams offer. */
#define ROM_STADIUMS            8
#define ROM_STADIUM_NAME_BASE   0x3CA9Bu
#define ROM_STADIUM_NAME_BYTES  7
#define ROM_STADIUM_PITCH_BASE  0x017AEDu

/* The range the cartridge's own stadiums span. The select screen draws a
 * preview from these, and it has not been looked at outside that range, so
 * a pack asking for a 40 yard pitch is clamped rather than trusted. */
#define ROM_PITCH_MIN_LENGTH   100
#define ROM_PITCH_MAX_LENGTH   140
#define ROM_PITCH_MIN_WIDTH     64
#define ROM_PITCH_MAX_WIDTH     96

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

/* The stadium plate has a full stop, which the roster font does not use:
 * U.S.A is stored with 0x54 between the letters. */
#define CHAR_PERIOD 0x54u

static uint8_t encode_char(char c) {
    if (c == ' ') return 0x00;                 /* padding renders as a space */
    for (unsigned i = 0; i < sizeof(kCharset) / sizeof(kCharset[0]); i++)
        if (kCharset[i].ch == c) return kCharset[i].code;
    return 0x00;                               /* unrepresentable -> blank */
}

/* Attributes are 4 bit fields, but the cartridge does not use all sixteen
 * levels. Measured across its own 720 players, every attribute sits between
 * nibble 2 and nibble 9, averaging about 6 - acceleration, for instance,
 * runs 2..9 with most players on 5, 6 or 7.
 *
 * Mapping an authored 0..99 straight onto 0..15 therefore put a normal
 * football rating of 70 to 90 at nibble 11 to 14: above anything the game
 * ships, and squeezed into so few steps that a whole squad came out
 * effectively identical. Mapping onto the range actually in use keeps modded
 * players comparable with stock ones and spreads a squad across real steps.
 *
 * 0 stays at the floor rather than wrapping, and 99 reaches the same ceiling
 * the cartridge's best players do rather than an unreachable one. */
#define RATING_NIBBLE_MIN 2u
#define RATING_NIBBLE_MAX 9u

static uint8_t rating_to_nibble(uint8_t rating) {
    if (rating > 99) rating = 99;
    const unsigned span = RATING_NIBBLE_MAX - RATING_NIBBLE_MIN;   /* 7 steps */
    unsigned n = RATING_NIBBLE_MIN + ((unsigned)rating * span + 49u) / 99u;
    return (uint8_t)(n > RATING_NIBBLE_MAX ? RATING_NIBBLE_MAX : n);
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

/* Position codes in the high nibble of byte 4. Read off the cartridge: slot 0
 * and slot 11 carry 1 across all 36 teams - the two goalkeepers - and the
 * values then climb through the outfield in squad order. 3 and 5 occur as
 * in-between ratings, so a mod picks the four it can name and the game still
 * reads the rest of the table unchanged. */
static uint8_t position_code(const char *pos) {
    if (!pos) return 4;
    if (pos[0] == 'G') return 1;   /* GK */
    if (pos[0] == 'D') return 2;   /* DF */
    if (pos[0] == 'F') return 6;   /* FW */
    return 4;                      /* MF, and anything unrecognised */
}

/* Byte 5 is the player's own slot index within the squad: it runs 0..19 and
 * each value appears exactly 36 times, once per team. It belongs to the
 * slot rather than to the player, so it is never rewritten. */
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
    /* High nibble is the position, low nibble stamina. The position was
     * previously preserved rather than written, so a pack could name a
     * player a goalkeeper and the game would still field him wherever the
     * original squad had that slot. */
    rom[base + 4] = (uint8_t)((position_code(p->position) << 4) |
                               rating_to_nibble(a->stamina));
    /* rom[base + 5] deliberately preserved. */
    rom[base + 6] = (uint8_t)(((p->skin_tone & 0x0F) << 4) |
                               (p->hair_style & 0x0F));
}

/* ------------------------------------------------- more stadiums ----- */

/* The eight are eight because four tables are packed against their
 * neighbours and one 16-bit literal says so. None of that is load-bearing:
 * each table moves to free space in its own bank, the single instruction
 * that indexes it is re-pointed, and the literal is raised.
 *
 *   $82:FADD  turf pattern    LDA $82FADD,X   operand at 0x121FA0
 *   $82:FAED  pitch length    LDA $82FAED,X   operand at 0x12200A
 *   $82:FAEE  pitch width     LDA $82FAEE,X   operand at 0x12201D
 *   $82:FAFD  unidentified    LDA $82FAFD,X   operand at 0x12203F
 *   $87:CA9B  names           ADC #$CA9B      operand at 0x03448B
 *   count                     CMP #$0008      operand at 0x121F6D
 *
 * The name reader never appears in the recompiled C because it runs
 * interpreted. It was found by logging the interpreter's PC whenever
 * anything read the name table: $86:C494, where the code multiplies the
 * stadium number by seven and adds the base.
 *
 * Every site is checked against the bytes it is expected to hold before
 * anything is written, so a different cartridge revision is refused rather
 * than corrupted. */
#define ROM_STADIUM_COUNT_OPERAND  0x121F6Du
#define ROM_STADIUM_NAME_OPERAND   0x03448Bu
#define ROM_FREE_BANK82            0x017B5Du   /* $82:FB5D, 1187 bytes */
#define ROM_FREE_BANK87            0x03FAC8u   /* $87:FAC8, 1336 bytes */

typedef struct { size_t operand; uint16_t from; } StadiumRef;

static const StadiumRef kStadiumWordRefs[] = {
    { 0x121FA0u, 0xFADDu },   /* turf */
    { 0x12200Au, 0xFAEDu },   /* length */
    { 0x12201Du, 0xFAEEu },   /* width, the same table one byte on */
    { 0x12203Fu, 0xFAFDu },   /* unidentified, but per stadium */
};

/* Where the tables live now. Stock until a pack asks for more. */
static unsigned s_stadium_slots = ROM_STADIUMS;
static size_t   s_stadium_name_base = ROM_STADIUM_NAME_BASE;
static size_t   s_stadium_pitch_base = ROM_STADIUM_PITCH_BASE;

static bool rom_is_free(const uint8_t *rom, size_t at, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (rom[at + i] != 0xFF) return false;
    return true;
}

static bool expand_stadiums(uint8_t *rom, size_t rom_size, unsigned slots) {
    s_stadium_slots = ROM_STADIUMS;
    s_stadium_name_base = ROM_STADIUM_NAME_BASE;
    s_stadium_pitch_base = ROM_STADIUM_PITCH_BASE;
    if (slots <= ROM_STADIUMS) return false;
    if (slots > ISSD_MAX_STADIUMS) slots = ISSD_MAX_STADIUMS;

    const size_t need82 = (size_t)slots * 6u;      /* three word tables */
    const size_t need87 = (size_t)slots * ROM_STADIUM_NAME_BYTES;
    if (rom_size < ROM_FREE_BANK87 + need87 ||
        !rom_is_free(rom, ROM_FREE_BANK82, need82) ||
        !rom_is_free(rom, ROM_FREE_BANK87, need87)) {
        issd_mod_result_note_error("no free space for extra stadiums");
        fprintf(stderr, "[ModLoader] The space these tables move into is not"
                        " free in this cartridge. Leaving 8 stadiums.\n");
        return false;
    }

    /* Refuse rather than corrupt if this is not the cartridge we measured. */
    if (rom[ROM_STADIUM_COUNT_OPERAND - 1] != 0xC9 ||
        rom[ROM_STADIUM_COUNT_OPERAND] != ROM_STADIUMS ||
        rom[ROM_STADIUM_NAME_OPERAND - 1] != 0x69) {
        issd_mod_result_note_error("cartridge does not match; stadiums kept");
        fprintf(stderr, "[ModLoader] The stadium code is not where it is "
                        "expected in this cartridge. Leaving 8 stadiums.\n");
        return false;
    }
    for (unsigned i = 0; i < sizeof kStadiumWordRefs / sizeof kStadiumWordRefs[0]; i++) {
        const StadiumRef *r = &kStadiumWordRefs[i];
        const uint16_t have = (uint16_t)(rom[r->operand] | (rom[r->operand + 1] << 8));
        if (rom[r->operand - 1] != 0xBF || have != r->from) {
            issd_mod_result_note_error("cartridge does not match; stadiums kept");
            fprintf(stderr, "[ModLoader] Stadium table reference %u is not "
                            "where it is expected. Leaving 8 stadiums.\n", i);
            return false;
        }
    }

    /* Copy each table to its new home, repeating the cartridge's own entries
     * so a slot nobody has customised is still a working stadium. */
    size_t cursor = ROM_FREE_BANK82;
    size_t pitch_dst = 0;
    for (unsigned i = 0; i < sizeof kStadiumWordRefs / sizeof kStadiumWordRefs[0]; i++) {
        const StadiumRef *r = &kStadiumWordRefs[i];
        size_t dst;
        if (r->from == 0xFAEEu) {
            dst = pitch_dst + 1;          /* width shares the pitch table */
        } else {
            const size_t src = (size_t)0x10000u + (r->from - 0x8000u);
            dst = cursor;
            for (unsigned k = 0; k < slots; k++) {
                const size_t e = src + (size_t)(k % ROM_STADIUMS) * 2u;
                rom[dst + k * 2] = rom[e];
                rom[dst + k * 2 + 1] = rom[e + 1];
            }
            cursor += (size_t)slots * 2u;
            if (r->from == 0xFAEDu) pitch_dst = dst;
        }
        const uint16_t addr = (uint16_t)(0x8000u + (dst - 0x10000u));
        rom[r->operand] = (uint8_t)(addr & 0xFF);
        rom[r->operand + 1] = (uint8_t)(addr >> 8);
    }

    for (unsigned k = 0; k < slots; k++) {
        const size_t src = ROM_STADIUM_NAME_BASE +
            (size_t)(k % ROM_STADIUMS) * ROM_STADIUM_NAME_BYTES;
        const size_t dst = ROM_FREE_BANK87 + (size_t)k * ROM_STADIUM_NAME_BYTES;
        memcpy(rom + dst, rom + src, ROM_STADIUM_NAME_BYTES);
    }
    const uint16_t name_addr = (uint16_t)(0x8000u + (ROM_FREE_BANK87 - 0x38000u));
    rom[ROM_STADIUM_NAME_OPERAND] = (uint8_t)(name_addr & 0xFF);
    rom[ROM_STADIUM_NAME_OPERAND + 1] = (uint8_t)(name_addr >> 8);

    rom[ROM_STADIUM_COUNT_OPERAND] = (uint8_t)slots;

    s_stadium_slots = slots;
    s_stadium_name_base = ROM_FREE_BANK87;
    s_stadium_pitch_base = pitch_dst;
    printf("[ModLoader] Stadiums expanded from %d to %u.\n", ROM_STADIUMS, slots);
    return true;
}

/* Write one stadium. Returns true when anything changed. */
static bool patch_stadium(uint8_t *rom, size_t rom_size, const char *pack_name,
                          const IssdModStadium *st) {
    if (st->stadium_id < 0) {
        char why[96];
        snprintf(why, sizeof why, "%s: a stadium has no stadium_id", pack_name);
        issd_mod_result_note_warning(why);
        fprintf(stderr, "[ModLoader] %s\n", why);
        return false;
    }
    if ((unsigned)st->stadium_id >= s_stadium_slots) {
        char why[96];
        snprintf(why, sizeof why, "%s: stadium %d does not exist",
                 pack_name, st->stadium_id);
        issd_mod_result_note_warning(why);
        fprintf(stderr,
                "[ModLoader] '%s': stadium_id %d is out of range; there "
                "are %u stadiums. Raise \"stadium_count\" to make more.\n",
                pack_name, st->stadium_id, s_stadium_slots);
        return false;
    }

    const size_t name = s_stadium_name_base +
                        (size_t)st->stadium_id * ROM_STADIUM_NAME_BYTES;
    const size_t pitch = s_stadium_pitch_base + (size_t)st->stadium_id * 2u;
    if (name + ROM_STADIUM_NAME_BYTES > rom_size || pitch + 2 > rom_size)
        return false;

    bool changed = false;
    if (st->name[0]) {
        /* Right-aligned with leading blanks, the way the cartridge stores
         * its own: the screen centres the field, so a left-aligned name
         * sits off to one side. */
        uint8_t enc[ROM_STADIUM_NAME_BYTES];
        int n = 0;
        for (const char *c = st->name; *c && n < ROM_STADIUM_NAME_BYTES; c++)
            enc[n++] = (*c == '.') ? (uint8_t)CHAR_PERIOD : encode_char(*c);
        const int pad = ROM_STADIUM_NAME_BYTES - n;
        for (int i = 0; i < pad; i++) rom[name + i] = 0x00;
        for (int i = 0; i < n; i++) rom[name + pad + i] = enc[i];
        changed = true;
        if ((int)strlen(st->name) > ROM_STADIUM_NAME_BYTES) {
            char why[96];
            snprintf(why, sizeof why, "%s: stadium name cut to %d characters",
                     pack_name, ROM_STADIUM_NAME_BYTES);
            issd_mod_result_note_warning(why);
        }
    }

    if (st->pitch_length) {
        uint8_t v = st->pitch_length;
        if (v < ROM_PITCH_MIN_LENGTH) v = ROM_PITCH_MIN_LENGTH;
        if (v > ROM_PITCH_MAX_LENGTH) v = ROM_PITCH_MAX_LENGTH;
        rom[pitch + 0] = v;
        changed = true;
    }
    if (st->pitch_width) {
        uint8_t v = st->pitch_width;
        if (v < ROM_PITCH_MIN_WIDTH) v = ROM_PITCH_MIN_WIDTH;
        if (v > ROM_PITCH_MAX_WIDTH) v = ROM_PITCH_MAX_WIDTH;
        rom[pitch + 1] = v;
        changed = true;
    }

    if (changed)
        printf("[ModLoader] Stadium %d: '%s' %dx%d yards\n", st->stadium_id,
               st->name[0] ? st->name : "(name kept)",
               rom[pitch + 0], rom[pitch + 1]);
    return changed;
}

/* LoROM: bank $80+n covers file offset n * $8000, mapped at $8000-$FFFF. */
static size_t formation_record_offset(const uint8_t *rom, uint8_t team_id) {
    const size_t entry = ROM_FORMATION_PTRS + (size_t)team_id * 2u;
    const unsigned ptr = (unsigned)rom[entry] | ((unsigned)rom[entry + 1] << 8);
    if (ptr < 0x8000u) return 0;       /* not a bank address: table is wrong */
    return (size_t)ROM_FORMATION_BANK * 0x8000u + (ptr - 0x8000u);
}

static void warn_unknown_formation(const char *pack, const char *name) {
    char why[96];
    snprintf(why, sizeof why, "%s: unknown formation %s", pack, name);
    issd_mod_result_note_warning(why);
    fprintf(stderr, "[ModLoader] '%s': unknown formation \"%s\". Known: ",
            pack, name);
    for (int i = 0; i < issd_formation_count(); i++)
        fprintf(stderr, "%s\"%s\"", i ? ", " : "", issd_formation_at(i)->name);
    fprintf(stderr, ".\n");
}

/* The label only names the shape; the positions and roles are the shape. A
 * pack that lists its players' positions differently from the formation it
 * asked for gets a warning rather than a silent correction, because which of
 * the two is wrong is the author's call. */
static void warn_position_mismatch(const IssdModTeam *team,
                                   const IssdFormation *f, int n) {
    int want[3] = {0, 0, 0};   /* defenders, midfielders, forwards */
    for (int i = 0; i < ISSD_FORMATION_SLOTS; i++) {
        uint8_t r = f->slots[i].role;
        if (r == ISSD_ROLE_DEFENDER || r == ISSD_ROLE_DEFENDER_ATTACK) want[0]++;
        else if (r == ISSD_ROLE_FORWARD) want[2]++;
        else want[1]++;
    }
    int have[3] = {0, 0, 0};
    const int outfield = n < 11 ? n : 11;
    for (int p = 1; p < outfield; p++) {   /* slot 0 is the keeper */
        char c = team->players[p].position[0];
        if (c == 'D') have[0]++;
        else if (c == 'F') have[2]++;
        else have[1]++;
    }
    if (outfield == 11 &&
        (have[0] != want[0] || have[1] != want[1] || have[2] != want[2]))
        fprintf(stderr,
                "[ModLoader] team %u '%s': formation \"%s\" lines up %d-%d-%d "
                "but the first eleven are listed %d-%d-%d. The pitch follows "
                "the formation; the squad list follows the positions.\n",
                team->team_id, team->name, f->name,
                want[0], want[1], want[2], have[0], have[1], have[2]);
}

/* Write a team's shape. Returns true when the record was rewritten. */
static bool patch_formation(uint8_t *rom, size_t rom_size,
                            const char *pack_name, const IssdModTeam *team,
                            int player_count) {
    if (!team->formation[0]) return false;

    const IssdFormation *base = issd_formation_find(team->formation);
    if (!base) { warn_unknown_formation(pack_name, team->formation); return false; }

    IssdFormation f;
    if (!issd_formation_apply_tactics(&f, base, team->tactics))
        fprintf(stderr,
                "[ModLoader] '%s': unknown tactics \"%s\" (attacking, balanced "
                "or defensive). Using the formation as authored.\n",
                pack_name, team->tactics);

    const size_t off = formation_record_offset(rom, team->team_id);
    if (!off || off + ROM_FORMATION_RECORD_BYTES > rom_size) {
        fprintf(stderr,
                "[ModLoader] '%s': team %u's formation record is outside the "
                "cartridge image. Leaving its shape alone.\n",
                pack_name, team->team_id);
        return false;
    }

    rom[off] = f.label;
    for (int i = 0; i < ISSD_FORMATION_SLOTS; i++) {
        rom[off + 1 + (size_t)i * 2] = (uint8_t)f.slots[i].depth;
        rom[off + 2 + (size_t)i * 2] = (uint8_t)f.slots[i].width;
        rom[off + 21 + (size_t)i]    = f.slots[i].role;
    }

    warn_position_mismatch(team, &f, player_count);
    printf("[ModLoader]   shape \"%s\"%s%s, shown as %s\n", f.name,
           team->tactics[0] ? " / " : "", team->tactics,
           issd_formation_label_text(f.label));
    return true;
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
  /* Counts describe the stack as it is now, not as it was before the
   * player changed it, so they start from zero every time. */
  issd_mod_result_reset();
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

    int players_patched = 0, teams_patched = 0, formations_patched = 0;
    int stadiums_patched = 0;

    /* Expansion rewrites code and moves tables, so it happens once, before
     * any pack writes a stadium. The largest ask across the stack wins:
     * packs that only replace a stadium do not care how many there are. */
    {
        unsigned want = ROM_STADIUMS;
        for (int slot = 0; ; slot++) {
            const int pi = issd_mod_pack_at_order(slot);
            if (pi < 0) break;
            const IssdModPack *p = issd_mod_get_pack(pi);
            if (p && (unsigned)p->stadium_slots > want)
                want = (unsigned)p->stadium_slots;
        }
        expand_stadiums(rom, rom_size, want);
    }

    /* Which pack last wrote each team, so an overlap can be named rather
     * than silently resolved. Stacking two packs that both rewrite Mexico
     * is legal - the later one wins - but it is almost never intended. */
    const char *owner[ROM_TEAMS];
    memset(owner, 0, sizeof owner);

    /* In stack order, not scan order: the pack enabled last wins. */
    for (int slot = 0; ; slot++) {
        const int pi = issd_mod_pack_at_order(slot);
        if (pi < 0) break;
        IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack) continue;
        issd_mod_result_mutable()->packs_applied++;

        for (int ti = 0; ti < pack->team_count; ti++) {
            const IssdModTeam *team = &pack->teams[ti];
            if (team->team_id >= ROM_TEAMS) {
                char why[96];
                snprintf(why, sizeof why, "%s: team %u does not exist",
                         pack->name, team->team_id);
                issd_mod_result_note_warning(why);
                fprintf(stderr,
                        "[ModLoader] '%s': team_id %u is out of range. The "
                        "cartridge indexes a fixed table of %d teams, so a "
                        "team can be replaced but not added.\n",
                        pack->name, team->team_id, ROM_TEAMS);
                continue;
            }
            if (owner[team->team_id]) {
                char why[96];
                snprintf(why, sizeof why, "%s overrides %s on team %u",
                         pack->name, owner[team->team_id], team->team_id);
                issd_mod_result_note_warning(why);
                fprintf(stderr, "[ModLoader] %s.\n", why);
            }
            owner[team->team_id] = pack->name;

            int n = team->player_count;
            if (n > ROM_PLAYERS_PER_TEAM) {
                fprintf(stderr,
                        "[ModLoader] '%s': team %u lists %d players; only the "
                        "first %d fit the cartridge's squad slots.\n",
                        pack->name, team->team_id, n, ROM_PLAYERS_PER_TEAM);
                issd_mod_result_note_warning("squad longer than 20");
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
            /* A team entry may carry no players at all: "leave this squad
             * alone, just change how it lines up" is a reasonable thing for a
             * pack to say, and reporting it as zero players patched reads as
             * a failure when nothing failed. */
            if (n > 0)
                printf("[ModLoader] Patched team %u '%s' (%d players)\n",
                       team->team_id, team->name, n);
            else
                printf("[ModLoader] Team %u '%s': squad left as it is\n",
                       team->team_id, team->name);
            if (patch_formation(rom, rom_size, pack->name, team, n))
                formations_patched++;
        }

        for (int si = 0; si < pack->stadium_count; si++)
            if (patch_stadium(rom, rom_size, pack->name, &pack->stadiums[si]))
                stadiums_patched++;
    }

    if (players_patched)
        printf("[ModLoader] Applied %d player(s) across %d team(s) to the "
               "cartridge image.\n", players_patched, teams_patched);
    if (formations_patched)
        printf("[ModLoader] Reshaped %d team(s).\n", formations_patched);

    if (stadiums_patched)
        printf("[ModLoader] Rebuilt %d stadium(s).\n", stadiums_patched);

    IssdModResult *r = issd_mod_result_mutable();
    r->stadiums_patched = stadiums_patched;
    r->teams_patched = teams_patched;
    r->players_patched = players_patched;
    r->formations_patched = formations_patched;
    return players_patched;
}
