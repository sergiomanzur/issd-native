#ifndef ISSD_STADIUM_GEOMETRY_H
#define ISSD_STADIUM_GEOMETRY_H
#include "issd_stadium.h"
#include <stddef.h>
typedef struct IssdStadiumGeometry {
    uint16_t base_layout, length, width, stride;
    uint8_t metatiles[2][8192];
    uint8_t world_maps[2][4096];
} IssdStadiumGeometry;
bool issd_stadium_read_template(const uint8_t *canonical, size_t size,
                                unsigned base_layout, IssdStadiumGeometry *output);
bool issd_stadium_read_native_resources(const uint8_t *canonical,size_t size,
                                        unsigned base_layout,uint8_t vram[65536],
                                        uint8_t palette[512]);
/* Pure host compilation; failure leaves output unchanged. Never writes RAM. */
bool issd_stadium_geometry_compile(const IssdStadiumGeometry *original,
                                   const IssdStadiumProfile *profile,
                                   IssdStadiumGeometry *output);
#endif
