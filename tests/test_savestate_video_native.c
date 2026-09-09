/* A savestate that carries only WRAM cannot be restored at boot: the picture
 * comes from CGRAM/OAM/VRAM, which the game streams as it walks its menus. To
 * reproduce a specific pitch position headlessly the slot must also carry the
 * PPU snapshot region, so loading one reconstructs the frame as well as the
 * simulation. */
#include "issd_save.h"
#include "issd_bridge.h"
#include "snes/snes.h"
#include "snes/ppu.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

uint8_t g_ram[0x20000];
int snes_frame_counter;
Snes *g_snes;
Cpu *g_snes_cpu;

void issd_bridge_update_state(IssdMatchState *out_state) {
  memset(out_state, 0, sizeof(*out_state));
  out_state->game_mode1 = 0x02;
}

static void fill(uint8_t *p, size_t n, unsigned seed) {
  for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(seed + i * 7u + (i >> 5));
}

int main(void) {
  static Snes snes;
  static Ppu ppu;
  g_snes = &snes;
  snes.ppu = &ppu;

  fill(g_ram, sizeof(g_ram), 0x11);
  fill((uint8_t *)ppu.cgram, sizeof(ppu.cgram), 0x22);
  fill((uint8_t *)ppu.oam, sizeof(ppu.oam), 0x33);
  fill(ppu.highOam, sizeof(ppu.highOam), 0x44);
  fill((uint8_t *)ppu.vram, sizeof(ppu.vram), 0x55);

  static uint8_t want_ram[sizeof(g_ram)];
  static uint16_t want_cgram[0x100], want_oam[0x100], want_vram[0x8000];
  static uint8_t want_high_oam[0x20];
  memcpy(want_ram, g_ram, sizeof(want_ram));
  memcpy(want_cgram, ppu.cgram, sizeof(want_cgram));
  memcpy(want_oam, ppu.oam, sizeof(want_oam));
  memcpy(want_high_oam, ppu.highOam, sizeof(want_high_oam));
  memcpy(want_vram, ppu.vram, sizeof(want_vram));

  assert(issd_save_to_slot(0, "pitch"));

  /* Whatever the machine happens to hold at boot must not survive the load. */
  fill(g_ram, sizeof(g_ram), 0x99);
  fill((uint8_t *)ppu.cgram, sizeof(ppu.cgram), 0x99);
  fill((uint8_t *)ppu.oam, sizeof(ppu.oam), 0x99);
  fill(ppu.highOam, sizeof(ppu.highOam), 0x99);
  fill((uint8_t *)ppu.vram, sizeof(ppu.vram), 0x99);

  assert(issd_load_from_slot(0));

  assert(memcmp(g_ram, want_ram, sizeof(want_ram)) == 0);
  assert(memcmp(ppu.cgram, want_cgram, sizeof(want_cgram)) == 0);
  assert(memcmp(ppu.oam, want_oam, sizeof(want_oam)) == 0);
  assert(memcmp(ppu.highOam, want_high_oam, sizeof(want_high_oam)) == 0);
  assert(memcmp(ppu.vram, want_vram, sizeof(want_vram)) == 0);

  /* A slot written by the WRAM-only format has no picture in it; replaying it
   * would silently render the wrong frame, so it must be refused outright. */
  FILE *f = fopen("saves/slot_0.sav", "r+b");
  assert(f);
  uint32_t old_version = 1;
  assert(fseek(f, 4, SEEK_SET) == 0);
  assert(fwrite(&old_version, sizeof(old_version), 1, f) == 1);
  fclose(f);
  assert(!issd_load_from_slot(0));

  return 0;
}
