/* The formation library a mod pack draws on.
 *
 * The cartridge's own eight shapes are all flat three-line formations, because
 * that is what 1995 looked like. Nothing in the data format requires that: the
 * ten home positions are free signed offsets, so a band of holding midfielders
 * sitting behind a band of attacking ones - a 4-2-3-1 - is expressible, and so
 * is everything else that came after. What is fixed is the printed label,
 * which can only be one of sixteen three-number strings, so a 4-2-3-1 is
 * labelled 4-5-1 the way a newspaper would have labelled it in 1995.
 *
 * The numbers below are in the cartridge's own units, taken from the range its
 * own teams use: widths run to about +/-40, depths to about +/-20.
 */
#include "issd_formation.h"

#include <ctype.h>
#include <string.h>

#define DF  ISSD_ROLE_DEFENDER
#define DFA ISSD_ROLE_DEFENDER_ATTACK
#define MF  ISSD_ROLE_MIDFIELDER
#define MFA ISSD_ROLE_MIDFIELDER_ATTACK
#define FW  ISSD_ROLE_FORWARD

/* Label indices, each read off the team select screen by writing the value
 * into a team's record and screenshotting the result. The cartridge groups
 * them by defender count, and the count in the label always matches the
 * number of slots carrying each role, so a formation and its label must
 * agree or the screen lies about the shape. */
static const char *const kLabelText[16] = {
    "4-5-1", "4-4-2", "4-3-3", "4-2-4",
    "3-5-2", "3-4-3", "3-3-4", "3-2-5",
    "2-5-3", "2-4-4", "2-3-5", "5-4-1",
    "5-3-2", "5-2-3", "1-5-4", "1-4-5"
};

#define LBL_451  0
#define LBL_442  1
#define LBL_433  2
#define LBL_424  3
#define LBL_352  4
#define LBL_343  5
#define LBL_334  6
#define LBL_235 10
#define LBL_541 11
#define LBL_532 12

static const IssdFormation kFormations[] = {
    { "4-4-2", "Flat back four, flat midfield four, two strikers.", LBL_442, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        {  0, -32, MF }, {-12, -11, MF }, {-12,  11, MF }, {  0,  32, MF },
        {  0, -16, FW }, {  0,  16, FW } } },

    { "4-4-2 diamond", "Back four, a holder, two wide, one behind the strikers.",
      LBL_442, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        { 12,   0, MF }, { -4, -28, MFA}, { -4,  28, MFA}, {-16,   0, MFA},
        {  0, -14, FW }, {  0,  14, FW } } },

    { "4-3-3", "Back four, a midfield three, a front three.", LBL_433, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        {  6,   0, MF }, { -8, -16, MF }, { -8,  16, MF },
        { -4, -34, FW }, {-10,   0, FW }, { -4,  34, FW } } },

    { "4-2-3-1", "Two holding midfielders, three behind a lone striker.",
      LBL_451, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        { 10, -10, MF }, { 10,  10, MF },
        { -8, -30, MFA}, { -8,   0, MFA}, { -8,  30, MFA},
        {-20,   0, FW } } },

    { "4-1-4-1", "An anchor behind a midfield four, one striker.", LBL_451, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        { 12,   0, MF },
        { -6, -32, MFA}, { -6, -10, MF }, { -6,  10, MF }, { -6,  32, MFA},
        {-18,   0, FW } } },

    { "4-5-1", "Back four, a flat five across midfield, one striker.", LBL_451, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        { -4, -34, MFA}, {  0, -14, MF }, {  2,   0, MF }, {  0,  14, MF },
        { -4,  34, MFA},
        {-14,   0, FW } } },

    { "3-5-2", "Three at the back, wing backs, two strikers.", LBL_352, {
        {  2, -20, DF }, { -8,   0, DF }, {  2,  20, DF },
        { -4, -40, MFA}, {  4, -12, MF }, {  8,   0, MF }, {  4,  12, MF },
        { -4,  40, MFA},
        {  0, -16, FW }, {  0,  16, FW } } },

    { "3-4-3", "Three at the back, four across, a front three.", LBL_343, {
        {  0, -20, DF }, { -4,   0, DF }, {  0,  20, DF },
        { -8, -32, MFA}, {  0, -11, MF }, {  0,  11, MF }, { -8,  32, MFA},
        {-10, -26, FW }, {-14,   0, FW }, {-10,  26, FW } } },

    { "3-4-2-1", "Three at the back, two shadow strikers behind a lone man.",
      LBL_343, {
        {  0, -20, DF }, { -4,   0, DF }, {  0,  20, DF },
        { -6, -34, MFA}, {  2, -11, MF }, {  2,  11, MF }, { -6,  34, MFA},
        {-10, -18, FW }, {-10,  18, FW }, {-22,   0, FW } } },

    { "5-3-2", "Five at the back, three in the middle, two up.", LBL_532, {
        {  0, -32, DFA}, {  0, -11, DF }, { -6,   0, DF }, {  0,  11, DF },
        {  0,  32, DFA},
        {  2, -24, MF }, { -2,   0, MF }, {  2,  24, MF },
        { -8, -16, FW }, { -8,  16, FW } } },

    { "5-4-1", "Five at the back, four across, one striker.", LBL_541, {
        {  2, -34, DFA}, {  0, -11, DF }, { -4,   0, DF }, {  0,  11, DF },
        {  2,  34, DFA},
        { -6, -30, MFA}, {  0, -10, MF }, {  0,  10, MF }, { -6,  30, MFA},
        {-18,   0, FW } } },

    { "4-2-4", "Back four, two in midfield, four forwards.", LBL_424, {
        {  8, -32, DF }, {  0, -11, DF }, {  0,  11, DF }, {  8,  32, DF },
        {  0, -12, MF }, {  0,  12, MF },
        { -8, -34, FW }, {-12, -12, FW }, {-12,  12, FW }, { -8,  34, FW } } },

    { "3-3-4", "Three at the back, three in the middle, four up.", LBL_334, {
        {  0, -20, DF }, { -4,   0, DF }, {  0,  20, DF },
        {  0, -24, MF }, {  4,   0, MF }, {  0,  24, MF },
        { -8, -34, FW }, {-12, -12, FW }, {-12,  12, FW }, { -8,  34, FW } } },

    { "2-3-5", "The pyramid. Two at the back and five forwards.", LBL_235, {
        {  0, -16, DF }, {  0,  16, DF },
        {  2, -26, MF }, {  4,   0, MF }, {  2,  26, MF },
        {-10, -40, FW }, {-12, -18, FW }, {-16,   0, FW }, {-12,  18, FW },
        {-10,  40, FW } } },
};

#define FORMATION_COUNT ((int)(sizeof kFormations / sizeof kFormations[0]))

/* Names are compared with separators and case thrown away, so a pack can
 * write "4-2-3-1", "4231" or "4 2 3 1" and mean the same thing. */
static void canon(const char *in, char *out, size_t cap) {
    size_t n = 0;
    for (; in && *in && n + 1 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '-' || c == ' ' || c == '_') continue;
        out[n++] = (char)tolower(c);
    }
    out[n] = '\0';
}

const IssdFormation *issd_formation_find(const char *name) {
    if (!name || !name[0]) return NULL;
    char want[32], have[32];
    canon(name, want, sizeof want);
    for (int i = 0; i < FORMATION_COUNT; i++) {
        canon(kFormations[i].name, have, sizeof have);
        if (strcmp(want, have) == 0) return &kFormations[i];
    }
    return NULL;
}

int issd_formation_count(void) { return FORMATION_COUNT; }

const IssdFormation *issd_formation_at(int index) {
    if (index < 0 || index >= FORMATION_COUNT) return NULL;
    return &kFormations[index];
}

const char *issd_formation_label_text(uint8_t label) {
    return label < 16 ? kLabelText[label] : "?-?-?";
}

/* A tactics preset moves the whole shape up or down the pitch and switches
 * the roles between their holding and attacking variants. It never changes
 * how many players are in each line, so the printed label stays true. */
bool issd_formation_apply_tactics(IssdFormation *out,
                                  const IssdFormation *src,
                                  const char *tactics) {
    if (!out || !src) return false;
    *out = *src;
    if (!tactics || !tactics[0]) return true;

    char want[32];
    canon(tactics, want, sizeof want);
    if (strcmp(want, "balanced") == 0 || strcmp(want, "normal") == 0)
        return true;

    int attacking;
    if (strcmp(want, "attacking") == 0 || strcmp(want, "attack") == 0)
        attacking = 1;
    else if (strcmp(want, "defensive") == 0 || strcmp(want, "defence") == 0 ||
             strcmp(want, "defense") == 0)
        attacking = 0;
    else
        return false;

    /* Six units is about a third of the depth range the cartridge's own teams
     * span, so the side visibly shifts without any line running into the one
     * in front of it. */
    const int shift = attacking ? -6 : 6;
    for (int i = 0; i < ISSD_FORMATION_SLOTS; i++) {
        int d = out->slots[i].depth + shift;
        if (d >  100) d =  100;
        if (d < -100) d = -100;
        out->slots[i].depth = (int8_t)d;

        uint8_t r = out->slots[i].role;
        if (attacking) {
            /* Only the wide players are pushed on: an overlapping centre back
             * is not a tactic, it is a mistake. */
            if (r == ISSD_ROLE_DEFENDER && out->slots[i].width <= -24)
                r = ISSD_ROLE_DEFENDER_ATTACK;
            else if (r == ISSD_ROLE_DEFENDER && out->slots[i].width >= 24)
                r = ISSD_ROLE_DEFENDER_ATTACK;
            else if (r == ISSD_ROLE_MIDFIELDER)
                r = ISSD_ROLE_MIDFIELDER_ATTACK;
        } else {
            if (r == ISSD_ROLE_DEFENDER_ATTACK)   r = ISSD_ROLE_DEFENDER;
            else if (r == ISSD_ROLE_MIDFIELDER_ATTACK) r = ISSD_ROLE_MIDFIELDER;
        }
        out->slots[i].role = r;
    }
    return true;
}
