#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct Ppu Ppu;
typedef struct { int first,last,top,bottom; unsigned layer; } IssdWorldLayer;
typedef struct { int x,y,size; uint16_t tile; uint8_t attributes; } IssdCameraPiece;
typedef struct IssdCameraView IssdCameraView;
bool issd_widescreen_camera_pieces(Ppu *ppu,const uint8_t *ram,const uint8_t *rom,
    size_t rom_size,const IssdCameraView *view,IssdCameraPiece *pieces,size_t capacity,size_t *count);
bool issd_widescreen_world_layer(const uint8_t *ram,unsigned layer,IssdWorldLayer *out);
bool issd_widescreen_world_sample(const uint8_t *ram,const IssdWorldLayer *layer,
                                 int x,int y,uint16_t *tile);

/* A 512-pixel tilemap ring must also accommodate partial edge tiles. 504
 * pixels (124 per side) fits at every eight-pixel scroll phase. */
#define ISSD_WIDESCREEN_MAX_EXTRA 124

/* Presentation transaction: call End after scanout, before executing game code. */
bool issd_widescreen_begin(Ppu *ppu, const uint8_t *ram,
                          const uint8_t *rom, size_t rom_size, int extra);
void issd_widescreen_end(Ppu *ppu);
/* Stable native-OAM generation for every Begin, including classic 4:3. The
 * pointer is read-only and valid until the next Begin/reset; current is the
 * fallback before a snapshot exists. */
const uint8_t *issd_widescreen_presented_ram(const uint8_t *current);
bool issd_widescreen_pitch_layout(const Ppu *ppu, const uint8_t *ram);
/* Verified pre-match/coin BG1 crowd-edge continuation; no pitch world maps. */
bool issd_widescreen_coin_layout(const Ppu *ppu, const uint8_t *ram);
/* Original halftime/fulltime card over the loaded stadium background. */
bool issd_widescreen_stats_layout(const Ppu *ppu, const uint8_t *ram);
/* True on a menu screen whose BG2 wallpaper can be repeated into the side
 * margins instead of pillarboxing them. */
bool issd_widescreen_menu_layout(const Ppu *ppu, const uint8_t *ram);
/* True on the title screen, whose margins take its backdrop colour rather
 * than black bars. */
bool issd_widescreen_title_layout(const Ppu *ppu, const uint8_t *ram);
bool Issd_IsWidescreenActive(void);
/* Drop the remembered previous-frame WRAM. Call whenever continuity with the
 * last presented frame is broken (state load, scene cut) so the supplement
 * reconstructs from the frame in hand instead of an unrelated one. */
void issd_widescreen_reset(void);
/* Checked snapshots restore animation separately; preserve it when rebasing. */
void issd_widescreen_rebase(Ppu *ppu, const uint8_t *ram);
/* Presentation-only companions of the original 512-entry replay directory.
 * Original recording/decoding and guest WRAM are never modified. */
#define ISSD_REPLAY_HISTORY_STATE_SIZE (4u + 512u * 448u)
void issd_replay_observe(const uint8_t *ram, uint32_t pc);
void issd_replay_reset(void);
void issd_replay_save_state(uint8_t *state);
bool issd_replay_validate_state(const uint8_t *state, size_t size);
bool issd_replay_load_state(const uint8_t *state, size_t size);
