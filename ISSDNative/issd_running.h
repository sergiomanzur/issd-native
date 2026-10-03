#ifndef ISSD_RUNNING_H
#define ISSD_RUNNING_H
#include <stddef.h>
#include <stdint.h>
typedef struct Ppu Ppu;
/* Nest inside widescreen's presentation transaction. No game state is changed. */
unsigned issd_running_begin(Ppu *ppu, const uint8_t *ram,
                            const uint8_t *rom, size_t rom_size);
void issd_running_end(Ppu *ppu);
#endif
