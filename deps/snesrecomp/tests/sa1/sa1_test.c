#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sa1.h"

static int check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
  }
  return 0;
}

int main(void) {
  int fails = 0;
  const size_t rom_size = 4u * 1024u * 1024u;
  uint8_t *rom = (uint8_t *)calloc(1, rom_size);
  uint8_t *bwram = (uint8_t *)calloc(1, 32u * 1024u);
  if (!rom || !bwram) return 2;

  /* SA-1 reset program at $00:8000 (physical ROM offset zero):
   * enable all I-RAM pages and BW-RAM writes, exercise arithmetic, publish
   * results to shared memories, raise the S-CPU IRQ flag, then sleep. */
  static const uint8_t program[] = {
      0xa9, 0xff,       0x8d, 0x2a, 0x22, /* CIWP = ff */
      0xa9, 0x80,       0x8d, 0x27, 0x22, /* CWBE = 80 */
      0xa9, 0x42,       0x8d, 0x00, 0x30, /* I-RAM[0] = 42 */
      0xa9, 0x03,       0x8d, 0x51, 0x22, /* MA = 0003 */
      0xa9, 0x00,       0x8d, 0x52, 0x22,
      0xa9, 0x04,       0x8d, 0x53, 0x22, /* MB = 0004 */
      0xa9, 0x00,       0x8d, 0x54, 0x22, /* multiply */
      0xad, 0x06, 0x23, 0x8d, 0x01, 0x30, /* I-RAM[1] = result */
      0xa9, 0x99,       0x8d, 0x00, 0x60, /* BW-RAM[0] = 99 */
      0xa9, 0x80,       0x8d, 0x09, 0x22, /* signal S-CPU IRQ */
      0xcb                                      /* WAI */
  };
  memcpy(rom, program, sizeof(program));

  Sa1 *sa1 = sa1_create(rom, (uint32_t)rom_size, bwram, 32u * 1024u);
  fails += check(sa1 != NULL, "sa1_create");
  if (!sa1) return 2;

  /* The chip powers up held in reset. */
  sa1_sync(sa1, 1000);
  fails += check(sa1_instructions_executed(sa1) == 0,
                 "SA-1 stays idle while reset is asserted");

  sa1_cpu_write(sa1, 0, 0x2203, 0x00);
  sa1_cpu_write(sa1, 0, 0x2204, 0x80);
  sa1_cpu_write(sa1, 0, 0x2200, 0x00);
  sa1_sync(sa1, 10000);

  fails += check(sa1_cpu_read(sa1, 0, 0x3000, 0xff) == 0x42,
                 "SA-1 CPU writes shared I-RAM");
  fails += check(sa1_cpu_read(sa1, 0, 0x3001, 0xff) == 12,
                 "SA-1 signed multiplier result is readable");
  fails += check(sa1_cpu_read(sa1, 0, 0x6000, 0xff) == 0x99,
                 "SA-1 CPU writes shared BW-RAM");
  fails += check((sa1_cpu_read(sa1, 0, 0x2300, 0) & 0x80) != 0,
                 "SA-1 raises the S-CPU IRQ flag");

  sa1_cpu_write(sa1, 0, 0x2201, 0x80);
  fails += check(sa1_cpu_irq_pending(sa1),
                 "pending S-CPU IRQ asserts when enabled");
  sa1_cpu_write(sa1, 0, 0x2202, 0x80);
  fails += check(!sa1_cpu_irq_pending(sa1),
                 "S-CPU IRQ clear deasserts the line");

  /* Super MMC selection applies to the low bank-C window. */
  rom[0x200000] = 0x5a;
  sa1_cpu_write(sa1, 0, 0x2220, 0x82);
  fails += check(sa1_cpu_read(sa1, 0, 0x8000, 0xff) == 0x5a,
                 "Super MMC remaps bank C to selected one-megabyte page");

  fails += check(sa1_master_clock(sa1) >= 10000,
                 "SA-1 synchronizes to the requested master clock");
  fails += check(sa1_instructions_executed(sa1) > 10,
                 "SA-1 interpreter executed the reset program");

  sa1_destroy(sa1);
  free(bwram);
  free(rom);
  if (fails) return 1;
  puts("sa1_test: PASS");
  return 0;
}
