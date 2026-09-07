#include "joypad.h"
#include "snes.h"

static void joypad_latch(Snes *snes) {
  snes->joypad1Latched = snes->input1_currentState;
  snes->joypad2Latched = snes->input2_currentState;
  snes->joypad1Index = 0;
  snes->joypad2Index = 0;
}

void joypad_write_strobe(Snes *snes, uint8_t value) {
  if (!snes) return;
  bool next = (value & 1u) != 0;
  if (next || snes->joypadStrobe)
    joypad_latch(snes);
  snes->joypadStrobe = next;
}

uint8_t joypad_read_serial(Snes *snes, unsigned port) {
  if (!snes || port > 1) return 1;
  if (snes->joypadStrobe) {
    uint16_t state = port ? snes->input2_currentState
                          : snes->input1_currentState;
    return (uint8_t)(state & 1u);
  }

  uint16_t latched = port ? snes->joypad2Latched : snes->joypad1Latched;
  uint8_t *index = port ? &snes->joypad2Index : &snes->joypad1Index;
  uint8_t value = *index < 16 ? (uint8_t)((latched >> *index) & 1u) : 1u;
  if (*index < 16) (*index)++;
  return value;
}

uint16_t joypad_auto_read_word(uint16_t state) {
  uint16_t word = 0;
  for (int i = 0; i < 16; i++, state >>= 1)
    word = (uint16_t)(word * 2u + (state & 1u));
  return word;
}

uint8_t joypad_auto_read_reg(uint16_t state, unsigned reg) {
  uint16_t word = joypad_auto_read_word(state);
  return (uint8_t)((reg & 1u) ? (word >> 8) : (word & 0xffu));
}
