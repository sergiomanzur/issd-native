#ifndef ISSD_MATCH_H
#define ISSD_MATCH_H
#include "issd_config.h"
#include <stddef.h>

enum { ISSD_MATCH_CAPTURE_SETUP = 1, ISSD_MATCH_CAPTURE_KICKOFF = 2 };
void issd_match_init(uint8_t *ram, const IssdConfig *config,
                     size_t (*capture)(void *, size_t),
                     bool (*restore)(const void *, size_t));
/* Resident loads keep the compatible main-menu checkpoint. */
void issd_match_reset(void);
/* Cartridge/gameplay changes invalidate every retained checkpoint. */
void issd_match_reset_context(void);
/* Called only after completed frames. Exhibition shortcuts stay restricted;
 * pause Restart also records competition kickoffs without changing rules. */
int issd_match_tick(bool healthy);
bool issd_match_has_setup(void);
bool issd_match_has_kickoff(void);
bool issd_match_has_drill(void);
bool issd_match_is_live(void);
bool issd_match_frame_healthy(void);
bool issd_match_rematch(void);
bool issd_match_can_restart(void);
bool issd_match_restart(void);
bool issd_match_has_main_menu(void);
bool issd_match_at_main_menu(void);
bool issd_match_can_back_main(void);
/* Host advances original boot/menu inputs when no resident menu exists. */
void issd_match_set_main_menu_fallback(bool (*request)(void));
bool issd_match_back_main(void);
bool issd_match_mark_drill(bool healthy);
bool issd_match_restart_drill(void);
bool issd_match_start_rules(void);
bool issd_match_save_favorite(void);
bool issd_match_play_favorite(void);
const char *issd_match_error(void);
void issd_match_selected_rules(const IssdConfig *config, IssdMatchRules *rules);
#endif
