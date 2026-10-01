#ifndef ISSD_SNAPSHOT_H
#define ISSD_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "snes/saveload.h"

void issd_snapshot_save_extra(SaveLoadInfo *sli);
void issd_snapshot_load_extra(SaveLoadInfo *sli, uint32_t version);
bool issd_snapshot_validate_extra(const void *data, size_t size, uint32_t version);
void issd_snapshot_on_loaded(uint32_t version);

#endif
