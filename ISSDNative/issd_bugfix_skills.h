#ifndef ISSD_BUGFIX_SKILLS_H
#define ISSD_BUGFIX_SKILLS_H
#include <stdbool.h>
#include <stdint.h>

/* Call before the original $86B12C roster-Down entry (LoROM mirror $06B12C
 * is equivalent). DP must be the original menu record, $1500. No CPU register
 * changes, saved host state, budget clamps, or lifecycle reset are required. */
bool issd_bugfix_skills(uint8_t *ram, uint16_t direct_page, bool enabled);
#endif
