#ifndef ISSD_SAVE_H
#define ISSD_SAVE_H

#include <stdint.h>
#include <stdbool.h>

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
