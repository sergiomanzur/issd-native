#ifndef ISSD_MATCH_H
#define ISSD_MATCH_H
#include "issd_config.h"
#include <stddef.h>

enum { ISSD_MATCH_CAPTURE_SETUP = 1, ISSD_MATCH_CAPTURE_KICKOFF = 2 };
void issd_match_init(uint8_t *ram, const IssdConfig *config,
                     size_t (*capture)(void *, size_t),
                     bool (*restore)(const void *, size_t));
void issd_match_reset(void);
/* Called only after completed frames. Never captures campaigns or demo play. */
int issd_match_tick(bool healthy);
bool issd_match_has_setup(void);
bool issd_match_has_kickoff(void);
bool issd_match_has_drill(void);
bool issd_match_is_live(void);
bool issd_match_frame_healthy(void);
bool issd_match_rematch(void);
bool issd_match_mark_drill(bool healthy);
bool issd_match_restart_drill(void);
bool issd_match_start_rules(void);
bool issd_match_save_favorite(void);
bool issd_match_play_favorite(void);
const char *issd_match_error(void);
void issd_match_selected_rules(const IssdConfig *config, IssdMatchRules *rules);
#endif
