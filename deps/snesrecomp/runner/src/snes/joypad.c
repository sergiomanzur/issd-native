#include "joypad.h"
#include "snes.h"

static uint16_t normalize(uint16_t state) {
  state &= 0xfff;
  if ((state & 48u) == 48u) state &= ~48u;
  if ((state & 192u) == 192u) state &= ~192u;
  return state;
}

void joypad_set_inputs(Snes *snes, const uint16_t inputs[4], uint8_t connected, bool multitap) {
  snes->input1_currentState = normalize(inputs[0]);
  snes->input2_currentState = normalize(inputs[1]);
  snes->input3_currentState = normalize(inputs[2]);
  snes->input4_currentState = normalize(inputs[3]);
  snes->joypadConnected = connected & 15u;
  snes->joypadMultitap = multitap;
}

static uint16_t pad_state(Snes *snes, unsigned player) {
  if (snes->joypadMultitap && !(snes->joypadConnected & (1u << player))) return 0xffff;
  switch (player) {
    case 0: return snes->input1_currentState;
    case 1: return snes->input2_currentState;
    case 2: return snes->input3_currentState;
    default: return snes->input4_currentState;
  }
}

void joypad_write_io(Snes *snes, uint8_t value) { snes->joypadIo = value; }

static void joypad_latch(Snes *snes) {
  snes->joypad1Latched = pad_state(snes, 0);
  snes->joypad2Latched = pad_state(snes, 1);
  snes->joypad3Latched = pad_state(snes, 2);
  snes->joypad4Latched = pad_state(snes, 3);
  snes->joypad1Index = 0;
  snes->joypad2Index = 0;
  snes->joypadPair2Index = 0;
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
  if (port == 1 && snes->joypadMultitap) {
    if (snes->joypadStrobe) return 2; /* MULTI5 presence signature */
    bool first_pair = (snes->joypadIo & 0x80u) != 0;
    uint8_t *index = first_pair ? &snes->joypad2Index : &snes->joypadPair2Index;
    uint16_t a = first_pair ? snes->joypad2Latched : snes->joypad4Latched;
    uint16_t b = first_pair ? snes->joypad3Latched : 0xffff; /* no fifth player */
    if (*index >= 16) return 3;
    uint8_t value = (uint8_t)(((a >> *index) & 1u) | (((b >> *index) & 1u) << 1));
    (*index)++;
    return value;
  }
  if (snes->joypadStrobe) {
    uint16_t state = pad_state(snes, port);
    return (uint8_t)(state & 1u);
  }

  uint16_t latched = port ? snes->joypad2Latched : snes->joypad1Latched;
  uint8_t *index = port ? &snes->joypad2Index : &snes->joypad1Index;
  uint8_t value = *index < 16 ? (uint8_t)((latched >> *index) & 1u) : 1u;
  if (*index < 16) (*index)++;
  return value;
}

void joypad_auto_poll(Snes *snes) {
  joypad_latch(snes);
  snes->joypadStrobe = false;
  bool first_pair = (snes->joypadIo & 0x80u) != 0;
  snes->joypadAutoWords[0] = joypad_auto_read_word(snes->joypad1Latched);
  snes->joypadAutoWords[1] = joypad_auto_read_word(first_pair || !snes->joypadMultitap
                           ? snes->joypad2Latched : snes->joypad4Latched);
  snes->joypadAutoWords[2] = 0; /* port one's second data line */
  snes->joypadAutoWords[3] = snes->joypadMultitap
      ? (first_pair ? joypad_auto_read_word(snes->joypad3Latched) : 0xffff) : 0;
  snes->joypad1Index = 16;
  if (first_pair || !snes->joypadMultitap) snes->joypad2Index = 16;
  else snes->joypadPair2Index = 16;
}

uint8_t joypad_read_auto(Snes *snes, unsigned reg) {
  if (!snes->joypadMultitap) {
    if (reg < 0x421c) return joypad_auto_read_reg(pad_state(snes, (reg - 0x4218) / 2), reg);
    return 0;
  }
  if (reg < 0x4218 || reg > 0x421f) return 0;
  uint16_t word = snes->joypadAutoWords[(reg - 0x4218) / 2];
  return (uint8_t)((reg & 1u) ? word >> 8 : word & 0xffu);
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
