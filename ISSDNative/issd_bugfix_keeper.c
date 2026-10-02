#include "issd_bugfix_keeper.h"

static unsigned moved_keepers;
void issd_bugfix_keeper_begin_loop(void) { moved_keepers = 0; }

static unsigned word(const uint8_t *r, unsigned a) {
    return r[a] | (unsigned)r[a + 1] << 8;
}

bool issd_bugfix_keeper_skip_movement(const uint8_t *ram, uint16_t actor, bool enabled) {
    if (!enabled || !ram || (actor != 0x500 && actor != 0x1000)) return false;
    unsigned controller = word(ram, 0x88);
    if (controller < 0x1aa0 || controller > 0x1b60 ||
        (controller - 0x1aa0) % 0x30 || word(ram, controller + 0x2c) != actor ||
        (word(ram, controller + 0x2e) & 0x8000)) return false;
    /* $8381DC selects the team keeper and $838406 writes its actor into the
     * requesting controller without checking other controller ownership.
     * $84804D then runs $848571 for each human record. Its $8485D3 call
     * to $80D2B3 integrates the same keeper's full velocity again.
     * Skip that movement branch for later human owners; retain their inputs,
     * selected actors, velocity and animation logic. Nothing is normalized or
     * clamped. Count actual movement branches, not selected controller records:
     * an earlier outfielder can select the keeper after its own movement. */
    unsigned bit = actor == 0x500 ? 1u : 2u;
    bool already_moved = (moved_keepers & bit) != 0;
    moved_keepers |= bit;
    return already_moved;
}
