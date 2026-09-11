#ifndef ISSD_HD_H
#define ISSD_HD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* High resolution replacements for the cartridge's background tiles.
 *
 * The emulated PPU draws at 256x224 and the result is scaled up for the
 * window, so a replacement cannot simply be handed to the renderer. Instead
 * the background tilemaps are walked a second time after the frame is drawn,
 * and wherever a tile has a replacement its pixels are written into the
 * already-scaled buffer.
 *
 * A tile is identified by its own graphics data together with the colours it
 * is drawn in, so the same artwork in two palettes is two textures. Nothing
 * about the cartridge is modified: a pack is purely a set of images.
 */

typedef struct Ppu Ppu;

/* Point the loader at a directory of replacement tiles and load what is
 * there. NULL or an empty string turns replacement off. Safe to call again
 * to switch packs. Returns the number of textures loaded. */
int  issd_hd_load_pack(const char *directory);

/* True once a pack with at least one texture is loaded. */
bool issd_hd_active(void);

/* Find the pack directories under `mods_dir`: a subdirectory holding at
 * least one .bmp is a pack. Returns how many were found, so the menu can
 * offer them without the player typing a path. */
int         issd_hd_scan_packs(const char *mods_dir);
int         issd_hd_available_count(void);
const char *issd_hd_available_name(int index);

/* Name of the loaded pack directory, or "" when none is loaded. */
const char *issd_hd_pack_name(void);

/* Write every background tile the game draws into `directory` as an 8x8
 * 32-bit BMP named after its identity, so a pack can be authored from the
 * real thing. NULL stops dumping. */
void issd_hd_set_dump_dir(const char *directory);

/* Ignore everything before this frame when dumping, so a run that walks
 * through several screens can yield a pack for just the last one. */
void issd_hd_set_dump_start(unsigned frame);

/* Record the registers a scanline will be drawn with. Call once per line,
 * after that line's HDMA has been applied and before the line is rendered;
 * `line` is the PPU's line counter, which draws screen row line - 1. The
 * title screen changes background mode partway down the frame, so the state
 * at the end of the frame does not describe the top of it. */
void issd_hd_begin_frame(void);
void issd_hd_note_line(const Ppu *ppu, int line);

/* Composite replacements into an already-scaled frame.
 *
 * `native` is the frame as the PPU drew it, `native_w` x `native_h`, with
 * `margin_left` widescreen columns before the authentic 256. `hi` is that
 * frame scaled up by `scale`. Only pixels that still show the tile the
 * tilemap says is there are replaced, so sprites and layers drawn on top of
 * a background survive untouched. */
void issd_hd_composite(const Ppu *ppu,
                       const uint32_t *native, int native_w, int native_h,
                       uint32_t *hi, int scale, int margin_left);

/* Walk the frame's tilemaps for dumping only, without drawing anything.
 * A dump must not need a pack to already exist. */
void issd_hd_dump_frame(const Ppu *ppu);

/* Tiles replaced in the last composited frame, for the status line. */
int issd_hd_last_frame_hits(void);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_HD_H */
