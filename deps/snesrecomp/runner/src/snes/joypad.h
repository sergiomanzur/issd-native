#ifndef SNES_JOYPAD_H
#define SNES_JOYPAD_H

#include <stdint.h>
#include <stdbool.h>

struct Snes;

/* Port one is P1; a multitap on port two carries P2/P3 on its first
 * pair and P4/an unconnected fifth pad on its second pair. */
void joypad_set_inputs(struct Snes *snes, const uint16_t inputs[4], uint8_t connected, bool multitap);
void joypad_write_io(struct Snes *snes, uint8_t value);
void joypad_auto_poll(struct Snes *snes);
uint8_t joypad_read_auto(struct Snes *snes, unsigned reg);

void joypad_write_strobe(struct Snes *snes, uint8_t value);
uint8_t joypad_read_serial(struct Snes *snes, unsigned port);
uint16_t joypad_auto_read_word(uint16_t state);
uint8_t joypad_auto_read_reg(uint16_t state, unsigned reg);

#endif /* SNES_JOYPAD_H */
