#ifndef ISSD_SNAPSHOT_H
#define ISSD_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "snes/saveload.h"
#include "issd_animation.h"
#include "issd_pose_history.h"
#include "issd_widescreen.h"
#define ISSD_SNAPSHOT_LEGACY_PRESENTATION_SIZE (128 + ISSD_ANIMATION_STATE_SIZE + ISSD_POSE_HISTORY_STATE_SIZE)
#define ISSD_SNAPSHOT_EXTRA_SIZE (ISSD_SNAPSHOT_LEGACY_PRESENTATION_SIZE + ISSD_REPLAY_HISTORY_STATE_SIZE)

void issd_snapshot_save_extra(SaveLoadInfo *sli);
void issd_snapshot_load_extra(SaveLoadInfo *sli, uint32_t version);
bool issd_snapshot_validate_extra(const void *data, size_t size, uint32_t version);
void issd_snapshot_on_loaded(uint32_t version);

#endif
