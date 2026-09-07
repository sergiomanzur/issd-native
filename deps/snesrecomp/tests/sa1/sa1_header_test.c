#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cart.h"
#include "snes.h"

static int loaded_type;
static int loaded_rom_size;
static int loaded_ram_size;

void cart_load(Cart *cart, int type, uint8_t *rom, int rom_size, int ram_size) {
  (void)cart;
  (void)rom;
  loaded_type = type;
  loaded_rom_size = rom_size;
  loaded_ram_size = ram_size;
}

static int check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
  }
  return 0;
}

int main(void) {
  static uint8_t rom[0x8000];
  Snes snes;
  Cart cart;
  const size_t header = 0x7fc0;
  int fails = 0;

  memset(&snes, 0, sizeof(snes));
  memset(&cart, 0, sizeof(cart));
  memset(rom, 0, sizeof(rom));
  snes.cart = &cart;

  memcpy(rom + header, "SUPER MARIO RPG      ", 21);
  rom[header + 0x15] = 0x23;
  rom[header + 0x16] = 0x35;
  rom[header + 0x17] = 12;
  rom[header + 0x18] = 5;
  rom[header + 0x19] = 1;
  rom[header + 0x1a] = 0x33;
  rom[header + 0x1c] = 0x4b;
  rom[header + 0x1d] = 0xc4;
  rom[header + 0x1e] = 0xb4;
  rom[header + 0x1f] = 0x3b;
  rom[header + 0x3c] = 0x90;
  rom[header + 0x3d] = 0xff;
  rom[0x7f90 & 0x7fff] = 0x78;

  fails += check(snes_loadRom(&snes, rom, sizeof(rom)),
                 "SA-1 header loads");
  fails += check(loaded_type == CART_SA1,
                 "$23/$35 header selects SA-1 mapping");
  fails += check(loaded_rom_size == 0x8000,
                 "synthetic header test ROM stays power-of-two sized");
  fails += check(loaded_ram_size == 32 * 1024,
                 "SA-1 header provisions 32 KiB BW-RAM");

  if (fails) return 1;
  puts("sa1_header_test: PASS");
  return 0;
}
