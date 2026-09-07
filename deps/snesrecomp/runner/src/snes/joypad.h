#ifndef SNES_JOYPAD_H
#define SNES_JOYPAD_H

#include <stdint.h>

struct Snes;

void joypad_write_strobe(struct Snes *snes, uint8_t value);
uint8_t joypad_read_serial(struct Snes *snes, unsigned port);
uint16_t joypad_auto_read_word(uint16_t state);
uint8_t joypad_auto_read_reg(uint16_t state, unsigned reg);

#endif /* SNES_JOYPAD_H */
