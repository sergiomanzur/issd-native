/* The formation library, and the one invariant that keeps it honest.
 *
 * The cartridge prints a three-number label that is chosen separately from
 * the shape, so nothing stops a formation from lining up 4-2-3-1 while the
 * screen claims 3-4-3. Every entry is checked here instead.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "issd_formation.h"

/* Read off the team select screen by writing each index into a team's record
 * and screenshotting the result; repeated here so a change to the library's
 * copy has to be deliberate. */
static const char *const kLabel[16] = {
    "4-5-1", "4-4-2", "4-3-3", "4-2-4",
    "3-5-2", "3-4-3", "3-3-4", "3-2-5",
    "2-5-3", "2-4-4", "2-3-5", "5-4-1",
    "5-3-2", "5-2-3", "1-5-4", "1-4-5"
};

int main(void) {
    const int n = issd_formation_count();
    assert(n > 0);

    for (int i = 0; i < 16; i++)
        assert(strcmp(issd_formation_label_text((uint8_t)i), kLabel[i]) == 0);

    for (int i = 0; i < n; i++) {
        const IssdFormation *f = issd_formation_at(i);
        assert(f && f->name && f->name[0]);

        int line[3] = {0, 0, 0};
        for (int s = 0; s < ISSD_FORMATION_SLOTS; s++) {
            switch (f->slots[s].role) {
                case ISSD_ROLE_DEFENDER:
                case ISSD_ROLE_DEFENDER_ATTACK:   line[0]++; break;
                case ISSD_ROLE_MIDFIELDER:
                case ISSD_ROLE_MIDFIELDER_ATTACK: line[1]++; break;
                case ISSD_ROLE_FORWARD:           line[2]++; break;
                default: assert(!"role byte outside the set the cartridge uses");
            }
        }
        assert(line[0] + line[1] + line[2] == ISSD_FORMATION_SLOTS);

        /* The printed label must count the same lines the roles do. */
        char shown[8];
        snprintf(shown, sizeof shown, "%d-%d-%d", line[0], line[1], line[2]);
        assert(f->label < 16);
        if (strcmp(shown, issd_formation_label_text(f->label)) != 0) {
            fprintf(stderr, "%s lines up %s but is labelled %s\n",
                    f->name, shown, issd_formation_label_text(f->label));
            assert(!"formation label disagrees with its own shape");
        }

        /* Two players standing on the same spot is an authoring slip. */
        for (int a = 0; a < ISSD_FORMATION_SLOTS; a++)
            for (int b = a + 1; b < ISSD_FORMATION_SLOTS; b++)
                assert(f->slots[a].depth != f->slots[b].depth ||
                       f->slots[a].width != f->slots[b].width);
    }

    /* Lookup ignores case and separators. */
    assert(issd_formation_find("4-2-3-1") == issd_formation_find("4231"));
    assert(issd_formation_find("4-2-3-1") == issd_formation_find("4 2 3 1"));
    assert(issd_formation_find("3-4-2-1") != NULL);
    assert(issd_formation_find("9-9-9") == NULL);
    assert(issd_formation_find("") == NULL);
    assert(issd_formation_find(NULL) == NULL);

    /* Tactics move the shape without changing how many are in each line, so
     * the label the screen prints stays true. */
    const IssdFormation *base = issd_formation_find("4-4-2");
    IssdFormation att, def, bal;
    assert(issd_formation_apply_tactics(&att, base, "attacking"));
    assert(issd_formation_apply_tactics(&def, base, "defensive"));
    assert(issd_formation_apply_tactics(&bal, base, NULL));
    assert(att.label == base->label && def.label == base->label);
    assert(memcmp(&bal, base, sizeof bal) == 0);

    for (int s = 0; s < ISSD_FORMATION_SLOTS; s++) {
        /* Negative depth is further up the pitch: confirmed by shifting every
         * slot by the same amount and watching the radar. */
        assert(att.slots[s].depth < base->slots[s].depth);
        assert(def.slots[s].depth > base->slots[s].depth);
        /* Defensive never leaves an attacking role behind. */
        assert(def.slots[s].role == ISSD_ROLE_DEFENDER ||
               def.slots[s].role == ISSD_ROLE_MIDFIELDER ||
               def.slots[s].role == ISSD_ROLE_FORWARD);
    }
    /* Attacking pushes the full backs on but never a centre back. */
    assert(att.slots[0].role == ISSD_ROLE_DEFENDER_ATTACK);   /* width -32 */
    assert(att.slots[1].role == ISSD_ROLE_DEFENDER);          /* width -11 */
    assert(att.slots[3].role == ISSD_ROLE_DEFENDER_ATTACK);   /* width  32 */

    assert(!issd_formation_apply_tactics(&att, base, "sideways"));

    puts("formation tests passed");
    return 0;
}
