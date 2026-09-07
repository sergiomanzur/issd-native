#include "issd_widescreen.h"
#include "snes/ppu.h"
#include <string.h>

/* The original streamers ($8B85E3/$8B86E9) maintain a 512x512 ring
 * with only 32 pixels of lookahead. Reconstruct its additional visible
 * columns from the same decompressed 32x32 world metatiles. The transaction
 * never changes WRAM, camera bounds, activation, or game-visible VRAM/OAM. */
static struct {
  Ppu *owner;
  uint16_t vram[0x2000], oam[0x100];
  uint8_t high_oam[0x20];
} frame;

static uint16_t word(const uint8_t *p, unsigned a) {
  return (uint16_t)(p[a] | p[a + 1] << 8);
}

bool issd_widescreen_pitch_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  /* $80846C: 3=demo, 6=menus/game. $50 enables the match OAM builder.
   * Mode alone cannot identify the pitch: menus share mode 6. */
  unsigned mode = word(ram, 0x32), stride = word(ram, 0x1ffcc);
  return (mode == 3 || mode == 6) && word(ram, 0x50) != 0 &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 3 &&
         ppu->bgXsc[1] == 0x13 && stride >= 0x80 &&
         stride <= 0x340 && (stride & 63) == 0;
}

static bool world_tile(const uint8_t *ram, unsigned layer,
                       int x, int y, uint16_t *tile) {
  unsigned stride = word(ram, 0x1ffcc);
  /* $8B87E7: each 256x256 world page is an 8x8 byte block.
   * Every world row contains stride/64 pages. Never wrap out of the map. */
  if (x < 0 || y < 0 || (unsigned)(x >> 8) >= stride / 64) return false;
  unsigned index = (unsigned)(y >> 8) * stride +
                   ((y & 0xe0) >> 2) + (unsigned)(x >> 8) * 64 +
                   ((x & 0xff) >> 5);
  if (index >= 0x1000) return false;
  unsigned metatile = ram[0x1d000 + layer * 0x1000 + index];
  unsigned definition = 0x18000 + layer * 0x2000 + metatile * 32;
  *tile = word(ram, definition + ((y & 31) >> 3) * 8 + ((x & 31) >> 3) * 2);
  return true;
}

static void fill_pitch(Ppu *ppu, const uint8_t *ram, int extra) {
  for (unsigned layer = 0; layer < 2; layer++) {
    /* PPU scroll registers retain ten bits. Recover the current world page
     * from WRAM, retaining the actual scanout offset (vertical is minus one). */
    int sx = (word(ram,0x13a0+layer*32)&~1023) | ppu->hScroll[layer];
    int sy = (word(ram,0x13b0+layer*32)&~1023) | ppu->vScroll[layer];
    int first_x = (sx - extra) & ~7, last_x = (sx + 255 + extra) & ~7;
    for (int y = sy & ~7; y <= ((sy + 223) & ~7); y += 8) {
      for (int x = first_x; x <= last_x; x += 8) {
        /* Shared partial edge tiles already belong to the original view. */
        if (x + 7 >= sx && x < sx + 256) continue;
        uint16_t tile;
        if (!world_tile(ram, layer, x, y, &tile)) continue;
        unsigned tx = ((unsigned)x >> 3) & 63, ty = ((unsigned)y >> 3) & 63;
        unsigned address = layer * 0x1000 + (ty & 31) * 32 +
                           (tx & 31) + (tx >> 5) * 0x400 + (ty >> 5) * 0x800;
        ppu->vram[address] = tile;
      }
    }
  }
}

static bool rom_byte(const uint8_t *rom, size_t size, unsigned a, uint8_t *out) {
  unsigned offset = ((a >> 16) & 0x7f) * 0x8000 + (a & 0x7fff);
  if (!rom || offset >= size) return false;
  *out = rom[offset];
  return true;
}

/* Supplemental OAM comes from the same sorted draw list ($8095E0), not from
 * interpolated/history sprites. Include only pieces the original horizontal
 * clipping rejected; existing native sprites and their priority stay intact. */
static void fill_objects(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                         size_t rom_size, int extra) {
  static const uint8_t sizes[8][2] = {
    {8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}
  };
  uint8_t left[16] = {0}, right[16] = {0};
  int free_slot = 0;
  /* Existing negative sprites are genuine edge pieces; parked F0F0 entries
   * never receive a hint. Positive 9-bit sprites require an explicit hint. */
  for (unsigned slot=0; slot<128; slot++) {
    if (ppu->oam[slot*2] == 0xf0f0) continue;
    unsigned raw = (ppu->oam[slot*2] & 255) |
      ((ppu->highOam[slot/4] >> ((slot%4)*2)) & 1) * 256;
    if (raw >= 512-64) left[slot/8] |= 1 << (slot%8);
  }
  unsigned objects[128], count = 0;
  while (count < 48 && word(ram, 0x1d40 + count*2)) {
    objects[count] = word(ram,0x1d40+count*2); count++;
  }
  /* $83CFE5 and $809B04 omit whole players at x<-32/x>=288. Their
   * simulation and current pose continue; recover them without changing the
   * shared offscreen flag, which gameplay routines also read. */
  for (unsigned object=0x400;object<0x1b00;object+=0x100) {
    bool exists=false;
    for (unsigned i=0;i<count;i++) exists |= objects[i]==object;
    if (!exists) objects[count++]=object;
  }
  /* Referee/assistants and field effects use the auxiliary draw records
   * enumerated by $809B28/$809B4A. Their zero pose is the inactive marker. */
  for (unsigned base=0x400;base<0xd00;base+=0x100) {
    for (unsigned offset=0xa0;offset<=0xd0;offset+=0x30) {
      if (offset==0xd0 && base<0x800) continue;
      unsigned object=base+offset;
      if (!word(ram,object)) continue;
      bool exists=false;
      for (unsigned i=0;i<count;i++) exists |= objects[i]==object;
      if (!exists) objects[count++]=object;
    }
  }
  for (unsigned i=1;i<count;i++) {
    unsigned object=objects[i],j=i;
    while (j && (int16_t)word(ram,objects[j-1]+0x12) > (int16_t)word(ram,object+0x12)) {
      objects[j]=objects[j-1]; j--;
    }
    objects[j]=object;
  }
  /* Nearer objects appear later in the game's list and have OAM priority. */
  while (count) {
    unsigned object = objects[--count];
    if (object < 0x400 || object > 0x1c40) continue;
    unsigned pose = word(ram, object);
    if (!pose) continue;
    bool packed = (pose & 0x8000) != 0;
    uint8_t parts;
    if (packed) {
      if (!rom_byte(rom,rom_size,0x880000 | pose,&parts)) continue;
    } else {
      parts = ram[pose];
    }
    if (!parts || parts > 64) continue;
    unsigned props = ram[object+3] | ((((ram[object+5] & 0x80 ?
      ram[object+5] : ram[0x7c]) & 0x30) | ram[object+4]) & 0xf0);
    for (unsigned part=0; part<parts; part++) {
      int dx, dy; unsigned tile, attr; bool large;
      if (packed) {
        uint8_t data[4]; bool valid = true;
        for (unsigned j=0;j<4;j++)
          valid &= rom_byte(rom,rom_size,0x880000 | (pose+1+part*4+j), &data[j]);
        if (!valid) continue;
        dy = (int8_t)data[0]; dx = (int8_t)data[1]; tile=data[2]; attr=data[3];
        large = (attr & 0x10) != 0;
      } else {
        unsigned index=pose+part*2;
        if (index >= 0xa000) break;
        dx=(int16_t)word(ram,0x2000+index); dy=(int16_t)word(ram,0x4000+index);
        tile=ram[0x6000+index]; attr=ram[0x6001+index];
        large = (ram[index+1] & 0x80) != 0;
      }
      int x=(int16_t)word(ram,object+8)-(large?8:4)+((props&0x40)?-dx:dx);
      int y=(int16_t)word(ram,object+12)+(int16_t)word(ram,object+16)-(large?8:4)+dy;
      int native_left = !packed && large ? -32 : -16;
      int size=sizes[ppu->obsel>>5][large];
      if ((x >= native_left && x < 256) || x + size <= -extra ||
          x >= 256+extra || y+size <= 0 || y >= 224) continue;
      /* These are hardware parked entries, never an arbitrary visible slot. */
      while (free_slot<128 && ppu->oam[free_slot*2]!=0xf0f0) free_slot++;
      if (free_slot == 128) goto done;
      unsigned color=(attr&0xc1)^props;
      if (attr&0x20) color = color&8 ? color|2 : (color&~4)|8;
      ppu->oam[free_slot*2]=(uint8_t)x | ((uint16_t)(uint8_t)y<<8);
      ppu->oam[free_slot*2+1]=(uint8_t)(tile+ram[object+2]) | ((uint16_t)color<<8);
      unsigned shift=(free_slot%4)*2;
      ppu->highOam[free_slot/4]=(ppu->highOam[free_slot/4]&~(3u<<shift)) |
        ((((unsigned)x>>8)&1) | (large?2:0))<<shift;
      (x<0?left:right)[free_slot/8] |= 1 << (free_slot%8);
      free_slot++;
    }
  }
done:
  PpuWsSetOamLeftHints(ppu,left);
  PpuWsSetOamRightHints(ppu,right);
}

bool issd_widescreen_begin(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                          size_t rom_size, int extra) {
  if (!ppu) return false;
  if (frame.owner) issd_widescreen_end(frame.owner);
  if (extra < 0) extra=0;
  if (extra > 95) extra=95;
  PpuWsSetOamLeftHints(ppu,NULL); PpuWsSetOamRightHints(ppu,NULL);
  PpuSetWidescreenLayerClamp(ppu,0);
  for (int l=0;l<4;l++) PpuSetWidescreenLayerClampBand(ppu,l,0,0);
  if (!extra) { PpuSetExtraSpace(ppu,0); return false; }
  if (!issd_widescreen_pitch_layout(ppu,ram)) {
    PpuSetExtraSpaceCentered(ppu,(uint16_t)extra); return false;
  }
  frame.owner=ppu;
  memcpy(frame.vram,ppu->vram,sizeof(frame.vram));
  memcpy(frame.oam,ppu->oam,sizeof(frame.oam));
  memcpy(frame.high_oam,ppu->highOam,sizeof(frame.high_oam));
  PpuSetExtraSpace(ppu,(uint16_t)extra);
  /* The actual world ends at its map edges. Exposing beyond it would read
   * unrelated WRAM as stadium tiles. Keep the widest valid camera view. */
  int left=extra,right=extra;
  int world_width=(word(ram,0x1ffcc)/64)*256;
  for (unsigned layer=0;layer<2;layer++) {
    int sx=(word(ram,0x13a0+layer*32)&~1023)|ppu->hScroll[layer];
    if (left>sx) left=sx;
    if (right>world_width-sx-256) right=world_width-sx-256;
  }
  PpuSetExtraSideSpace(ppu,left>0?left:0,right>0?right:0,0);
  PpuSetWidescreenLayerClamp(ppu,4); /* BG3 carries score, clock, map and names. */
  fill_pitch(ppu,ram,extra);
  fill_objects(ppu,ram,rom,rom_size,extra);
  return true;
}

void issd_widescreen_end(Ppu *ppu) {
  if (frame.owner != ppu || !ppu) return;
  memcpy(ppu->vram,frame.vram,sizeof(frame.vram));
  memcpy(ppu->oam,frame.oam,sizeof(frame.oam));
  memcpy(ppu->highOam,frame.high_oam,sizeof(frame.high_oam));
  frame.owner=NULL;
}
