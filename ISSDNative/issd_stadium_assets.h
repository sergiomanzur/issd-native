#ifndef ISSD_STADIUM_ASSETS_H
#define ISSD_STADIUM_ASSETS_H
#include "issd_stadium_geometry.h"
typedef struct IssdStadiumTileWrite {
    uint16_t word_destination;
    size_t size;
    uint8_t *data;
} IssdStadiumTileWrite;
typedef struct IssdStadiumHdAsset {
    uint64_t key;
    uint8_t content_digest[32];
    char filename[4096];
} IssdStadiumHdAsset;
typedef struct IssdStadiumAssets {
    IssdStadiumGeometry geometry;
    IssdStadiumTileWrite *writes;
    size_t write_count;
    uint8_t palette[96];
    size_t palette_size;
    IssdStadiumHdAsset *hd;
    size_t hd_count;
} IssdStadiumAssets;
/* On failure *output is unchanged. A successful load replaces/frees its prior
 * malloc-owned compilation; initialize *output to NULL before first use. */
bool issd_stadium_assets_load(const char *manifest_path, unsigned expected_base,
                              IssdStadiumAssets **output);
void issd_stadium_assets_free(IssdStadiumAssets *assets);
#endif
