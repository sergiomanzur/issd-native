#ifndef ISSD_GAMEPLAY_H
#define ISSD_GAMEPLAY_H
#include <stdbool.h>
#include <stdint.h>

/* Apply after the cartridge has computed an AI movement target, before it is
 * consumed. Stateless integer policies; disabled calls never write WRAM. */
bool issd_gameplay_goalkeeper(uint8_t *ram, uint16_t actor, bool enabled);
bool issd_gameplay_player(uint8_t *ram, uint16_t actor, bool enabled);
#endif
