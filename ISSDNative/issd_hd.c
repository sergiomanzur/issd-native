/* High resolution background tiles.
 *
 * How this works, and why it works this way:
 *
 * The PPU renders at 256x224 into a buffer that is then scaled up by an
 * integer factor for the window. There is no point in the pipeline where a
 * replacement image could simply be substituted, because by the time a frame
 * exists the tiles have already been flattened into pixels.
 *
 * So the tilemaps are walked a second time, after the frame is drawn. For
 * every background tile on screen its identity is computed, a replacement is
 * looked up, and if one exists its pixels are written into the scaled buffer.
 *
 * The hard part is occlusion: a tile may be covered by a sprite, or by a
 * higher layer, or altered by colour math, and the second walk knows none of
 * that. Rather than reimplement the PPU's priority rules, each pixel is
 * checked against the frame the PPU actually produced: a pixel is replaced
 * only when it still holds exactly the colour that tile would have put there.
 * Anything drawn on top changes the colour and is left alone automatically.
 * A sprite pixel that happens to match exactly is replaced, which is
 * invisible - it is the same colour either way.
 *
 * Identity is the tile's graphics data plus the palette it is drawn in, so
 * the same artwork in two colour schemes is two textures. That matches how
 * the cartridge reuses art and keeps a pack editable as plain images.
 */
#include "issd_hd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "snes/ppu.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define hd_mkdir(p) _mkdir(p)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#define hd_mkdir(p) mkdir((p), 0755)
#endif

/* ---------------------------------------------------------------- tiles -- */

#define TILE_PX 8

/* Per-scanline register snapshot. The title screen drives a mode change
 * through HDMA partway down the frame, so one snapshot per frame would
 * describe the wrong background for everything above the split. */
typedef struct {
    uint8_t  bgmode;          /* raw $2105: mode in bits 0-2, big tiles above */
    uint8_t  bgXsc[4];        /* $2107-$210A */
    uint16_t bgTileAdr;       /* $210B/$210C */
    uint16_t hScroll[4];
    uint16_t vScroll[4];
    uint8_t  mainLayers;      /* $212C */
    uint8_t  valid;
} HdLineRegs;

#define MAX_LINES 240
static HdLineRegs s_lines[MAX_LINES];

/* ------------------------------------------------------------- textures -- */

typedef struct {
    uint64_t  key;            /* tile identity; 0 means the slot is empty */
    uint32_t *pixels;         /* size x size ARGB, top-down */
    int       size;           /* edge length in pixels; scale = size / 8 */
} HdTexture;

static HdTexture *s_table;
static size_t     s_table_mask;      /* capacity - 1, capacity a power of two */
static int        s_texture_count;
static char       s_pack_name[128];
static int        s_last_hits;

/* Dumping keeps its own table of what has already been written so a tile that
 * is on screen for a thousand frames is written once. */
static char      *s_dump_dir;
static uint64_t  *s_dumped;
static size_t     s_dumped_mask;
static int        s_dumped_count;
static FILE      *s_dump_manifest;
static unsigned   s_frame;
static bool       s_dump_full_warned;
static unsigned   s_dump_start;   /* ignore frames before this one */

/* ------------------------------------------------------------------ BMP -- */

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1, bfReserved2;
    uint32_t bfOffBits;
} HdBmpFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth, biHeight;
    uint16_t biPlanes, biBitCount;
    uint32_t biCompression, biSizeImage;
    int32_t  biXPelsPerMeter, biYPelsPerMeter;
    uint32_t biClrUsed, biClrImportant;
} HdBmpInfoHeader;
#pragma pack(pop)

/* Reads the 32-bit top-down BMPs this file also writes. Bottom-up files are
 * accepted too, because that is what most editors save. */
static uint32_t *hd_load_bmp(const char *path, int *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    HdBmpFileHeader fh;
    HdBmpInfoHeader ih;
    if (fread(&fh, sizeof fh, 1, f) != 1 || fread(&ih, sizeof ih, 1, f) != 1 ||
        fh.bfType != 0x4D42 || ih.biBitCount != 32 || ih.biCompression > 3) {
        fclose(f);
        return NULL;
    }
    const int w = ih.biWidth;
    const int h = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
    if (w <= 0 || w != h || w % TILE_PX != 0 || w > 8 * 64) { fclose(f); return NULL; }

    uint32_t *px = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    if (!px) { fclose(f); return NULL; }
    if (fseek(f, (long)fh.bfOffBits, SEEK_SET) != 0 ||
        fread(px, sizeof(uint32_t), (size_t)w * h, f) != (size_t)w * h) {
        free(px); fclose(f); return NULL;
    }
    fclose(f);

    if (ih.biHeight > 0) {                     /* bottom-up: flip into place */
        for (int y = 0; y < h / 2; y++)
            for (int x = 0; x < w; x++) {
                uint32_t t = px[y * w + x];
                px[y * w + x] = px[(h - 1 - y) * w + x];
                px[(h - 1 - y) * w + x] = t;
            }
    }
    *out_size = w;
    return px;
}

static bool hd_save_bmp(const char *path, const uint32_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    HdBmpFileHeader fh = { 0x4D42,
        (uint32_t)(sizeof(HdBmpFileHeader) + sizeof(HdBmpInfoHeader) + w * h * 4),
        0, 0, (uint32_t)(sizeof(HdBmpFileHeader) + sizeof(HdBmpInfoHeader)) };
    HdBmpInfoHeader ih = { sizeof(HdBmpInfoHeader), w, -h, 1, 32, 0,
        (uint32_t)(w * h * 4), 2835, 2835, 0, 0 };
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    fwrite(px, 4, (size_t)w * h, f);
    fclose(f);
    return true;
}

/* ----------------------------------------------------------- hash table -- */

static uint64_t hd_mix(uint64_t h, uint64_t v) {
    h ^= v;
    h *= 0x100000001B3ull;                     /* FNV-1a prime */
    return h;
}

static void hd_table_free(void) {
    if (s_table) {
        for (size_t i = 0; i <= s_table_mask; i++) free(s_table[i].pixels);
        free(s_table);
    }
    s_table = NULL;
    s_table_mask = 0;
    s_texture_count = 0;
}

static bool hd_table_alloc(size_t want) {
    size_t cap = 64;
    while (cap < want * 2) cap <<= 1;
    s_table = (HdTexture *)calloc(cap, sizeof(HdTexture));
    if (!s_table) return false;
    s_table_mask = cap - 1;
    return true;
}

static HdTexture *hd_table_slot(uint64_t key);

/* Open addressing never terminates on a full table, and stacked packs can
 * hold far more tiles than one. Double before that can happen. */
static bool hd_table_grow(void) {
    HdTexture *old = s_table;
    const size_t old_cap = s_table_mask + 1;
    const size_t cap = old_cap * 2;
    s_table = (HdTexture *)calloc(cap, sizeof(HdTexture));
    if (!s_table) { s_table = old; return false; }
    s_table_mask = cap - 1;
    for (size_t i = 0; i < old_cap; i++)
        if (old[i].key) *hd_table_slot(old[i].key) = old[i];
    free(old);
    return true;
}

static HdTexture *hd_table_slot(uint64_t key) {
    size_t i = (size_t)key & s_table_mask;
    for (;;) {
        if (!s_table[i].key || s_table[i].key == key) return &s_table[i];
        i = (i + 1) & s_table_mask;
    }
}

static const HdTexture *hd_lookup(uint64_t key) {
    if (!s_table) return NULL;
    const HdTexture *slot = hd_table_slot(key);
    return slot->key == key ? slot : NULL;
}

/* --------------------------------------------------------- tile identity -- */

/* Identity is not cheap: an 8bpp tile hashes 32 words of graphics and all
 * 256 colours it can reach. A tile eight pixels tall is walked on eight
 * consecutive scanlines and usually appears many times across the screen,
 * so the same work would be repeated tens of thousands of times a frame.
 *
 * Within one composited frame the graphics and the palette are fixed, so a
 * direct-mapped memo keyed on where the tile came from is exact. It is
 * invalidated by frame rather than cleared, so the cost is one comparison.
 */
typedef struct {
    uint64_t tag;
    uint64_t key;
    const void *tex;
    uint32_t generation;
} HdMemoEntry;

#define HD_MEMO_SIZE 8192
static HdMemoEntry s_memo[HD_MEMO_SIZE];
static uint32_t    s_generation;


/* Words of VRAM one tile's graphics occupies. VRAM is addressed in words, not
 * bytes, so this is also the stride from one character to the next. */
static int hd_tile_words(int bpp) { return bpp * 4; }   /* 2bpp 8, 4bpp 16, 8bpp 32 */

/* A tile is its graphics plus the colours it is drawn in. Hashing the palette
 * as well means the same artwork used for two teams' kits is two textures,
 * which is what a pack author wants. */
static uint64_t hd_tile_key(const Ppu *ppu, unsigned tileadr, unsigned character,
                            int bpp, unsigned pal_base) {
    uint64_t h = 0xCBF29CE484222325ull;
    h = hd_mix(h, (uint64_t)bpp);
    const unsigned words = (unsigned)hd_tile_words(bpp);
    const unsigned base = tileadr + character * words;
    for (unsigned i = 0; i < words; i++) {
        /* Bit planes sit in pairs eight words apart, but the whole tile is one
         * contiguous run of `words`, so a flat walk covers all of it. */
        h = hd_mix(h, ppu->vram[(base + i) & 0x7FFF]);
    }
    const unsigned colours = 1u << bpp;
    for (unsigned i = 0; i < colours; i++)
        h = hd_mix(h, ppu->cgram[(pal_base + i) & 0xFF]);
    return h ? h : 1;                          /* 0 marks an empty slot */
}

/* Decode one pixel of a tile, as the PPU's own fetch does. */
static unsigned hd_tile_pixel(const Ppu *ppu, unsigned tileadr, unsigned character,
                              int bpp, unsigned px, unsigned py) {
    const unsigned words = (unsigned)hd_tile_words(bpp);
    const unsigned addr = (tileadr + character * words + py) & 0x7FFF;
    const unsigned bit = 7u - px;
    unsigned pixel = 0;
    for (int plane = 0; plane < bpp / 2; plane++) {
        const uint16_t w = ppu->vram[(addr + (unsigned)plane * 8u) & 0x7FFF];
        pixel |= ((w >> bit) & 1u) << (plane * 2);
        pixel |= ((w >> (bit + 8)) & 1u) << (plane * 2 + 1);
    }
    return pixel;
}

/* The colour the PPU writes for a palette entry, master brightness included.
 * Red is the low five bits of the SNES word. */
static uint32_t hd_colour(const Ppu *ppu, unsigned index) {
    const uint16_t c = ppu->cgram[index & 0xFF];
    return ((uint32_t)ppu->brightnessMult[c & 0x1F] << 16) |
           ((uint32_t)ppu->brightnessMult[(c >> 5) & 0x1F] << 8) |
            (uint32_t)ppu->brightnessMult[(c >> 10) & 0x1F];
}

/* Colour depth of a layer in a given background mode; 0 when the mode does
 * not use that layer. Modes 5 and 6 are interlaced/offset variants this pass
 * does not walk, and mode 7 is not a tilemap at all. */
static int hd_layer_bpp(int mode, int layer) {
    switch (mode) {
        case 0: return 2;
        case 1: return layer == 2 ? 2 : (layer < 2 ? 4 : 0);
        case 2: return layer < 2 ? 4 : 0;
        case 3: return layer == 0 ? 8 : (layer == 1 ? 4 : 0);
        case 4: return layer == 0 ? 8 : (layer == 1 ? 2 : 0);
        default: return 0;
    }
}

/* Palette base for a tilemap entry. 8bpp addresses all 256 colours directly;
 * everything else takes a palette number out of the entry. Mode 0 gives each
 * layer its own quarter of CGRAM. */
static unsigned hd_palette_base(int mode, int layer, int bpp, uint16_t tile) {
    if (bpp == 8) return 0;
    const unsigned pal = (tile & 0x1C00u) >> 10;
    if (mode == 0) return (pal + (unsigned)layer * 8u) * 4u;
    return pal * (unsigned)(1 << bpp);
}

/* ------------------------------------------------------------- dumping --- */

static bool hd_dump_seen(uint64_t key) {
    if (!s_dumped) {
        s_dumped_mask = 65535;
        s_dumped = (uint64_t *)calloc(s_dumped_mask + 1, sizeof(uint64_t));
        if (!s_dumped) return true;
    }
    size_t i = (size_t)key & s_dumped_mask;
    for (;;) {
        if (!s_dumped[i]) {
            if ((size_t)s_dumped_count * 2 >= s_dumped_mask) {
                if (!s_dump_full_warned) {
                    s_dump_full_warned = true;
                    fprintf(stderr, "[HD] Dumped %d distinct tiles; that is"
                            " as many as one run tracks. Narrow the run or"
                            " filter tiles.csv by frame.\n", s_dumped_count);
                }
                return true;
            }
            s_dumped[i] = key;
            s_dumped_count++;
            return false;
        }
        if (s_dumped[i] == key) return true;
        i = (i + 1) & s_dumped_mask;
    }
}

static void hd_dump_tile(const Ppu *ppu, uint64_t key, unsigned tileadr,
                         unsigned character, int bpp, unsigned pal_base) {
    /* A run that walks to a particular screen passes through every screen
     * before it, so a pack for one of them starts by ignoring the rest. */
    if (!s_dump_dir || s_frame < s_dump_start || hd_dump_seen(key)) return;

    uint32_t px[TILE_PX * TILE_PX];
    for (unsigned y = 0; y < TILE_PX; y++)
        for (unsigned x = 0; x < TILE_PX; x++) {
            const unsigned p = hd_tile_pixel(ppu, tileadr, character, bpp, x, y);
            /* Index 0 is transparent on every background layer, so it is
             * dumped as transparent rather than as whatever colour happens to
             * sit in that palette slot. */
            px[y * TILE_PX + x] = p ? (0xFF000000u | hd_colour(ppu, pal_base + p)) : 0u;
        }

    char path[1024];
    snprintf(path, sizeof path, "%s/%016llx.bmp", s_dump_dir,
             (unsigned long long)key);
    hd_save_bmp(path, px, TILE_PX, TILE_PX);

    /* Which frame a tile first appeared on is how a pack author picks one
     * screen out of a run that walked through several. */
    if (!s_dump_manifest) {
        char manifest[1024];
        snprintf(manifest, sizeof manifest, "%s/tiles.csv", s_dump_dir);
        s_dump_manifest = fopen(manifest, "w");
        if (s_dump_manifest)
            fprintf(s_dump_manifest, "tile,bpp,first_frame\n");
    }
    if (s_dump_manifest) {
        fprintf(s_dump_manifest, "%016llx,%d,%u\n",
                (unsigned long long)key, bpp, s_frame);
        fflush(s_dump_manifest);
    }
}

/* ---------------------------------------------------------------- pack --- */

static void hd_note_pack_name(const char *directory) {
    const char *base = directory;
    for (const char *p = directory; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    snprintf(s_pack_name, sizeof s_pack_name, "%s", base);
}

static void hd_add_texture(const char *directory, const char *filename) {
    uint64_t key = 0;
    int digits = 0;
    for (const char *p = filename; *p && *p != '.'; p++, digits++) {
        unsigned v;
        if (*p >= '0' && *p <= '9') v = (unsigned)(*p - '0');
        else if (*p >= 'a' && *p <= 'f') v = (unsigned)(*p - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F') v = (unsigned)(*p - 'A' + 10);
        else return;                            /* not one of ours */
        key = (key << 4) | v;
    }
    if (digits != 16 || !key) return;

    char path[1024];
    snprintf(path, sizeof path, "%s/%s", directory, filename);
    int size = 0;
    uint32_t *px = hd_load_bmp(path, &size);
    if (!px) {
        fprintf(stderr, "[HD] %s is not a square 32-bit BMP whose edge is a "
                        "multiple of 8. Skipped.\n", filename);
        return;
    }
    if ((size_t)(s_texture_count + 1) * 10u > (s_table_mask + 1) * 7u)
        if (!hd_table_grow()) { free(px); return; }

    HdTexture *slot = hd_table_slot(key);
    if (slot->key) {
        /* A later pack in the stack wins, so a small pack can override a
         * few tiles of a big one without duplicating it. */
        free(slot->pixels);
    } else {
        slot->key = key;
        s_texture_count++;
    }
    slot->pixels = px;
    slot->size = size;
}

/* Packs found under mods/. Names only: the directory is rebuilt when one is
 * chosen, so a pack can be dropped in and picked without editing a config. */
#define HD_MAX_PACKS 32
static char s_available[HD_MAX_PACKS][64];
static int  s_available_count;

static bool hd_dir_has_tiles(const char *dir) {
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof pattern, "%s\\*.bmp", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return true;
#else
    DIR *d = opendir(dir);
    if (!d) return false;
    bool found = false;
    struct dirent *e;
    while (!found && (e = readdir(d)) != NULL) {
        const char *dot = strrchr(e->d_name, '.');
        if (dot && (strcmp(dot, ".bmp") == 0 || strcmp(dot, ".BMP") == 0))
            found = true;
    }
    closedir(d);
    return found;
#endif
}

static void hd_consider_pack(const char *mods_dir, const char *name) {
    if (s_available_count >= HD_MAX_PACKS) return;
    if (name[0] == '.') return;
    char full[1024];
    snprintf(full, sizeof full, "%s/%s", mods_dir, name);
    if (!hd_dir_has_tiles(full)) return;
    snprintf(s_available[s_available_count], sizeof s_available[0], "%s", name);
    s_available_count++;
}

int issd_hd_scan_packs(const char *mods_dir) {
    s_available_count = 0;
    if (!mods_dir || !mods_dir[0]) return 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof pattern, "%s\\*", mods_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                hd_consider_pack(mods_dir, fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(mods_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) hd_consider_pack(mods_dir, e->d_name);
        closedir(d);
    }
#endif
    return s_available_count;
}

int issd_hd_available_count(void) { return s_available_count; }

const char *issd_hd_available_name(int index) {
    return (index >= 0 && index < s_available_count) ? s_available[index] : "";
}

/* Which packs are switched on, and in what order they are stacked. Order is
 * a small integer per pack rather than a separate list, so a pack that
 * disappears from mods/ simply stops being enabled. */
static int s_enable_order[HD_MAX_PACKS];
static int s_next_enable_order = 1;

void issd_hd_set_enabled(int index, bool enabled) {
    if (index < 0 || index >= HD_MAX_PACKS) return;
    if ((s_enable_order[index] != 0) == enabled) return;
    s_enable_order[index] = enabled ? s_next_enable_order++ : 0;
}

bool issd_hd_is_enabled(int index) {
    return index >= 0 && index < HD_MAX_PACKS && s_enable_order[index] != 0;
}

int issd_hd_enabled_count(void) {
    int n = 0;
    for (int i = 0; i < s_available_count; i++) if (s_enable_order[i]) n++;
    return n;
}

/* Index of the pack `slot` places into the stack, or -1 past the end. */
static int hd_pack_at_order(int slot) {
    int best = 0, seen = 0;
    for (;;) {
        int next = -1;
        for (int i = 0; i < s_available_count; i++) {
            if (!s_enable_order[i] || s_enable_order[i] <= best) continue;
            if (next < 0 || s_enable_order[i] < s_enable_order[next]) next = i;
        }
        if (next < 0) return -1;
        if (seen == slot) return next;
        best = s_enable_order[next];
        seen++;
    }
}

void issd_hd_enabled_list(char *out, size_t cap) {
    if (!out || !cap) return;
    out[0] = '\0';
    size_t used = 0;
    for (int slot = 0; ; slot++) {
        const int i = hd_pack_at_order(slot);
        if (i < 0) break;
        const size_t len = strlen(s_available[i]);
        if (used + len + (used ? 1u : 0u) >= cap) break;
        if (used) out[used++] = '|';
        memcpy(out + used, s_available[i], len);
        used += len;
        out[used] = '\0';
    }
}

void issd_hd_enable_from_list(const char *list) {
    memset(s_enable_order, 0, sizeof s_enable_order);
    s_next_enable_order = 1;
    if (!list || !list[0]) return;
    const char *p = list;
    while (*p) {
        const char *end = strchr(p, '|');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        for (int i = 0; i < s_available_count; i++)
            if (strlen(s_available[i]) == len &&
                strncmp(s_available[i], p, len) == 0) {
                issd_hd_set_enabled(i, true);
                break;
            }
        if (!end) break;
        p = end + 1;
    }
}

int issd_hd_apply(const char *mods_dir) {
    issd_hd_clear();
    if (!mods_dir || !mods_dir[0]) mods_dir = "mods";
    for (int slot = 0; ; slot++) {
        const int i = hd_pack_at_order(slot);
        if (i < 0) break;
        char dir[512];
        snprintf(dir, sizeof dir, "%s/%s", mods_dir, s_available[i]);
        issd_hd_add_pack(dir);
    }
    /* The stack's name is what the menu shows when only one pack is on; with
     * several, the count is the honest summary. */
    const int n = issd_hd_enabled_count();
    if (n > 1) snprintf(s_pack_name, sizeof s_pack_name, "%d packs", n);
    return s_texture_count;
}

void issd_hd_clear(void) {
    hd_table_free();
    s_pack_name[0] = '\0';
}

int issd_hd_load_pack(const char *directory) {
    issd_hd_clear();
    return directory && directory[0] ? issd_hd_add_pack(directory) : 0;
}

int issd_hd_add_pack(const char *directory) {
    if (!directory || !directory[0]) return 0;
    if (!s_table && !hd_table_alloc(1024)) return 0;
    const int before = s_texture_count;

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof pattern, "%s\\*.bmp", directory);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                hd_add_texture(directory, fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(directory);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            const char *dot = strrchr(e->d_name, '.');
            if (dot && (strcmp(dot, ".bmp") == 0 || strcmp(dot, ".BMP") == 0))
                hd_add_texture(directory, e->d_name);
        }
        closedir(d);
    }
#endif

    if (s_texture_count > before) {
        hd_note_pack_name(directory);
        printf("[HD] %s: %d tile(s), %d in the stack.\n", directory,
               s_texture_count - before, s_texture_count);
    } else {
        printf("[HD] No replacement tiles in '%s'.\n", directory);
    }
    return s_texture_count;
}

bool issd_hd_active(void) { return s_texture_count > 0; }
int  issd_hd_texture_count(void) { return s_texture_count; }
const char *issd_hd_pack_name(void) { return s_pack_name; }
int issd_hd_last_frame_hits(void) { return s_last_hits; }

void issd_hd_set_dump_start(unsigned frame) { s_dump_start = frame; }

void issd_hd_set_dump_dir(const char *directory) {
    free(s_dump_dir);
    s_dump_dir = NULL;
    free(s_dumped);
    s_dumped = NULL;
    if (s_dump_manifest) { fclose(s_dump_manifest); s_dump_manifest = NULL; }
    s_dump_full_warned = false;
    s_dumped_mask = 0;
    s_dumped_count = 0;
    if (directory && directory[0]) {
        hd_mkdir(directory);                   /* already existing is fine */
        s_dump_dir = (char *)malloc(strlen(directory) + 1);
        if (s_dump_dir) strcpy(s_dump_dir, directory);
    }
}

/* --------------------------------------------------------------- frame --- */

void issd_hd_begin_frame(void) {
    s_frame++;
    for (int i = 0; i < MAX_LINES; i++) s_lines[i].valid = 0;
}

void issd_hd_note_line(const Ppu *ppu, int line) {
    /* ppu_runLine(line) draws screen row line - 1. */
    const int row = line - 1;
    if (!ppu || row < 0 || row >= MAX_LINES) return;
    HdLineRegs *r = &s_lines[row];
    r->bgmode = ppu->bgmode;
    memcpy(r->bgXsc, ppu->bgXsc, sizeof r->bgXsc);
    r->bgTileAdr = ppu->bgTileAdr;
    for (int i = 0; i < 4; i++) {
        r->hScroll[i] = ppu->hScroll[i];
        r->vScroll[i] = ppu->vScroll[i];
    }
    r->mainLayers = ppu->screenEnabled[0];
    r->valid = 1;
}

/* Read a tilemap entry the way the PPU does, including the 32x32 screen
 * paging that a wider or higher tilemap adds. */
static uint16_t hd_tilemap_entry(const Ppu *ppu, const HdLineRegs *r, int layer,
                                 unsigned sx, unsigned sy, unsigned tile_shift) {
    const unsigned page_shift = tile_shift + 5;
    int sc = (int)((r->bgXsc[layer] & 0xFC) << 8);
    sc += (int)(((sy >> tile_shift) & 31u) << 5);
    if (((sy >> page_shift) & 1u) && (r->bgXsc[layer] & 0x2))
        sc += (r->bgXsc[layer] & 0x1) ? 0x800 : 0x400;
    if (((sx >> page_shift) & 1u) && (r->bgXsc[layer] & 0x1))
        sc += 0x400;
    sc += (int)((sx >> tile_shift) & 31u);
    return ppu->vram[sc & 0x7FFF];
}

/* One walk serves both jobs. With `hi` NULL nothing is drawn and the pass only
 * records which tiles the game put on screen, which is what authoring a pack
 * starts from; a dump then does not depend on a pack already existing. */
void issd_hd_dump_frame(const Ppu *ppu) {
    if (s_dump_dir) issd_hd_composite(ppu, NULL, 0, 0, NULL, 1, 0);
}

void issd_hd_composite(const Ppu *ppu,
                       const uint32_t *native, int native_w, int native_h,
                       uint32_t *hi, int scale, int margin_left) {
    s_last_hits = 0;
    if (!ppu || scale < 1) return;
    /* At 1x there is nothing to put the extra detail into, and sampling a
     * 4x tile down to one pixel would change how the game looks without
     * being asked to. */
    const bool drawing = (hi != NULL && native != NULL && scale >= 2);
    if (!drawing && !s_dump_dir) return;
    if (drawing && !s_table) return;
    if (native_w <= 0) native_w = 256;
    if (native_h <= 0) native_h = 224;
    s_generation++;

    const int hi_w = native_w * scale;

    for (int y = 0; y < native_h && y < MAX_LINES; y++) {
        const HdLineRegs *r = &s_lines[y];
        if (!r->valid) continue;
        const int mode = r->bgmode & 7;

        for (int layer = 0; layer < 4; layer++) {
            const int bpp = hd_layer_bpp(mode, layer);
            if (!bpp || !(r->mainLayers & (1 << layer))) continue;

            const bool big = (r->bgmode >> layer) & 0x10;
            const unsigned tile_shift = big ? 4u : 3u;
            const unsigned tile_mask = (1u << tile_shift) - 1u;
            const unsigned tileadr =
                (unsigned)(((r->bgTileAdr >> (layer * 4)) & 0xF) << 12);
            const unsigned sy = (unsigned)((unsigned)y + r->vScroll[layer]);

            /* Walk one tile at a time: a tile with no replacement costs one
             * lookup instead of eight. */
            int screen_x = 0;
            while (screen_x < 256) {
                const unsigned sx = (unsigned)screen_x + r->hScroll[layer];
                const uint16_t tile =
                    hd_tilemap_entry(ppu, r, layer, sx, sy, tile_shift);
                const unsigned run = tile_mask + 1u - (sx & tile_mask);
                int end = screen_x + (int)run;
                if (end > 256) end = 256;

                const unsigned pal_base = hd_palette_base(mode, layer, bpp, tile);
                unsigned py = sy & tile_mask;
                if (tile & 0x8000) py = tile_mask - py;
                unsigned character = tile & 0x3FFu;
                if (big) character = (character + ((py >> 3) << 4)) & 0x3FFu;

                /* The character only changes at an 8 pixel boundary, so the
                 * identity and the lookup are computed once per character
                 * rather than once per pixel. A tile with no replacement then
                 * costs one hash for its whole run. */
                unsigned cached_chr = 0x10000u;
                const HdTexture *tex = NULL;

                for (int x = screen_x; x < end; x++) {
                    unsigned px = ((unsigned)x + r->hScroll[layer]) & tile_mask;
                    if (tile & 0x4000) px = tile_mask - px;
                    unsigned chr = character;
                    if (big) chr = (chr + (px >> 3)) & 0x3FFu;

                    if (chr != cached_chr) {
                        cached_chr = chr;
                        /* The tag has to identify the tile exactly, not
                         * merely usually: overlapping these fields lets
                         * two different tiles share a slot and wear each
                         * other's texture. Each gets its own bits. */
                        const uint64_t tag =
                            (uint64_t)tileadr |
                            ((uint64_t)chr << 16) |
                            ((uint64_t)pal_base << 32) |
                            ((uint64_t)bpp << 40);
                        HdMemoEntry *m =
                            &s_memo[(tag ^ (tag >> 17)) & (HD_MEMO_SIZE - 1)];
                        if (m->generation != s_generation || m->tag != tag) {
                            m->generation = s_generation;
                            m->tag = tag;
                            m->key = hd_tile_key(ppu, tileadr, chr, bpp, pal_base);
                            m->tex = s_table ? (const void *)hd_lookup(m->key) : NULL;
                            if (s_dump_dir)
                                hd_dump_tile(ppu, m->key, tileadr, chr, bpp, pal_base);
                        }
                        tex = (const HdTexture *)m->tex;
                    }
                    if (!drawing || !tex) continue;

                    const unsigned pixel =
                        hd_tile_pixel(ppu, tileadr, chr, bpp, px & 7u, py & 7u);
                    if (!pixel) continue;       /* transparent: nothing to replace */

                    const int nx = margin_left + x;
                    if (nx < 0 || nx >= native_w) continue;
                    if ((native[(size_t)y * native_w + nx] & 0xFFFFFFu) !=
                        hd_colour(ppu, pal_base + pixel))
                        continue;               /* covered, or colour-mathed */

                    /* Sample the replacement. It is stored unflipped, so a
                     * flipped tile mirrors within the block as well. */
                    const int ts = tex->size / TILE_PX;
                    const unsigned tx = px & 7u, ty = py & 7u;
                    for (int sy2 = 0; sy2 < scale; sy2++) {
                        uint32_t *dst =
                            &hi[(size_t)(y * scale + sy2) * hi_w + (size_t)nx * scale];
                        int v = (sy2 * ts) / scale;
                        if (tile & 0x8000) v = ts - 1 - v;
                        const uint32_t *src =
                            &tex->pixels[(size_t)(ty * ts + v) * tex->size + tx * ts];
                        for (int sx2 = 0; sx2 < scale; sx2++) {
                            int u = (sx2 * ts) / scale;
                            if (tile & 0x4000) u = ts - 1 - u;
                            const uint32_t t = src[u];
                            if (t >> 24) dst[sx2] = t & 0xFFFFFFu;
                        }
                    }
                    s_last_hits++;
                }
                screen_x = end;
            }
        }
    }
}
