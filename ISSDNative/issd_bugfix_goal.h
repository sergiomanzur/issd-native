#ifndef ISSD_BUGFIX_GOAL_H
#define ISSD_BUGFIX_GOAL_H
#include <stdbool.h>
#include <stdint.h>

/* At 038DAB, turn an already-outside goal witness into original BCS rejection.
 * Caller changes carry only when the original routine returned carry clear. */
bool issd_bugfix_goal_reject(const uint8_t *ram, bool enabled);
/* At 06DBBE, after the original INC $00A2,x; team comes from WRAM $14D2.
 * The original HUD only renders two decimal digits. */
bool issd_bugfix_goal_score(uint8_t *ram, uint16_t team, bool enabled);
/* At 24DBF6 after A4DBDB copies a fouled actor's unchecked restart location. */
bool issd_bugfix_goal_restart(uint8_t *ram, bool enabled);
#endif
