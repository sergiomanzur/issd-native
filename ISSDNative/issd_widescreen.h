#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct Ppu Ppu;

/* Presentation transaction: call End after scanout, before executing game code. */
bool issd_widescreen_begin(Ppu *ppu, const uint8_t *ram,
                          const uint8_t *rom, size_t rom_size, int extra);
void issd_widescreen_end(Ppu *ppu);
bool issd_widescreen_pitch_layout(const Ppu *ppu, const uint8_t *ram);
