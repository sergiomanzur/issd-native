#include "issd_widescreen.h"
#include "issd_pose_history.h"
#include "snes/ppu.h"
#include <string.h>

/* The original streamers ($8B85E3/$8B86E9) maintain a 512x512 ring
 * with only 32 pixels of lookahead. Reconstruct its additional visible
 * columns from the same decompressed 32x32 world metatiles. The transaction
 * never changes WRAM, camera bounds, activation, or game-visible VRAM/OAM. */
static struct {
  Ppu *owner;
  uint16_t vram[0x8000], oam[0x100];
  uint8_t high_oam[0x20];
} frame;

static uint16_t word(const uint8_t *p, unsigned a) {
  return (uint16_t)(p[a] | p[a + 1] << 8);
}

bool issd_widescreen_pitch_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  /* $80846C: 3=demo, 6=menus/game. $50 enables the match OAM builder.
   * Submode in $70, measured across a full 6000 frame match:
   *   0x00..0x07  menus and stadium select
   *   0x08        live match play (5173 of 5236 in-match frames)
   *   0x09, 0x0B  brief in-match states, left enabled
   *   0x1C        pre-match coin toss
   *
   * The coin toss happens on the pitch, so every other pitch signal is set
   * and it passed the >= 0x08 test, leaving the reconstruction to build side
   * margins from metatile maps the game has not populated yet: broken grass
   * either side. It is pillarboxed by submode instead. */
  unsigned mode = word(ram, 0x32);
  unsigned submode = word(ram, 0x70);
  if (mode != 3 && mode != 6) return false;
  if (submode < 0x08) return false;
  /* Pre-match presentation: the stadium fly-in, the coin toss and the
   * framed pitch view. It all happens on the pitch, so stride, $50, the BG
   * mode and both tilemap bases read exactly like live play, but the world
   * metatile maps hold nothing for the widened columns yet. Reconstructing
   * them painted grass and crowd rows either side of the framed view.
   *
   * Measured across a full Open Game from boot to kickoff, 0x0F covers 1061
   * frames of that sequence and 0x1C one more; an earlier fix excluded only
   * 0x1C, which is why the coin toss stayed broken.
   * Submode 0x12 is the half-time stats screen (CODE_80B2C2). It uses mode 6
   * and stadium stride, but is a framed stats card where metatiles should not
   * leak into the side margins as green lines. */
  if (submode == 0x0F || submode == 0x10 || submode == 0x12 || submode == 0x1C) return false;
  /* A previous guard tested $38 == 0xC4E4 && $3A == 0x8B here. Neither holds
   * during the coin toss: $38 is 0 throughout and $3A is a frame counter, so
   * the guard never once fired. Removed rather than left as reassurance. */

  unsigned stride = word(ram, 0x1ffcc);
  return word(ram, 0x50) != 0 &&
         (ppu->bgmode & 0xf7) == 1 && ppu->bgXsc[0] == 3 &&
         ppu->bgXsc[1] == 0x13 && stride >= 0x80 &&
         stride <= 0x340 && (stride & 63) == 0;
}

/* Menus are pillarboxed unless they are one of the screens built the way the
 * cartridge builds all of them: BG2 a tiling wallpaper, BG1 the panels and
 * BG3 or sprites the text. Verified by isolating layers on the main menu and
 * the scenario select, which share the layering exactly.
 *
 * Only BG2 is extended, and only by repeating what is already on screen. The
 * wallpaper is a repeating pattern, so it tiles into the margins seamlessly;
 * nothing is scaled and no artwork is invented. Panels and text stay at their
 * authored positions in the middle 256 pixels. */
bool issd_widescreen_menu_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  unsigned mode = word(ram, 0x32);
  /* Modes 0 and 1 are boot, the Konami logo and the title screen. The title
   * is an HDMA mode 3 split with windowed photo frames; widening its layers
   * would fight that, and it is not a menu. */
  if (mode != 5 && mode != 6) return false;
  if (issd_widescreen_pitch_layout(ppu, ram)) return false;
  /* A menu has no stadium loaded. The pre-match presentation does: it runs
   * on the pitch with a valid metatile stride, and only the framed centre is
   * meant to be visible. Without this it is not a pitch (its submode is
   * excluded above) and it does have BG2, so it fell through to the menu
   * path and had grass and crowd repeated into its margins - the same broken
   * green the pitch reconstruction had been painting there. */
  {
    unsigned stride = word(ram, 0x1ffcc);
    if (stride >= 0x80 && stride <= 0x340 && (stride & 63) == 0) return false;
  }
  /* Some screens composite the wallpaper through the sub screen for colour
   * math and leave only sprites on the main screen: the scenario select runs
   * main=0x10, sub=0x07. Checking the main screen alone missed those. */
  return ((ppu->screenEnabled[0] | ppu->screenEnabled[1]) & (1u << 1)) != 0;
}

/* The title screen is an HDMA mode 3 split with windowed photo frames, so
 * extending its layers would fight the split. It does not need extending:
 * with every layer disabled the picture still renders white, which means the
 * white is the CGRAM backdrop rather than any layer. Widening the picture
 * while clamping every layer to the middle 256 pixels therefore fills the
 * margins with the screen own backdrop colour and leaves the split, the
 * windows and the sprites exactly where the cartridge put them. */
bool issd_widescreen_title_layout(const Ppu *ppu, const uint8_t *ram) {
  if (!ppu || !ram) return false;
  return word(ram, 0x32) == 1;
}

/* The stride is a page allocation, not the authored map width. Stadium maps
 * end partway through their last page; the remaining zero columns are blank
 * metatiles (solid green on BG1). Find the occupied horizontal extent per
 * layer and repeat its outermost 8-pixel tile column when a corner exposes
 * that padding. Repeating the whole 32-pixel metatile repeats diagonal wall
 * transitions as a checkerboard instead of continuing the outer surface. */
static void world_x_bounds(const uint8_t *ram, unsigned layer,
                           int *first, int *last) {
  unsigned stride = word(ram, 0x1ffcc);
  *first = (int)(stride / 64) * 256;
  *last = -32;
  if (!stride) return;
  for (unsigned i = 0; i < 0x1000; i++) {
    if (!ram[0x1d000 + layer * 0x1000 + i]) continue;
    unsigned column = (i % stride) / 64 * 8 + (i & 7);
    int x = (int)column * 32;
    if (x < *first) *first = x;
    if (x > *last) *last = x;
  }
  if (*last < *first) { *first = 0; *last = (int)(stride / 64) * 256 - 32; }
}

static bool world_tile(const uint8_t *ram, unsigned layer,
                       int x, int y, int first, int last, uint16_t *tile) {
  unsigned stride = word(ram, 0x1ffcc);
  /* $8B87E7: each 256x256 world page is an 8x8 byte block.
   * Every world row contains stride/64 pages.
   * Clamp view coordinates into valid world stadium map bounds so
   * viewports extending past sidelines or penalty areas sample the valid
   * stadium boundary metatiles (outer grandstands, hoardings, walls)
   * instead of failing and creating black void margins. */
  if (stride < 64 || (stride & 63) != 0) return false;
  if (x < first) x = first + (x & 7);
  else if (x >= last + 32) x = last + 24 + (x & 7);

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
    int first, last;
    world_x_bounds(ram, layer, &first, &last);
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
        if (!world_tile(ram, layer, x, y, first, last, &tile)) {
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

static const uint8_t *rom_span(const uint8_t *rom, size_t size,
                               unsigned address, unsigned count) {
  unsigned offset = ((address >> 16) & 0x7f) * 0x8000 + (address & 0x7fff);
  if (!rom || (address & 0xffff) < 0x8000 ||
      count > 0x10000 - (address & 0xffff) ||
      offset > size || count > size - offset) return NULL;
  return rom + offset;
}

/* $84E6C7 updates object+$14 even when $1E suppresses the pose and graphics
 * DMA. The descriptor in bank $82 pairs the actual pose with two
 * length-prefixed graphics rows. Replaying only OAM (or a remembered pose)
 * interprets old menu tiles or a different animation's tiles as a player.
 * Reproduce those uploads in the presentation transaction, including the
 * uniform/number tile selected by $84E77D. Each player owns its tile slots. */
static uint16_t prepare_player(Ppu *ppu, const uint8_t *ram, unsigned object,
                               const uint8_t *rom, size_t rom_size) {
  const uint8_t *desc = rom_span(rom, rom_size,
      0x820000 | word(ram, object + 0x14), 6);
  if (!desc || !ram[object + 0x30]) return 0;
  unsigned source = desc[2] | desc[3] << 8 | desc[4] << 16;
  const uint8_t *rows[2];
  unsigned lengths[2];
  for (unsigned row = 0; row < 2; row++) {
    const uint8_t *header = rom_span(rom, rom_size, source, 2);
    if (!header) return 0;
    lengths[row] = word(header, 0);
    if (!lengths[row] || lengths[row] > 0x100 || (lengths[row] & 1)) return 0;
    rows[row] = rom_span(rom, rom_size, source + 2, lengths[row]);
    if (!rows[row]) return 0;
    source += 2 + lengths[row];
  }
  unsigned dest = 0x6000 + (word(ram, object + 2) & 0x1ff) * 16;
  if (dest + 0x180 > 0x8000) return 0;
  for (unsigned row = 0; row < 2; row++)
    for (unsigned j = 0; j < lengths[row]; j += 2)
      ppu->vram[dest + row * 0x100 + j / 2] = word(rows[row], j);

  unsigned type = ram[object + 0x30];
  if (type == 8) {
    const uint8_t *entry = rom_span(rom, rom_size, 0x81ceec + desc[5], 2);
    const uint8_t *pixels = entry ?
        rom_span(rom, rom_size, 0xa40000 | word(entry, 0), 64) : NULL;
    if (pixels)
      for (unsigned j = 0; j < 64; j += 2)
        ppu->vram[0x7fe0 + j / 2] = word(pixels, j);
  }
  if (type > 1 && type != 9) {
    unsigned tile = 0xf014; /* blank uniform detail */
    unsigned detail = desc[5];
    if (type != 7 && type != 8 && ram[object + 0x57] != 0x54) {
      if (detail & 0x80) {
        const uint8_t *entry = rom_span(rom, rom_size,
            0x81ce8a + ram[object + 0x57], 2);
        if (entry) {
          tile = word(entry, 0);
          if (tile != 0xf014) tile += ((detail & 0x7f) - 1) * 32;
        }
      } else if (detail && detail <= 12 && !(detail & 1)) {
        if ((detail == 4 || detail == 6) && (ram[object + 4] & 0x40))
          detail ^= 2;
        const uint8_t *entry = rom_span(rom, rom_size, 0x81cede + detail, 2);
        if (entry) {
          tile = word(entry, 0);
          if (detail < 10) tile += ram[object + 0x56] * 16;
        }
      }
    }
    const uint8_t *pixels = rom_span(rom, rom_size, 0x980000 | tile, 32);
    if (pixels)
      for (unsigned j = 0; j < 32; j += 2)
        ppu->vram[dest + 0x170 + j / 2] = word(pixels, j);
  }
  return word(desc, 0);
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
  unsigned left_threshold = 512u - (unsigned)(left_extra + 32);
  if (left_threshold > 512u - 64u) left_threshold = 512u - 64u;
  for (unsigned slot=0; slot<128; slot++) {
    if (is_oam_slot_free(ppu, slot)) continue;
    unsigned raw = (ppu->oam[slot*2] & 255) |
      ((ppu->highOam[slot/4] >> ((slot%4)*2)) & 1) * 256;
    if (raw >= left_threshold) left[slot/8] |= 1 << (slot%8);
  }
  ObjectEntry objects[128];
  unsigned count = 0;
  while (count < 48 && word(ram, 0x1d40 + count*2)) {
    objects[count].object = word(ram,0x1d40+count*2);
    objects[count].missing_native_copy = false;
    count++;
  }
  /* Include players whose animation descriptor advanced while their native
   * pose/graphics upload was culled. Zero-pose inactive records stay inactive. */
  for (unsigned object=0x400;object<0x1b00;object+=0x100) {
    unsigned pose = word(ram, object);
    if (!pose && !(object >= 0x500 && ram[object + 0x30] &&
                   (word(ram, object + 0x14) & 0x8000))) continue;
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
  /* Auxiliary draw records: officials, and the field objects enumerated by
   * $809B28/$809B4A. A zero pose is the inactive marker.
   *
   * This is a scan rather than a list of known ids because hardcoding three
   * of them left five live records with no supplemental copy at all: over a
   * 6000 frame match, $09A0, $09D0, $0AA0, $0AD0 and $0BA0 were live and
   * inside the widescreen margin for 3290 frames with nothing drawing them,
   * which is what made objects wink in and out at the edges. One of the three
   * hardcoded ids, $08A0, was never live at all. */
  for (unsigned base=0x400; base<0xd00; base+=0x100) {
   for (unsigned offset=0xa0; offset<=0xd0; offset+=0x30) {
    if (offset==0xd0 && base<0x800) continue;
    unsigned object = base + offset;
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
    int ox = (int16_t)word(ram, object + 8);
    int oy = (int16_t)word(ram, object + 12);
    if (entry.missing_native_copy && object >= 0x500 && object < 0x1b00 &&
        !(object & 255) && ram[object + 0x30] &&
        (word(ram, object + 0x14) & 0x8000)) {
      /* Player geometry fits within 64 pixels of its origin. Avoid uploading
       * invisible players, especially the shared type-8 detail tile. */
      if (ox + 64 <= -left_extra || ox - 64 >= 256 + right_extra) continue;
      pose = prepare_player(ppu, ram, object, rom, rom_size);
      if (!pose) continue;
    }
    /* Geometry and graphics now follow the same cartridge descriptor. A
     * guessed history pose cannot safely use another animation's tile data. */
    issd_pose_history_observe(object, ox, oy, (uint16_t)pose);
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

void issd_widescreen_reset(void) {
  s_prev_ram_valid = false;
  issd_pose_history_reset();
}

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
  PpuSetWidescreenLayerRepeat(ppu,0);
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
  static bool s_was_pitch = false;
  bool is_pitch = issd_widescreen_pitch_layout(ppu, ram);
  if (is_pitch && !s_was_pitch) {
    issd_widescreen_reset();
  }
  s_was_pitch = is_pitch;
  if (is_pitch) {
    s_inactive_frames = 0;
  } else {
    s_inactive_frames++;
  }

  if (!is_pitch && (s_inactive_frames >= 2 || s_ws_extra == 0)) {
    s_ws_extra = 0;
    unsigned submode = word(ram, 0x70);
    bool is_pillarboxed = (submode == 0x0F || submode == 0x10 || submode == 0x12 || submode == 0x1C);
    if (is_pillarboxed) {
      PpuSetExtraSpaceCentered(ppu, (uint16_t)extra);
    } else {
      PpuSetExtraSpace(ppu, (uint16_t)extra);
      PpuSetExtraSideSpace(ppu, extra, extra, 0);
      if (issd_widescreen_menu_layout(ppu, ram)) {
        PpuSetWidescreenLayerRepeat(ppu, 1u << 1);                     /* BG2 */
        PpuSetWidescreenLayerClamp(ppu, (1u<<0) | (1u<<2) | (1u<<3));  /* rest */
      } else {
        PpuSetWidescreenLayerClamp(ppu, 0x0F);   /* all layers clamped, backdrop fills margins */
      }
    }
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

