#define main original_fixture_main
#include "test_widescreen_native.c"
#undef main
void issd_replay_observe(const uint8_t *,uint32_t);
void issd_replay_reset(void);
void issd_replay_save_state(uint8_t *);
bool issd_replay_validate_state(const uint8_t *,size_t);
bool issd_replay_load_state(const uint8_t *,size_t);
#define HISTORY_SIZE (4u + 512u * 448u)
static uint8_t history[HISTORY_SIZE], original[0x20000], unchanged[HISTORY_SIZE];
static unsigned visible(void) {
  unsigned count=0;
  for(unsigned i=0;i<128;i++) if((ppu.oam[i*2]>>8)<224) count++;
  return count;
}
static void replay_actor(unsigned mode,unsigned absent,int x) {
  fixture();word(0x70,mode);word(0x800,0x9000);word(0x802,0x60);
  word(0x808,(uint16_t)x);word(0x80c,80);word(0x814,0xa400);
  word(0x816,2);word(0x818,2);word(0x81e,absent);word(0x830,1);
  rom[0x12401]=0x90;rom[0x12403]=0x80;rom[0x12404]=0x96;
  rom[0xb0000]=32;rom[0xb0022]=32;
  memset(rom+0xb0002,0x11,32);memset(rom+0xb0024,0x22,32);
  rom[0x41000]=1;rom[0x41004]=0x10;
}

int main(void) {
  issd_replay_reset();
  replay_actor(8,1,-24);
  word(0x18a4,2);word(0x18a0,0x420);
  word(0x10000,0x400);word(0x1040f,0x41e);
  memcpy(original,ram,sizeof ram);
  issd_replay_observe(ram,0x8baafd);
  assert(!memcmp(original,ram,sizeof ram));
  issd_replay_save_state(history);
  assert(issd_replay_validate_state(history,sizeof history));
  issd_replay_reset();
  assert(issd_replay_load_state(history,sizeof history));
  word(0x70,0x13);word(0x18aa,0);word(0x808,0xff00);
  /* Decode frame zero, then the original advances its cursor to frame one. */
  issd_replay_observe(ram,0x8bae80);word(0x18aa,2);
  issd_widescreen_reset();
  memcpy(original,ram,sizeof ram);
  assert(issd_widescreen_begin(&ppu,ram,rom,sizeof rom,71));
  assert(visible()==1);
  assert(!memcmp(original,ram,sizeof ram));
  issd_widescreen_end(&ppu);
  /* Data-ring reuse invalidates a matching directory slot. */
  ram[0x10404]^=1;issd_widescreen_reset();
  issd_widescreen_begin(&ppu,ram,rom,sizeof rom,71);
  assert(visible()==0);issd_widescreen_end(&ppu);
  assert(!issd_replay_load_state(history,sizeof history-1));
  history[4+8+18]=2;
  assert(!issd_replay_validate_state(history,sizeof history));
  issd_replay_save_state(unchanged);
  assert(!issd_replay_load_state(history,sizeof history));
  issd_replay_save_state(history);
  assert(!memcmp(history,unchanged,sizeof history));
  /* Both complete multi-piece actors and native/extra split actors survive.
   * The internal 256-pixel border must never become a second sprite clip. */
  for(unsigned side=0;side<2;side++) {
    issd_replay_reset();replay_actor(8,1,side ? 280 : -30);
    rom[0x41000]=2;rom[0x41005]=0;rom[0x41006]=8;rom[0x41008]=0x10;
    word(0x18a4,2);word(0x18a0,0x420);word(0x10000,0x400);
    issd_replay_observe(ram,0x8baafd);
    word(0x70,0x13);word(0x18aa,0);
    issd_replay_observe(ram,0x8bae80);
    issd_widescreen_reset();
    issd_widescreen_begin(&ppu,ram,rom,sizeof rom,71);
    assert(visible()==2);issd_widescreen_end(&ppu);
  }
  issd_replay_reset();replay_actor(0x13,0,254);
  rom[0x41000]=2;rom[0x41004]=0;rom[0x41005]=0;
  rom[0x41006]=16;rom[0x41008]=0;
  word(0x1d40,0x800);
  ppu.oam[0]=(76u<<8)|250u;ppu.oam[1]=0x60;
  issd_widescreen_begin(&ppu,ram,rom,sizeof rom,71);
  assert(visible()==2 && (ppu.oam[2]&255)==10);
  assert(ppu.wsOamRightHint[0]&2);
  issd_widescreen_end(&ppu);
  puts("Replay history restores real edge actors without changing guest memory");
}
