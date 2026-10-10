/* Replacement background tiles, on a synthetic PPU.
 *
 * Two things have to hold or a pack is worse than useless: a tile has to be
 * recognised from its own graphics and colours, and a pixel that something
 * else is drawn over must be left exactly as the game drew it.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "snes/ppu.h"
#include "issd_hd.h"

#define W 256
#define H 224
#define SCALE 4

/* A 4bpp tile whose columns run 0..7, so every colour index appears and a
 * wrong bit order cannot pass. */
static void write_test_tile(Ppu *ppu, unsigned tileadr, unsigned character) {
    const unsigned base = tileadr + character * 16u;
    for (unsigned row = 0; row < 8; row++) {
        uint16_t plane01 = 0, plane23 = 0;
        for (unsigned col = 0; col < 8; col++) {
            const unsigned value = col;              /* 0..7 across the tile */
            const unsigned bit = 7u - col;
            if (value & 1) plane01 |= (uint16_t)(1u << bit);
            if (value & 2) plane01 |= (uint16_t)(1u << (bit + 8));
            if (value & 4) plane23 |= (uint16_t)(1u << bit);
            if (value & 8) plane23 |= (uint16_t)(1u << (bit + 8));
        }
        ppu->vram[(base + row) & 0x7FFF] = plane01;
        ppu->vram[(base + row + 8) & 0x7FFF] = plane23;
    }
}

static uint32_t expected_colour(const Ppu *ppu, unsigned index) {
    const uint16_t c = ppu->cgram[index & 0xFF];
    return ((uint32_t)ppu->brightnessMult[c & 0x1F] << 16) |
           ((uint32_t)ppu->brightnessMult[(c >> 5) & 0x1F] << 8) |
            (uint32_t)ppu->brightnessMult[(c >> 10) & 0x1F];
}

/* The loader reads 32-bit top-down BMPs; write one the same way. */
static void write_texture(const char *path, int size, uint32_t argb) {
    uint8_t head[54];
    memset(head, 0, sizeof head);
    const uint32_t body = (uint32_t)size * (uint32_t)size * 4u;
    head[0] = 'B'; head[1] = 'M';
    const uint32_t filesize = 54u + body;
    memcpy(head + 2, &filesize, 4);
    const uint32_t offbits = 54u;
    memcpy(head + 10, &offbits, 4);
    const uint32_t hsize = 40u;
    memcpy(head + 14, &hsize, 4);
    const int32_t w = size, h = -size;
    memcpy(head + 18, &w, 4);
    memcpy(head + 22, &h, 4);
    const uint16_t planes = 1, bits = 32;
    memcpy(head + 26, &planes, 2);
    memcpy(head + 28, &bits, 2);
    memcpy(head + 34, &body, 4);

    FILE *f = fopen(path, "wb");
    assert(f);
    fwrite(head, 1, sizeof head, f);
    for (int i = 0; i < size * size; i++) fwrite(&argb, 4, 1, f);
    fclose(f);
}

/* Dump a frame that shows exactly one tile, and report the name it was
 * written under. That name is the identity the loader looks a pack up by,
 * so a test never has to reimplement how identity is computed. */
static void dump_single(Ppu *ppu, const char *dir, char *out, size_t cap) {
    issd_hd_set_dump_dir(dir);
    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);
    issd_hd_dump_frame(ppu);
    issd_hd_set_dump_dir(NULL);

    char manifest[1024];
    snprintf(manifest, sizeof manifest, "%s/tiles.csv", dir);
    FILE *f = fopen(manifest, "r");
    assert(f);
    char line[256];
    assert(fgets(line, sizeof line, f));                  /* header */
    assert(fgets(line, sizeof line, f));
    char *comma = strchr(line, ',');
    assert(comma);
    *comma = '\0';
    snprintf(out, cap, "%s", line);
    assert(!fgets(line, sizeof line, f) && "expected exactly one tile");
    fclose(f);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";

    Ppu *ppu = (Ppu *)calloc(1, sizeof(Ppu));
    assert(ppu);

    /* Mode 1: BG1 is 4bpp. One 32x32 tilemap of the same character. */
    ppu->bgmode = 1;
    ppu->bgXsc[0] = 0x00;                    /* tilemap at word 0, 32x32 */
    ppu->bgTileAdr = 0x0001;                 /* BG1 graphics at word 0x1000 */
    ppu->screenEnabled[0] = 0x01;            /* BG1 on the main screen */
    ppu->hScroll[0] = 0;
    ppu->vScroll[0] = 0;
    for (int i = 0; i < 32; i++) ppu->brightnessMult[i] = (uint8_t)((i << 3) | (i >> 2));
    memset(&ppu->brightnessMult[32], ppu->brightnessMult[31], 31);
    for (int i = 0; i < 256; i++) ppu->cgram[i] = (uint16_t)(i * 0x1111u);

    const unsigned tileadr = 0x1000, character = 3;
    write_test_tile(ppu, tileadr, character);
    for (int i = 0; i < 32 * 32; i++) ppu->vram[i] = (uint16_t)character;

    /* A frame in which the whole screen shows that tile. */
    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);

    uint32_t *native = (uint32_t *)malloc(sizeof(uint32_t) * W * H);
    uint32_t *hi = (uint32_t *)malloc(sizeof(uint32_t) * W * H * SCALE * SCALE);
    assert(native && hi);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            native[y * W + x] = expected_colour(ppu, (unsigned)(x & 7));

    /* One pixel column is "covered" by something the second walk knows
     * nothing about. It must survive. */
    const int covered_x = 40;
    const uint32_t sprite = 0x00123456u;
    for (int y = 0; y < H; y++) native[y * W + covered_x] = sprite;

    for (int i = 0; i < W * H * SCALE * SCALE; i++) hi[i] = 0u;

    /* With no pack loaded nothing may be written at all. */
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);
    for (int i = 0; i < W * H * SCALE * SCALE; i++) assert(hi[i] == 0u);
    assert(issd_hd_last_frame_hits() == 0);

    /* Now give colour index 5 of that tile a replacement. Index 0 is
     * transparent on a background layer and must never be replaced. */
    char path[1024];
    snprintf(path, sizeof path, "%s/pack", dir);

    /* Find the identity the walk computes by dumping this one tile. Setting
     * the dump directory creates it, which is also where the pack will go. */
    issd_hd_set_dump_dir(path);
    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);
    issd_hd_dump_frame(ppu);
    issd_hd_set_dump_dir(NULL);

    /* Exactly one tile is on screen, so exactly one file was written. Reuse
     * its name: that name is the identity the loader will look up. */
    char dumped[1024] = {0};
    {
        char manifest[1024];
        snprintf(manifest, sizeof manifest, "%s/tiles.csv", path);
        FILE *f = fopen(manifest, "r");
        assert(f);
        char line[256];
        assert(fgets(line, sizeof line, f));          /* header */
        assert(fgets(line, sizeof line, f));
        char *comma = strchr(line, ',');
        assert(comma);
        *comma = '\0';
        snprintf(dumped, sizeof dumped, "%s", line);
        fclose(f);
    }

    const uint32_t replacement = 0xFF00FF00u;         /* opaque green */
    snprintf(path, sizeof path, "%s/pack/%s.bmp", dir, dumped);
    write_texture(path, 8 * SCALE, replacement);

    snprintf(path, sizeof path, "%s/pack", dir);
    assert(issd_hd_load_pack(path) == 1);
    assert(issd_hd_active());

    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);
    assert(issd_hd_last_frame_hits() > 0);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const uint32_t *block = &hi[(size_t)(y * SCALE) * (W * SCALE) + (size_t)x * SCALE];
            if (x == covered_x) {
                assert(block[0] == 0u && "a covered pixel must not be replaced");
            } else if ((x & 7) == 0) {
                assert(block[0] == 0u && "a transparent pixel must not be replaced");
            } else {
                assert(block[0] == (replacement & 0xFFFFFFu));
                assert(block[SCALE - 1] == (replacement & 0xFFFFFFu));
            }
        }
    }

    /* The compositor runs after temporary presentation VRAM is restored.
     * It must use the tilemap, graphics, palette and brightness at scanout. */
    {
        uint64_t key = strtoull(dumped, NULL, 16);
        char local_red[1024], local_blue[1024];
        snprintf(local_red,sizeof local_red,"%s/local-red.bmp",dir);
        snprintf(local_blue,sizeof local_blue,"%s/local-blue.bmp",dir);
        write_texture(local_red,32,0xffff0000u);
        write_texture(local_blue,32,0xff0000ffu);
        const char *red_files[] = {local_red}, *blue_files[] = {local_blue};
        assert(issd_hd_load_stadium(8,1,&key,red_files,1));
        assert(issd_hd_load_stadium(9,1,&key,blue_files,1));
        issd_hd_begin_frame();
        issd_hd_set_stadium_context(8,1); issd_hd_note_line(ppu,1);
        issd_hd_set_stadium_context(9,1); issd_hd_note_line(ppu,2);
        issd_hd_set_stadium_context(-1,0);
        memset(hi,0,sizeof(uint32_t)*W*H*SCALE*SCALE);
        issd_hd_composite(ppu,native,W,H,hi,SCALE,0);
        assert(hi[5*SCALE] == 0xff0000u);
        assert(hi[W*SCALE*SCALE+5*SCALE] == 0x0000ffu);
        assert(hi[0] == 0 && hi[covered_x*SCALE] == 0);
        issd_hd_begin_frame();
        issd_hd_set_stadium_context(8,2); issd_hd_note_line(ppu,1);
        memset(hi,0,sizeof(uint32_t)*W*H*SCALE*SCALE);
        issd_hd_composite(ppu,native,W,H,hi,SCALE,0);
        assert(hi[5*SCALE] == (replacement&0xffffffu));
        assert(issd_hd_load_stadium(8,2,&key,blue_files,1));
        issd_hd_begin_frame(); issd_hd_note_line(ppu,1);
        memset(hi,0,sizeof(uint32_t)*W*H*SCALE*SCALE);
        issd_hd_composite(ppu,native,W,H,hi,SCALE,0);
        assert(hi[5*SCALE] == 0xffu);
        assert(issd_hd_load_pack(NULL) == 0 && issd_hd_active());
        issd_hd_begin_frame(); issd_hd_note_line(ppu,1);
        memset(hi,0,sizeof(uint32_t)*W*H*SCALE*SCALE);
        issd_hd_composite(ppu,native,W,H,hi,SCALE,0);
        assert(hi[5*SCALE] == 0xffu);
        issd_hd_clear_stadiums();
        assert(!issd_hd_active());
        snprintf(path,sizeof path,"%s/pack",dir);
        assert(issd_hd_load_pack(path) == 1);
    }
    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);
    uint16_t saved_map = ppu->vram[0];
    uint16_t saved_art = ppu->vram[tileadr + character * 16u];
    uint16_t saved_palette = ppu->cgram[5];
    uint8_t saved_brightness = ppu->brightnessMult[31];
    ppu->vram[0] = 100;
    ppu->vram[tileadr + character * 16u] = 0;
    ppu->cgram[5] ^= 0x7FFF;
    ppu->brightnessMult[31] = 0;
    memset(hi, 0, sizeof(uint32_t) * W * H * SCALE * SCALE);
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);
    assert(hi[5 * SCALE] == (replacement & 0xFFFFFFu) &&
           "scanout texture must survive presentation state restoration");
    ppu->vram[0] = saved_map;
    ppu->vram[tileadr + character * 16u] = saved_art;
    ppu->cgram[5] = saved_palette;
    ppu->brightnessMult[31] = saved_brightness;

    /* A mid-frame palette/VRAM change must not retroactively alter the
     * first line, or inherit that first line's memoized texture identity. */
    issd_hd_begin_frame();
    issd_hd_note_line(ppu, 1);
    ppu->cgram[5] ^= 0x7FFF;
    ppu->vram[tileadr + character * 16u] ^= 1;
    issd_hd_note_line(ppu, 2);
    ppu->cgram[5] = saved_palette;
    ppu->vram[tileadr + character * 16u] = saved_art;
    memset(hi, 0, sizeof(uint32_t) * W * H * SCALE * SCALE);
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);
    assert(hi[5 * SCALE] == (replacement & 0xFFFFFFu));
    assert(hi[(size_t)SCALE * W * SCALE + 5 * SCALE] == 0 &&
           "later scanline must use its own tile identity");

    /* Matching RGB is not proof of BG ownership: a sprite can have the
     * exact same green as the pitch and still must retain its original art. */
    issd_hd_begin_frame();
    ppu->renderFlags = kPpuRenderFlags_NewRenderer;
    for (int line = 1; line <= H; line++) {
        issd_hd_note_line(ppu, line);
        for (int x = 0; x < kPpuBufWidth; x++)
            ppu->bgBuffers[0].data[x] = 0x1000; /* BG1 owns main pixels */
        ppu->bgBuffers[0].data[kPpuExtraLeftRight + 5] = 0x2405; /* OBJ */
        issd_hd_note_rendered_line(ppu, line);
    }
    memset(hi, 0, sizeof(uint32_t) * W * H * SCALE * SCALE);
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);
    assert(hi[5 * SCALE] == 0 && "same-colour sprite must retain ownership");
    assert(hi[6 * SCALE] == (replacement & 0xFFFFFFu));
    ppu->renderFlags = 0;

    /* HD pitch detail must continue into both widened margins. */
    {
        enum { EXTRA = 8, WIDE = W + 2 * EXTRA };
        uint32_t wide_native[WIDE];
        uint32_t wide_hi[WIDE * SCALE * SCALE];
        for (int x = 0; x < WIDE; x++)
            wide_native[x] = expected_colour(ppu, (unsigned)(x - EXTRA) & 7);
        memset(wide_hi, 0, sizeof wide_hi);
        issd_hd_begin_frame();
        issd_hd_note_line(ppu, 1);
        issd_hd_composite(ppu, wide_native, WIDE, 1, wide_hi, SCALE, EXTRA);
        assert(wide_hi[5 * SCALE] == (replacement & 0xFFFFFFu) &&
               "left margin must receive HD pitch detail");
        assert(wide_hi[(WIDE - 3) * SCALE] == (replacement & 0xFFFFFFu) &&
               "right margin must receive HD pitch detail");
    }

    /* At 1x there is nothing to gain and the frame must be left alone. */
    for (int i = 0; i < W * H; i++) hi[i] = 0u;
    issd_hd_composite(ppu, native, W, H, hi, 1, 0);
    for (int i = 0; i < W * H; i++) assert(hi[i] == 0u);

    /* Changing the palette changes the identity, so the same graphics in
     * different colours do not silently reuse one texture. */
    ppu->cgram[5] ^= 0x7FFFu;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            native[y * W + x] = expected_colour(ppu, (unsigned)(x & 7));
    for (int i = 0; i < W * H * SCALE * SCALE; i++) hi[i] = 0u;
    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);
    assert(issd_hd_last_frame_hits() == 0);

    /* Unloading really unloads. */
    assert(issd_hd_load_pack(NULL) == 0);
    assert(!issd_hd_active());

    /* --- two tiles that once shared a cache slot ------------------------
     *
     * Identity is computed once per tile and memoised for the frame. An
     * early version packed the memo's key into overlapping bits, so a tile
     * at character 4 in palette 0 and one at character 0 in palette 1
     * landed in the same slot and the second wore the first one's texture.
     * On the pitch that turned the team name in the HUD into garbage.
     */
    ppu->cgram[1] = 0x001Fu;                  /* palette 0, index 1 */
    ppu->cgram[17] = 0x7C00u;                 /* palette 1, index 1 */
    for (unsigned chr = 0; chr <= 4; chr += 4) {
        const unsigned base = tileadr + chr * 16u;
        for (unsigned row = 0; row < 8; row++) {
            /* Low byte is bit plane 0, high byte plane 1: every pixel index 1.
             * The two characters carry the same art, so only the palette can
             * tell them apart - which is the point. */
            ppu->vram[(base + row) & 0x7FFF] = 0x00FFu;
            ppu->vram[(base + row + 8) & 0x7FFF] = 0x0000u;
        }
    }

    char name_a[64], name_b[64];
    {
        char dir_a[1024], dir_b[1024];
        snprintf(dir_a, sizeof dir_a, "%s/a", dir);
        snprintf(dir_b, sizeof dir_b, "%s/b", dir);
        /* One tile on screen at a time, so each dump names exactly one. */
        for (int i = 0; i < 32 * 32; i++) ppu->vram[i] = 4u;          /* chr 4, pal 0 */
        dump_single(ppu, dir_a, name_a, sizeof name_a);
        for (int i = 0; i < 32 * 32; i++) ppu->vram[i] = 0x0400u;     /* chr 0, pal 1 */
        dump_single(ppu, dir_b, name_b, sizeof name_b);
        assert(strcmp(name_a, name_b) != 0);
    }

    const uint32_t red = 0xFFFF0000u, blue = 0xFF0000FFu;
    snprintf(path, sizeof path, "%s/pair", dir);
    issd_hd_set_dump_dir(path);               /* creates the directory */
    issd_hd_set_dump_dir(NULL);
    snprintf(path, sizeof path, "%s/pair/%s.bmp", dir, name_a);
    write_texture(path, 8 * SCALE, red);
    snprintf(path, sizeof path, "%s/pair/%s.bmp", dir, name_b);
    write_texture(path, 8 * SCALE, blue);
    snprintf(path, sizeof path, "%s/pair", dir);
    assert(issd_hd_load_pack(path) == 2);

    /* Alternate the two across the screen so both are walked every line. */
    for (int i = 0; i < 32 * 32; i++) ppu->vram[i] = (i & 1) ? 0x0400u : 4u;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            native[y * W + x] = expected_colour(ppu, ((x >> 3) & 1) ? 17u : 1u);
    for (int i = 0; i < W * H * SCALE * SCALE; i++) hi[i] = 0u;

    issd_hd_begin_frame();
    for (int line = 1; line <= H; line++) issd_hd_note_line(ppu, line);
    issd_hd_composite(ppu, native, W, H, hi, SCALE, 0);

    for (int y = 0; y < H; y += 16) {
        for (int x = 0; x < W; x++) {
            const uint32_t got =
                hi[(size_t)(y * SCALE) * (W * SCALE) + (size_t)x * SCALE];
            const uint32_t want = ((x >> 3) & 1) ? (blue & 0xFFFFFFu)
                                                 : (red & 0xFFFFFFu);
            if (got != want)
                fprintf(stderr, "x=%d y=%d got=%06X want=%06X\n",
                        x, y, got, want);
            assert(got == want && "two tiles must not share one cache slot");
        }
    }

    /* Repeat margins use the authentic center's source coordinates even
     * when the hardware tilemap has a different second screen page. */
    {
        enum { EXTRA = 16, WIDE = W + 2 * EXTRA };
        uint32_t wide_native[WIDE], wide_hi[WIDE * SCALE * SCALE];
        ppu->bgXsc[0] = 1; /* 64 columns; second page is deliberately red */
        ppu->wsLayerRepeat = 1;
        for (int i = 0x400; i < 0x800; i++) ppu->vram[i] = 4;
        for (int x = 0; x < WIDE; x++) {
            unsigned source = (unsigned)(x - EXTRA) & 255u;
            wide_native[x] = expected_colour(ppu, (source & 8) ? 17u : 1u);
        }
        memset(wide_hi, 0, sizeof wide_hi);
        issd_hd_begin_frame();
        issd_hd_note_line(ppu, 1);
        issd_hd_composite(ppu, wide_native, WIDE, 1, wide_hi, SCALE, EXTRA);
        assert(wide_hi[13 * SCALE] == (blue & 0xFFFFFFu));
        assert(wide_hi[(W + EXTRA + 13) * SCALE] == (blue & 0xFFFFFFu));
        /* An unaligned scroll puts the final center tile run across the
         * right border, where repeat must restart at authentic x=0. */
        ppu->hScroll[0] = 73;
        for (int x = 0; x < WIDE; x++) {
            unsigned source = ((unsigned)(x - EXTRA) & 255u) + 73u;
            unsigned entry = source >= 256u ? 4u : ((source & 8) ? 0x0400u : 4u);
            wide_native[x] = expected_colour(ppu, entry == 0x0400u ? 17u : 1u);
        }
        memset(wide_hi, 0, sizeof wide_hi);
        issd_hd_begin_frame();
        issd_hd_note_line(ppu, 1);
        issd_hd_composite(ppu, wide_native, WIDE, 1, wide_hi, SCALE, EXTRA);
        assert(wide_hi[(W + EXTRA) * SCALE] == (blue & 0xFFFFFFu) &&
               "right repeat border must restart its tile lookup after unaligned scroll");
        ppu->hScroll[0] = 0;
        ppu->wsLayerRepeat = 0;
        ppu->bgXsc[0] = 0;
    }

    assert(issd_hd_load_pack(NULL) == 0);

    free(native);
    free(hi);
    free(ppu);
    puts("hd tests passed");
    return 0;
}
