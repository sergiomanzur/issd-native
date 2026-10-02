/* Integrated descriptor continuation: real geometry and pixels move together. */
#define main native_fixture_main
#include "test_widescreen_native.c"
#undef main

static uint8_t before_ram[sizeof(ram)];
static uint16_t before_vram[0x8000], before_oam[0x100];
static uint8_t before_high[0x20];

static void test_scene_and_reset(void) {
  fixture();
  word(0x808, 100);
  assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 95));
  issd_widescreen_end(&ppu);
  word(0x70, 0x12); word(0x808, 200);
  assert(!issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 95));
  assert(ppu.extraLeftCur == 0 && ppu.extraRightCur == 0);
  const uint8_t *shown = issd_widescreen_presented_ram(ram);
  assert((shown[0x808] | shown[0x809] << 8) == 200);
  word(0x70, 0x08); word(0x808, 300);
  assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 95));
  shown = issd_widescreen_presented_ram(ram);
  assert((shown[0x808] | shown[0x809] << 8) == 300);
  issd_widescreen_end(&ppu);
  word(0x70, 0x09); word(0x808, 400); /* pitch-to-pitch cut/replay */
  assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 95));
  shown = issd_widescreen_presented_ram(ram);
  assert((shown[0x808] | shown[0x809] << 8) == 400);
  issd_widescreen_reset(); /* Reset must close an outstanding transaction. */
  assert(ppu.vram[0x1800 + 31] == 0xdead);
  assert(!Issd_IsWidescreenActive());
  assert(issd_widescreen_presented_ram(ram) == ram);
  assert(!issd_widescreen_begin(&ppu, NULL, rom, sizeof(rom), 95));
  assert(issd_widescreen_presented_ram(ram) == ram);
}

static void test_ultrawide_scroll_phases(void) {
  /* At most 64 ring columns may be addressed, even with partial edge tiles.
   * Use distinct world metatiles so left/right aliases cannot pass silently. */
  for (unsigned phase = 0; phase < 8; phase++) {
    fixture();
    for (unsigned layer = 0; layer < 2; layer++) {
      for (unsigned i = 0; i < 0x1000; i++)
        ram[0x1d000 + layer * 0x1000 + i] = 1 + (i & 7);
      for (unsigned m = 1; m <= 8; m++)
        for (unsigned t = 0; t < 16; t++)
          word(0x18000 + layer * 0x2000 + m * 32 + t * 2, layer * 0x1000 + m * 32 + t);
      ppu.hScroll[layer] = 256 + phase;
      ppu.vScroll[layer] = 256;
    }
    assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 124));
    assert(ppu.extraLeftCur == 124 && ppu.extraRightCur == 124);
    /* World x136 (metatile5, tile1) and x632 (metatile4, tile3). */
    assert(ppu.vram[0x800 + 17] == 5 * 32 + 1);
    assert(ppu.vram[0x800 + 15] == 4 * 32 + 3);
    for (unsigned x = 256; x <= ((511 + phase) & ~7u); x += 8) {
      unsigned tx = (x >> 3) & 63;
      assert(ppu.vram[0x800 + (tx & 31) + (tx >> 5) * 0x400] == 0xdead);
    }
    issd_widescreen_end(&ppu);
    assert(ppu.vram[0x800 + 17] == 0xdead);
  }
  /* Extreme margins remain representable in 9-bit OAM with side hints. */
  for (unsigned side = 0; side < 2; side++) {
    fixture();
    word(0x1d40, 0x500); word(0x500, 0x9000);
    word(0x508, side ? 380 : (uint16_t)-112); word(0x50c, 80);
    rom[0x41000] = 1; rom[0x41004] = 0x10;
    assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 999));
    assert(ppu.extraLeftCur == 124 && ppu.extraRightCur == 124);
    assert((ppu.oam[0] & 255) == (side ? 116 : 136));
    assert(ppu.highOam[0] & 1);
    assert((side ? ppu.wsOamRightHint[0] : ppu.wsOamLeftHint[0]) & 1);
    issd_widescreen_end(&ppu);
    assert(ppu.oam[0] == 0xf0f0);
  }
}

static unsigned present_checked(int *presented_x) {
  memcpy(before_ram, ram, sizeof(ram));
  memcpy(before_vram, ppu.vram, sizeof(before_vram));
  memcpy(before_oam, ppu.oam, sizeof(before_oam));
  memcpy(before_high, ppu.highOam, sizeof(before_high));
  assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 95));
  const uint8_t *shown = issd_widescreen_presented_ram(ram);
  *presented_x = (int16_t)(shown[0x808] | shown[0x809] << 8);
  unsigned pixels = ppu.vram[0x6600];
  assert(memcmp(before_ram, ram, sizeof(ram)) == 0);
  /* A native top-wrapped sprite is occupied even with y >= 224. */
  assert(ppu.oam[0] == before_oam[0] && ppu.oam[1] == before_oam[1]);
  issd_widescreen_end(&ppu);
  assert(issd_widescreen_presented_ram(ram) == shown);
  assert(memcmp(before_vram, ppu.vram, sizeof(before_vram)) == 0);
  assert(memcmp(before_oam, ppu.oam, sizeof(before_oam)) == 0);
  assert(memcmp(before_high, ppu.highOam, sizeof(before_high)) == 0);
  return pixels;
}

static void test_cold_authored_action(void) {
  fixture();
  word(0x802, 0x60); word(0x808, (uint16_t)-40); word(0x80c, 80);
  word(0x814, 0xa400); word(0x816, 2); word(0x818, 0xcf20);
  word(0x81a, 2); word(0x81c, 0xcf00); word(0x81e, 1);
  word(0x830, 1); word(0x832, 0x38); word(0x892, 0xbecf);
  ppu.oam[0] = 0xfa50; ppu.oam[1] = 0x2470; ppu.highOam[0] = 2;
  rom[0x41000] = 1; rom[0x41004] = 0x10;
  for (unsigned direction = 0; direction < 8; direction++) {
    rom[0x14f00 + direction * 2] = 0x10;
    rom[0x14f01 + direction * 2] = 0xcf;
  }
  for (unsigned i = 0; i < 4; i++) {
    unsigned d = 0x12400 + i * 6, source = 0xb0000 + i * 0x100;
    rom[d] = 0; rom[d + 1] = 0x90;
    rom[d + 2] = 0; rom[d + 3] = 0x80 + i; rom[d + 4] = 0x96;
    rom[source] = 32; rom[source + 34] = 32;
    memset(rom + source + 2, (i + 1) * 0x11, 32);
    memset(rom + source + 36, 0x55 + i, 32);
    rom[0x14f10 + i * 2] = i * 6;
    rom[0x14f11 + i * 2] = i == 3 ? 0x24 : 0xa4;
    rom[0x14f20 + i] = i == 3 ? 0 : 2 + i;
  }
  /* No native observations and no movement: an authored action still
   * advances through its exact ROM graphics, then holds its terminal pose. */
  unsigned distinct = 0;
  int shown;
  for (unsigned f = 0; f < 24; f++) {
    unsigned pixels = present_checked(&shown);
    for (unsigned i = 0; i < 4; i++)
      if (pixels == (i + 1) * 0x1111) distinct |= 1u << i;
    if (f > 14) assert(pixels == 0x4444);
  }
  assert(distinct == 15);
  /* Deactivation and same-snapshot reuse discard the visual cursor. */
  word(0x830, 0); present_checked(&shown); present_checked(&shown);
  word(0x830, 1); present_checked(&shown);
  assert(present_checked(&shown) == 0x1111);
}

int main(void) {
  test_scene_and_reset();
  test_ultrawide_scroll_phases();
  test_cold_authored_action();
  fixture();
  word(0x802, 0x60); word(0x800, 0x9000); word(0x830, 1);
  word(0x818, 3); /* moving-cycle fixture, not a zero-duration terminal hold */
  word(0x80c, 80); word(0x1d40, 0x800);
  /* All four descriptors share geometry; only complete descriptor history
   * can detect the animation, unlike the old history of pose addresses. */
  rom[0x41000] = 1; rom[0x41004] = 0x10;
  for (unsigned i = 0; i < 4; i++) {
    unsigned d = 0x12400 + i * 6, source = 0xb0000 + i * 0x100;
    rom[d] = 0; rom[d+1] = 0x90;
    rom[d+2] = 0; rom[d+3] = 0x80 + i; rom[d+4] = 0x96;
    rom[source] = 32; rom[source+34] = 32;
    memset(rom+source+2, 0x11 * (i+1), 32);
    memset(rom+source+36, 0x55 + i, 32);
  }
  ppu.oam[0] = 0xfa50; ppu.oam[1] = 0x2470;
  ppu.highOam[0] = 2; /* y250, 16px: wraps into visible top rows */
  int shown;
  for (unsigned f = 0; f < 40; f++) {
    word(0x808, 200 - f * 3);
    word(0x814, 0xa400 + ((f/2) % 4) * 6);
    present_checked(&shown);
  }
  word(0x1d40, 0); word(0x81e, 1);
  word(0x814, 0xa412);
  unsigned distinct = 0, changes = 0, last = 0;
  for (unsigned f = 0; f < 32; f++) {
    word(0x808, (uint16_t)(-12 - (int)f * 2));
    unsigned pixels = present_checked(&shown);
    if (f > 0) {
      assert(shown == -12 - ((int)f-1)*2);
      for (unsigned i = 0; i < 4; i++)
        if (pixels == (i+1)*0x1111) distinct |= 1u << i;
      if (last && pixels != last) changes++;
      last = pixels;
    }
  }
  assert(distinct == 15 && changes >= 8);
  /* Camera motion alone must stop continuation: world x stays unchanged. */
  for (unsigned f = 0; f < 20; f++) {
    word(0x808, (uint16_t)(-60 - (int)f));
    word(0x13a0, 100 + f);
    unsigned pixels = present_checked(&shown);
    if (f > 12) assert(pixels == 0x4444);
  }
  /* Fresh cartridge descriptors in the outer band override learned replay. */
  word(0x814, 0xa400);
  present_checked(&shown); /* previous generation */
  assert(present_checked(&shown) == 0x1111);
  /* A culled cold-start action is decoded directly from ROM without first
   * learning a cycle. Disabling and reusing the record must drop old history. */
  word(0x800, 0); word(0x814, 0); word(0x830, 0);
  present_checked(&shown); present_checked(&shown);
  word(0x800, 0x9000); word(0x814, 0xa412); word(0x830, 1);
  for (unsigned f = 0; f < 20; f++) {
    word(0x808, (uint16_t)(-20 - (int)f * 2));
    unsigned pixels = present_checked(&shown);
    if (f) assert(pixels == 0x4444);
  }
  word(0x814, 0xa406); /* real action changes remain authoritative when cold */
  present_checked(&shown);
  assert(present_checked(&shown) == 0x2222);
  /* The native OAM generation is also available for classic 4:3 overlays. */
  issd_widescreen_reset();
  assert(issd_widescreen_presented_ram(ram) == ram);
  word(0x808, 100);
  assert(!issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 0));
  word(0x808, 103);
  assert(!issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 0));
  const uint8_t *classic = issd_widescreen_presented_ram(ram);
  assert((classic[0x808] | classic[0x809] << 8) == 100);
  assert(memcmp(ppu.vram, before_vram, sizeof(before_vram)) == 0);
  /* Geometry may not cross a LoROM bank boundary. Reject it before either
   * graphics row is uploaded, even when backing ROM bytes exist. */
  fixture();
  word(0x802, 0x60); word(0x808, (uint16_t)-40); word(0x80c, 80);
  word(0x814, 0xa400); word(0x830, 1);
  rom[0x12400] = 0xff; rom[0x12401] = 0xff;
  rom[0x12402] = 0; rom[0x12403] = 0x80; rom[0x12404] = 0x96;
  rom[0x47fff] = 1;
  rom[0xb0000] = 32; rom[0xb0022] = 32;
  memset(rom+0xb0002, 0x12, 32); memset(rom+0xb0024, 0x34, 32);
  ppu.vram[0x6600] = 0xbeef;
  assert(issd_widescreen_begin(&ppu, ram, rom, sizeof(rom), 95));
  assert(ppu.vram[0x6600] == 0xbeef && ppu.oam[0] == 0xf0f0);
  issd_widescreen_end(&ppu);
  puts("widescreen reliability tests passed");
  return 0;
}
