/* Replay omits culled actors; stale live records must not become margin ghosts. */
#define main original_native_fixture_main
#include "test_widescreen_native.c"
#undef main

static void replay_actor(unsigned mode, unsigned absent, int x) {
  fixture(); word(0x70, mode);
  word(0x800, 0x9000); word(0x802, 0x60);
  word(0x808, (uint16_t)x); word(0x80c, 80);
  word(0x814, 0xa400); word(0x816, 2); word(0x818, 2);
  word(0x81e, absent); word(0x830, 1);
  rom[0x12400] = 0; rom[0x12401] = 0x90;
  rom[0x12402] = 0; rom[0x12403] = 0x80; rom[0x12404] = 0x96;
  rom[0xb0000] = 32; rom[0xb0022] = 32;
  memset(rom + 0xb0002, 0x11, 32);
  memset(rom + 0xb0024, 0x22, 32);
  rom[0x41000] = 1; rom[0x41004] = 0x10;
}

static unsigned visible(void) {
  unsigned count=0;
  for (unsigned i=0; i<128; ++i)
    if ((ppu.oam[i*2] >> 8) < 224) ++count;
  return count;
}

int main(void) {
  static uint8_t original_ram[sizeof(ram)];
  const int extras[] = {51, 71, 124};
  for (unsigned width=0; width<3; ++width) {
    for (unsigned side=0; side<2; ++side) {
     for (unsigned listed=0; listed<2; ++listed) {
      int x=side ? 280 : -24;
      /* Original live-game horizontal culls still need reconstruction. */
      replay_actor(8, 1, x);
      if (listed) word(0x1d40, 0x800);
      assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),extras[width]));
      assert(visible()==1); issd_widescreen_end(&ppu);
      /* Recorded replay actors retain real pieces in both widened margins. */
      replay_actor(0x13, 0, x);
      if (listed) word(0x1d40, 0x800);
      assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),extras[width]));
      assert(visible()==1); issd_widescreen_end(&ppu);
      /* $98F279 marks an omitted replay actor absent without clearing its
       * old coordinates/descriptor. Neither movement nor pose may be invented. */
      replay_actor(0x13, 1, x);
      if (listed) word(0x1d40, 0x800);
      memcpy(original_ram,ram,sizeof(ram));
      assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),extras[width]));
      assert(visible()==0);
      assert(!memcmp(original_ram,ram,sizeof(ram)));
      issd_widescreen_end(&ppu);
      assert(visible()==0 && ppu.vram[0x6600]==0);
     }
    }
  }
  /* Presence changes use the previous logic pass, just like latched OAM.
   * An actor that leaves and re-enters the recording must not retain a ghost. */
  replay_actor(0x13, 0, -24);
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(visible()==1); issd_widescreen_end(&ppu);
  word(0x81e,1);
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(visible()==1); issd_widescreen_end(&ppu);
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(visible()==0); issd_widescreen_end(&ppu);
  word(0x81e,0);
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(visible()==0); issd_widescreen_end(&ppu);
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof(rom),71));
  assert(visible()==1); issd_widescreen_end(&ppu);
  puts("Replay absent actors excluded; recorded edge pieces/live culls retained");
  return 0;
}
