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
  /* $80846C: 3=demo, 6=menus/game.
   * Submode in $70: 0x00..0x07 = menus, stadium select, pre-match coin toss (pillarboxed).
   *                 0x08+ = live match (gameplay, fouls, corners, throw-ins, goals, replays).
   * $50 enables the match OAM builder. */
  unsigned mode = word(ram, 0x32);
  unsigned submode = word(ram, 0x70);
  if (mode != 3 && mode != 6) return false;
  if (submode < 0x08) return false;
  /* Check coin toss state machine (CODE_8BC4E4: $38 == 0xC4E4, $3A == 0x8B) */
  if (word(ram, 0x38) == 0xC4E4 && (word(ram, 0x3A) & 0xFF) == 0x8B) return false;

  unsigned stride = word(ram, 0x1ffcc);
  return word(ram, 0x50) != 0 &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 3 &&
         ppu->bgXsc[1] == 0x13 && stride >= 0x80 &&
         stride <= 0x340 && (stride & 63) == 0;
}

static bool world_tile(const uint8_t *ram, unsigned layer,
                       int x, int y, uint16_t *tile) {
  unsigned stride = word(ram, 0x1ffcc);
  /* $8B87E7: each 256x256 world page is an 8x8 byte block.
   * Every world row contains stride/64 pages.
   * Clamp view coordinates into valid world stadium map bounds so
   * viewports extending past sidelines or penalty areas sample the valid
   * stadium boundary metatiles (outer grandstands, hoardings, walls)
   * instead of failing and creating black void margins. */
  if (stride < 64 || (stride & 63) != 0) return false;
  int max_pages_x = (int)(stride / 64);
  int max_x = max_pages_x * 256 - 1;
  if (x < 0) x = 0;
  else if (x > max_x) x = max_x;

  if (y < 0) y = 0;
  unsigned page_y = (unsigned)y >> 8;
  unsigned index = page_y * stride +
                   ((y & 0xe0) >> 2) + ((unsigned)x >> 8) * 64 +
                   ((x & 255) >> 5);
  if (index >= 0x1000) {
    /* Clamp y within the 4096-byte metatile page budget */
    unsigned max_pages_y = 0x1000 / stride;
    if (max_pages_y > 0) {
      y = (int)(max_pages_y * 256) - 1;
      index = ((unsigned)y >> 8) * stride +
              ((y & 0xe0) >> 2) + ((unsigned)x >> 8) * 64 +
              ((x & 255) >> 5);
    }
    if (index >= 0x1000) index = 0x0FFF;
  }
  unsigned metatile = ram[0x1d000 + layer * 0x1000 + index];
  unsigned definition = 0x18000 + layer * 0x2000 + metatile * 32;
  *tile = word(ram, definition + ((y & 31) >> 3) * 8 + ((x & 31) >> 3) * 2);
  return true;
}

static void fill_pitch(Ppu *ppu, const uint8_t *ram, int left, int right) {
  for (unsigned layer = 0; layer < 2; layer++) {
    /* PPU scroll registers retain ten bits. Recover the current world page
     * from WRAM, retaining the actual scanout offset (vertical is minus one). */
    int sx = (word(ram,0x13a0+layer*32)&~1023) | ppu->hScroll[layer];
    int sy = (word(ram,0x13b0+layer*32)&~1023) | ppu->vScroll[layer];
    int first_x = (sx - left) & ~7, last_x = (sx + 255 + right) & ~7;
    for (int y = sy & ~7; y <= ((sy + 223) & ~7); y += 8) {
      for (int x = first_x; x <= last_x; x += 8) {
        /* Shared partial edge tiles already belong to the original view. */
        if (x + 7 >= sx && x < sx + 256) continue;
        unsigned tx = ((unsigned)x >> 3) & 63, ty = ((unsigned)y >> 3) & 63;
        unsigned address = layer * 0x1000 + (ty & 31) * 32 +
                           (tx & 31) + (tx >> 5) * 0x400 + (ty >> 5) * 0x800;
        uint16_t tile;
        if (!world_tile(ram, layer, x, y, &tile)) {
          ppu->vram[address] = 0;
          continue;
        }
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

typedef struct {
  unsigned object;
  bool missing_native_copy;
} ObjectEntry;

/* A slot may only be reused when the hardware cannot draw it on any visible
 * line. Sprite rows are fetched as row = (uint8_t)(line - y), so for y >= 224
 * the first candidate row is 256 - y and the sprite still appears along the top
 * edge whenever 256 - y < height. Treating every y >= 224 as parked therefore
 * overwrote live sprites straddling the top of the screen - most visibly the
 * ball on its way up, which vanished mid-flight and only returned once the
 * keeper caught it and its y dropped back into range. */
static inline bool is_oam_slot_free(const Ppu *ppu, unsigned slot) {
  static const uint8_t sizes[8][2] = {
    {8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}
  };
  unsigned y = ppu->oam[slot*2] >> 8;
  if (y < 224) return false;
  unsigned large = (ppu->highOam[slot/4] >> ((slot%4)*2 + 1)) & 1;
  return y + sizes[PPU_objSize(ppu)][large] <= 256;
}

/* Supplemental OAM comes from the same sorted draw list ($8095E0), not from
 * interpolated/history sprites. If the native list already contains the object,
 * include only pieces the original horizontal clipping rejected. If the native
 * list omitted the whole object, every piece intersecting the widened viewport
 * needs a supplemental copy because there is no native OAM to preserve. */
static void fill_objects(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                         size_t rom_size, int left_extra, int right_extra) {
  static const uint8_t sizes[8][2] = {
    {8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}
  };
  uint8_t left[16] = {0}, right[16] = {0};
  int free_slot = 0;
  /* Existing negative sprites are genuine edge pieces; parked entries
   * never receive a hint. Positive 9-bit sprites require an explicit hint. */
  for (unsigned slot=0; slot<128; slot++) {
    if (is_oam_slot_free(ppu, slot)) continue;
    unsigned raw = (ppu->oam[slot*2] & 255) |
      ((ppu->highOam[slot/4] >> ((slot%4)*2)) & 1) * 256;
    if (raw >= 512-64) left[slot/8] |= 1 << (slot%8);
  }
  ObjectEntry objects[128];
  unsigned count = 0;
  while (count < 48 && word(ram, 0x1d40 + count*2)) {
    objects[count].object = word(ram,0x1d40+count*2);
    objects[count].missing_native_copy = false;
    count++;
  }
  /* Active players ($0400..$1B00 step $0100) not in the 4:3 list:
   * add if they have an active pose */
  for (unsigned object=0x400;object<0x1b00;object+=0x100) {
    if (!word(ram, object)) continue;
    bool exists=false;
    for (unsigned i=0;i<count;i++) {
      if (objects[i].object==object) { exists=true; break; }
    }
    if (!exists && count < 128) {
      objects[count].object=object;
      objects[count].missing_native_copy=true;
      count++;
    }
  }
  /* Active match officials (referee, linesmen) */
  const unsigned aux_officials[] = { 0x04A0, 0x08A0, 0x08D0 };
  for (unsigned k=0; k < sizeof(aux_officials)/sizeof(aux_officials[0]); k++) {
    unsigned object = aux_officials[k];
    if (!word(ram, object)) continue;
    bool exists=false;
    for (unsigned i=0;i<count;i++) {
      if (objects[i].object==object) { exists=true; break; }
    }
    if (!exists && count < 128) {
      objects[count].object=object;
      objects[count].missing_native_copy=true;
      count++;
    }
  }
  for (unsigned i=1;i<count;i++) {
    ObjectEntry entry=objects[i];
    unsigned object=entry.object,j=i;
    while (j && (int16_t)word(ram,objects[j-1].object+0x12) > (int16_t)word(ram,object+0x12)) {
      objects[j]=objects[j-1]; j--;
    }
    objects[j]=entry;
  }
  /* Nearer objects appear later in the game's list and have OAM priority. */
  while (count) {
    ObjectEntry entry = objects[--count];
    unsigned object = entry.object;
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
      bool native_part_visible = x >= native_left && x < 256;
      if ((!entry.missing_native_copy && native_part_visible) ||
          x + size <= -left_extra || x >= 256+right_extra ||
          y+size <= 0 || y >= 224) continue;
      /* These are hardware parked entries, never an arbitrary visible slot. */
      while (free_slot<128 && !is_oam_slot_free(ppu, (unsigned)free_slot)) free_slot++;
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

/* The cartridge double buffers its sprite list: every NMI first DMAs the OAM
 * built by the previous logic pass, then rebuilds it for the next one. Measured
 * on a live match, PPU OAM and the latched PPU scroll registers at frame N are
 * exactly the object records and camera as they stood at frame N-1.
 *
 * The supplement must therefore reconstruct from that same generation. Reading
 * this frame's fresh records emitted geometry one frame ahead of the native
 * sprites it was completing, so an object crossing the native clip edge was
 * either dropped (the supplement deferred to a native copy that was never
 * built) or drawn twice at two positions, and a camera crossing a 1024 pixel
 * page boundary mixed this frame's high scroll bits with last frame's low bits
 * and slewed the whole reconstructed margin by a full page for one frame. */
static uint8_t s_prev_ram[0x20000];
static bool s_prev_ram_valid = false;

void issd_widescreen_reset(void) { s_prev_ram_valid = false; }

static void remember_ram(const uint8_t *ram) {
  if (!ram) return;
  memcpy(s_prev_ram, ram, sizeof(s_prev_ram));
  s_prev_ram_valid = true;
}

static int s_ws_extra = 0;

bool Issd_IsWidescreenActive(void) {
  return s_ws_extra > 0;
}

bool issd_widescreen_begin(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                          size_t rom_size, int extra) {
  if (!ppu) { s_ws_extra = 0; remember_ram(ram); return false; }
  if (frame.owner) issd_widescreen_end(frame.owner);
  if (extra < 0) extra=0;
  if (extra > 95) extra=95;
  PpuWsSetOamLeftHints(ppu,NULL); PpuWsSetOamRightHints(ppu,NULL);
  PpuSetWidescreenLayerClamp(ppu,0);
  for (int l=0;l<4;l++) PpuSetWidescreenLayerClampBand(ppu,l,0,0);
  /* Classic 4:3 takes none of the presentation branches: no VRAM/OAM
   * transaction, no supplemental sprites, no reconstructed margins. */
  if (!extra) {
    s_ws_extra = 0;
    PpuSetExtraSpace(ppu,0);
    remember_ram(ram);
    return false;
  }
  static int s_inactive_frames = 0;
  bool is_pitch = issd_widescreen_pitch_layout(ppu, ram);
  if (is_pitch) {
    s_inactive_frames = 0;
  } else {
    s_inactive_frames++;
  }

  if (!is_pitch && (s_inactive_frames >= 2 || s_ws_extra == 0)) {
    s_ws_extra = 0;
    PpuSetExtraSpaceCentered(ppu,(uint16_t)extra);
    remember_ram(ram);
    return false;
  }
  s_ws_extra = extra;
  frame.owner=ppu;
  memcpy(frame.vram,ppu->vram,sizeof(frame.vram));
  memcpy(frame.oam,ppu->oam,sizeof(frame.oam));
  memcpy(frame.high_oam,ppu->highOam,sizeof(frame.high_oam));
  PpuSetExtraSpace(ppu,(uint16_t)extra);
  PpuSetExtraSideSpace(ppu, extra, extra, 0);
  /* The pitch lives on BG1/BG2. BG3 carries score, clock, radar and player
   * labels; leaving it eligible for side-margin rendering repeats the 4:3 HUD
   * into the widened field. Keep only BG1/BG2 in the expanded columns and
   * widen pitch-layer windows so box/goal-area line masks follow the new view. */
  PpuSetWidescreenLayerMask(ppu, 3);
  PpuSetWidescreenWindowExpansion(ppu, 3, 3);
  PpuSetWidescreenLayerClamp(ppu, 4);
  /* Reconstruct against the WRAM generation that produced the native OAM and
   * the currently latched scroll registers, not this frame's fresh one. */
  const uint8_t *rec = s_prev_ram_valid ? s_prev_ram : ram;
  fill_pitch(ppu, rec, extra, extra);
  fill_objects(ppu, rec, rom, rom_size, extra, extra);
  remember_ram(ram);
  return true;
}

void issd_widescreen_end(Ppu *ppu) {
  if (frame.owner != ppu || !ppu) return;
  memcpy(ppu->vram,frame.vram,sizeof(frame.vram));
  memcpy(ppu->oam,frame.oam,sizeof(frame.oam));
  memcpy(ppu->highOam,frame.high_oam,sizeof(frame.high_oam));
  frame.owner=NULL;
}

