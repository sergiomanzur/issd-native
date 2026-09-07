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

static int check(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

static void make_lorom_header(uint8_t *rom, uint8_t map_mode,
                              uint8_t cartridge_type) {
  const size_t header = 0x7fc0;
  memset(rom, 0, 0x8000);
  memcpy(rom + header, "SUPER MARIO KART     ", 21);
  rom[header + 0x15] = map_mode;
  rom[header + 0x16] = cartridge_type;
  rom[header + 0x17] = 9;
  rom[header + 0x18] = 1;
  rom[header + 0x19] = 1;
  rom[header + 0x1c] = 0x34;
  rom[header + 0x1d] = 0x12;
  rom[header + 0x1e] = 0xcb;
  rom[header + 0x1f] = 0xed;
  rom[header + 0x3c] = 0x00;
  rom[header + 0x3d] = 0x80;
  rom[0] = 0x78;
}

int main(void) {
  uint8_t rom[0x8000];
  Snes snes;
  Cart cart;
  int fails = 0;

  memset(&snes, 0, sizeof snes);
  memset(&cart, 0, sizeof cart);
  snes.cart = &cart;

  make_lorom_header(rom, 0x20, 0x03);
  fails += check(snes_loadRom(&snes, rom, sizeof rom),
                 "DSP-1 header loads");
  fails += check(loaded_type == CART_DSP1,
                 "$FFD6 low nibble identifies NEC DSP");
  fails += check(loaded_rom_size == 0x8000,
                 "synthetic ROM keeps its power-of-two size");
  fails += check(loaded_ram_size == 0x800,
                 "SMK header declares 2 KiB SRAM");

  make_lorom_header(rom, 0x31, 0x05);
  fails += check(snes_loadRom(&snes, rom, sizeof rom),
                 "title-gated SMK Fast HiROM conversion loads");
  fails += check(loaded_type == CART_DSP1_HIROM,
                 "SMK $31/$05 header selects HiROM DSP-1 mapping");
  fails += check(loaded_ram_size == 0x800,
                 "converted SMK keeps 2 KiB SRAM");

  loaded_type = 0;
  loaded_ram_size = -1;
  make_lorom_header(rom, 0x23, 0x00);
  fails += check(snes_loadRom(&snes, rom, sizeof rom),
                 "plain LoROM header loads");
  fails += check(loaded_type == CART_LOROM,
                 "$FFD5 map nibble is not mistaken for a DSP");
  fails += check(loaded_ram_size == 0,
                 "plain ROM does not gain phantom SRAM");

  make_lorom_header(rom, 0x30, 0x03);
  fails += check(!snes_loadRom(&snes, rom, sizeof rom),
                 "DSP-4-style header is rejected rather than misclassified");

  make_lorom_header(rom, 0x20, 0x05);
  fails += check(!snes_loadRom(&snes, rom, sizeof rom),
                 "other NEC DSP board signatures require explicit support");

  if (fails) return 1;
  puts("dsp1_header_test: PASS");
  return 0;
}
