#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "joypad.h"
#include "snes.h"

static int check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
  }
  return 0;
}

int main(void) {
  Snes snes;
  int fails = 0;
  memset(&snes, 0, sizeof(snes));

  /* Internal input order is B,Y,Select,Start,Up,Down,Left,Right,A,X,L,R. */
  snes.input1_currentState = (1u << 0) | (1u << 3) |
                             (1u << 8) | (1u << 11);
  joypad_write_strobe(&snes, 1);
  joypad_write_strobe(&snes, 0);
  for (unsigned bit = 0; bit < 16; bit++) {
    uint8_t expected = bit == 0 || bit == 3 || bit == 8 || bit == 11;
    fails += check(joypad_read_serial(&snes, 0) == expected,
                   "latched controller bit order");
  }
  fails += check(joypad_read_serial(&snes, 0) == 1,
                 "reads after bit 15 report a connected controller");

  snes.input1_currentState = 0;
  joypad_write_strobe(&snes, 1);
  fails += check(joypad_read_serial(&snes, 0) == 0,
                 "high strobe tracks the live B button");
  snes.input1_currentState = 1;
  fails += check(joypad_read_serial(&snes, 0) == 1,
                 "high strobe continuously relatches");

  if (fails) return 1;
  puts("manual_joypad_test: PASS");
  return 0;
}
