#ifndef ISSD_FORMATION_H
#define ISSD_FORMATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Team shapes, as the cartridge actually stores them.
 *
 * Every team owns a 31-byte formation record: a label index, ten home
 * positions for the outfield players, and ten role bytes. The goalkeeper has
 * no entry - he is placed by the engine.
 *
 * The label is only what the team select screen prints. The shape the players
 * actually take up comes from the positions and roles, and those are free, so
 * a modern formation the cartridge never shipped can be built and then
 * labelled with whichever of the sixteen printable labels describes it.
 */
#define ISSD_FORMATION_SLOTS 10

/* Role bytes, read off the cartridge and confirmed against the shapes the
 * teams that use them line up in. The two "attacking" variants are what give
 * a team overlapping full backs or midfielders who push past the strikers;
 * they count as the same line for the printed label. */
enum {
    ISSD_ROLE_DEFENDER          = 1,
    ISSD_ROLE_MIDFIELDER        = 2,
    ISSD_ROLE_FORWARD           = 3,
    ISSD_ROLE_DEFENDER_ATTACK   = 5,
    ISSD_ROLE_MIDFIELDER_ATTACK = 6
};

typedef struct {
    int8_t  depth;   /* negative is further up the pitch */
    int8_t  width;   /* negative is toward one touchline, positive the other */
    uint8_t role;    /* one of ISSD_ROLE_* */
} IssdFormationSlot;

typedef struct {
    const char       *name;         /* "4-2-3-1" - what a mod pack writes */
    const char       *description;
    uint8_t           label;        /* 0-15; what the team select screen prints */
    IssdFormationSlot slots[ISSD_FORMATION_SLOTS];
} IssdFormation;

/* Look a formation up by the name a mod pack uses. Case and separators are
 * ignored, so "4-2-3-1", "4231" and "4 2 3 1" all match. NULL if unknown. */
const IssdFormation *issd_formation_find(const char *name);

int                  issd_formation_count(void);
const IssdFormation *issd_formation_at(int index);

/* The text the team select screen prints for a label index, for logging.
 * All sixteen were read off the screen one at a time. */
const char *issd_formation_label_text(uint8_t label);

/* Copy `src` into `out`, shifted by a tactics preset: "attacking",
 * "balanced" (or NULL/empty) or "defensive". Returns false and copies
 * unchanged if the preset is not recognised. */
bool issd_formation_apply_tactics(IssdFormation *out,
                                  const IssdFormation *src,
                                  const char *tactics);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_FORMATION_H */
