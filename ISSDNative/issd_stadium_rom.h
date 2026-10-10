#ifndef ISSD_STADIUM_ROM_H
#define ISSD_STADIUM_ROM_H
#include <stddef.h>
#include <stdint.h>
#include "issd_stadium.h"

/* Owns the selected-scene allocation. Cart only borrows this buffer; callers
 * must detach its view before clearing or replacing the scene allocation. */
typedef struct IssdStadiumRom {
    uint8_t *data;
    size_t size;
} IssdStadiumRom;
bool issd_stadium_rom_build(IssdStadiumRom *scene, const uint8_t *canonical,
                            size_t size, const IssdStadiumProfile *profile);
void issd_stadium_rom_clear(IssdStadiumRom *scene);
#endif
