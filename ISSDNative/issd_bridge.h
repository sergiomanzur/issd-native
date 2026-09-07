#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Game Mode IDs */
typedef enum {
    ISSD_MODE1_TITLE = 0x00,
    ISSD_MODE1_MAIN_MENU = 0x01,
    ISSD_MODE1_MATCH = 0x02,
} IssdGameMode1;

/* In-game Match State Structure */
typedef struct {
    uint8_t game_mode1;
    uint8_t game_mode2;
    uint8_t p1_score;
    uint8_t p2_score;
    uint8_t p1_team;
    uint8_t p2_team;
    uint8_t timer_minutes;
    uint8_t timer_seconds;
    uint8_t weather;
    uint8_t referee;
    uint8_t game_level;
    uint8_t foul_setting;
    uint8_t yellow_card_setting;
    uint8_t offside_setting;
    uint16_t camera_x;
    uint16_t camera_y;
    bool dog_referee_unlocked;
    bool all_stars_unlocked;
} IssdMatchState;

/* Bridge Functions */
void issd_bridge_init(void);
void issd_bridge_update_state(IssdMatchState* out_state);
void issd_bridge_log_state(const IssdMatchState* state);

#ifdef __cplusplus
}
#endif
