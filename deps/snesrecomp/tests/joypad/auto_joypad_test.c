#include <stdint.h>
#include <stdio.h>

#include "joypad.h"

static int check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
  }
  return 0;
}

static int expect_auto_word(uint16_t state, unsigned base, uint16_t expected,
                            const char *label) {
  uint8_t lo = joypad_auto_read_reg(state, base);
  uint8_t hi = joypad_auto_read_reg(state, base + 1);
  char msg[128];
  int fails = 0;

  snprintf(msg, sizeof(msg), "%s low byte", label);
  fails += check(lo == (uint8_t)(expected & 0xffu), msg);
  snprintf(msg, sizeof(msg), "%s high byte", label);
  fails += check(hi == (uint8_t)(expected >> 8), msg);
  snprintf(msg, sizeof(msg), "%s 16-bit little-endian word", label);
  fails += check(((uint16_t)lo | ((uint16_t)hi << 8)) == expected, msg);
  return fails;
}

int main(void) {
  int fails = 0;

  /* Runner input state is the serial joypad order:
   * B,Y,Select,Start,Up,Down,Left,Right,A,X,L,R.
   *
   * SNES automatic joypad registers store the first serial bit in bit 15 of
   * the 16-bit word at $4218/$4219:
   *   high byte $4219: B,Y,Select,Start,Up,Down,Left,Right
   *   low byte  $4218: A,X,L,R,0,0,0,0
   */
  fails += expect_auto_word(1u << 6, 0x4218, 0x0200, "p1 left");

  fails += expect_auto_word(1u << 8, 0x4218, 0x0080, "p1 A");

  fails += expect_auto_word((1u << 0) | (1u << 7) | (1u << 11),
                            0x4218, 0x8110, "p1 B+Right+R");

  fails += expect_auto_word((1u << 5) | (1u << 9),
                            0x421a, 0x0440, "p2 Down+X");

  if (fails) return 1;
  puts("auto_joypad_test: PASS");
  return 0;
}
