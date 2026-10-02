#ifndef ISSD_BUGFIX_KEEPER_H
#define ISSD_BUGFIX_KEEPER_H
#include <stdbool.h>
#include <stdint.h>

/* At $8485D1 with original N=1, use BPL $8485FE when this returns true.
 * The controller loop's $88 qualifies human ownership. Never writes WRAM. */
bool issd_bugfix_keeper_skip_movement(const uint8_t *ram, uint16_t actor, bool enabled);
/* Reset before executing $848048 and on load/reset. Counts only actual
 * movement branches, so a mid-loop keeper selection preserves its first move.
 * Completed-frame snapshots resume before the next controller-loop reset. */
void issd_bugfix_keeper_begin_loop(void);
#endif
