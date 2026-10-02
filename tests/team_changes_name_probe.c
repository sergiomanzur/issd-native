/* Query the shipped readability name lookup against an actual game snapshot. */
#include "issd_readability.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const uint8_t g_issd_font8x8[96][8] = {{0}};
static uint8_t ram[0x20000], before[0x20000];
int main(int argc, char **argv) {
  if (argc != 3) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f || fread(ram, 1, sizeof(ram), f) != sizeof(ram)) return 3;
  fclose(f); memcpy(before, ram, sizeof(ram));
  char name[9]; unsigned actor = (unsigned)strtoul(argv[2], NULL, 0);
  if (!issd_readability_player_name(ram, actor, name)) return 4;
  if (memcmp(before, ram, sizeof(ram))) return 5;
  puts(name); return 0;
}
