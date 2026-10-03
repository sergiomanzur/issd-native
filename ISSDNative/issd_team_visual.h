#ifndef ISSD_TEAM_VISUAL_H
#define ISSD_TEAM_VISUAL_H
#include <stddef.h>
#include <stdint.h>
enum { ISSD_TEAM_VISUAL_PARTS = 64 };
typedef struct {
    int x,y,size;
    uint16_t tile_attributes;
} IssdTeamVisualPart;
typedef struct {
    int team, actor, x,y,width,height,part_count,flag;
    IssdTeamVisualPart parts[ISSD_TEAM_VISUAL_PARTS];
} IssdTeamVisual;
/* Original actor list and reorganized OAM descriptors are read only.
 * Caller supplies the RAM image corresponding to the displayed OAM frame. */
size_t issd_team_visual_collect(const uint8_t *ram,const uint8_t *rom,
                               size_t rom_size,IssdTeamVisual *out,size_t capacity);
/* Return the exact matching OAM indices. Never suppress by rectangle alone. */
size_t issd_team_visual_match_oam(const IssdTeamVisual *visual,
                                const uint16_t oam[256],const uint8_t high[32],
                                uint8_t indices[ISSD_TEAM_VISUAL_PARTS]);
struct Ppu;
void issd_team_visual_begin(struct Ppu *ppu,const uint8_t *ram,const uint8_t *rom,size_t rom_size);
void issd_team_visual_render(uint32_t *fb,int width,int height,int margin);
void issd_team_visual_end(struct Ppu *ppu);
#endif
