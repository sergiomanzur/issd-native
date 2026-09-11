#ifndef ISSD_SAVE_H
#define ISSD_SAVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>   /* size_t: MSVC pulls this in transitively, glibc does not */

#ifdef __cplusplus
extern "C" {
#endif

#define ISSD_MAX_SAVE_SLOTS 8

typedef struct {
    uint32_t magic;         /* 0x44535349 'ISSD' */
    uint32_t version;       /* Save format version */
    uint64_t timestamp;     /* Unix timestamp */
    uint32_t frame_counter; /* In-game total frames */
    
    /* Game State Mirrors */
    uint8_t  game_mode1;
    uint8_t  game_mode2;
    uint8_t  p1_team;
    uint8_t  p2_team;
    uint8_t  p1_score;
    uint8_t  p2_score;
    uint16_t match_seconds_remaining;

    /* Tournament & Scenario Metadata */
    uint8_t  tournament_stage;
    uint8_t  difficulty;
    char     slot_label[32];

    /* Full SNES WRAM snapshot (128 KB) */
    uint8_t  wram[0x20000];

    /* PPU snapshot region. WRAM alone describes the simulation but not the
     * picture: palettes, sprites and tiles are streamed by the game as it
     * walks its menus, so a slot restored at boot would render garbage
     * without them. Mirrors the snapshot block delimited in snes/ppu.h. */
    uint16_t cgram[0x100];
    uint16_t oam[0x100];
    uint8_t  high_oam[0x20];
    uint16_t vram[0x8000];
} IssdSaveSlot;

bool issd_save_init(void);
bool issd_save_to_slot(int slot_index, const char *label);
bool issd_load_from_slot(int slot_index);
bool issd_save_quick(void);
bool issd_load_quick(void);
bool issd_save_get_info(int slot_index, char *out_info, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_SAVE_H */
