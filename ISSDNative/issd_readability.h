#pragma once
#include "issd_config.h"

/* Read-only, live-match presentation. Supply the object generation used by
 * native scanout; coordinates include the widescreen left margin. The top 24
 * scanlines remain reserved for the cartridge HUD. Selected-player labels and
 * markers use hud_scale and avoid one another and both radar rectangles;
 * labels are omitted if the surface has no free space.
 * Radar 1x preserves the cartridge pixels (position/opacity apply to 2x/3x).
 * Enlarged maps fit the surface proportionally and blend their background;
 * moving one leaves the cartridge radar visible because this read-only stage
 * cannot reconstruct the pitch hidden underneath it. */
void issd_readability_render(uint32_t *framebuffer, int width, int height,
                             int margin, const uint8_t *ram,
                             const IssdConfig *config);
bool issd_readability_player_name(const uint8_t *ram, unsigned actor, char name[9]);
