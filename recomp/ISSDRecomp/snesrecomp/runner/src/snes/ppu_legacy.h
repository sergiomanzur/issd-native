#pragma once

#include "ppu.h"

/*
 * Compatibility scanline renderer.
 *
 * Selection belongs to Ppu::renderFlags; game hosts should never provide a
 * parallel renderer-selection global. This declaration stays private to the
 * PPU implementation even though the legacy renderer has its own translation
 * unit.
 */
void ppu_draw_whole_line_legacy(Ppu *ppu, int line);
