#ifndef ISSD_STADIUM_SCENE_H
#define ISSD_STADIUM_SCENE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct Cart;
struct Ppu;
/* Cart borrows the scene allocation; reset detaches before freeing it. */
void issd_stadium_scene_reset(struct Cart *cart);
/* Rebuild host-only view after a snapshot load; guest RAM is read-only. */
bool issd_stadium_scene_restore(struct Cart *cart, const uint8_t ram[0x20000],
                                const uint8_t *canonical, size_t size);
bool issd_stadium_scene_opcode(struct Cart *cart, uint8_t ram[0x20000],
                               const uint8_t *canonical, size_t size, uint32_t pc);
void issd_stadium_scene_transfer(struct Ppu *ppu, uint8_t ram[0x20000], uint32_t pc);
void issd_stadium_scene_art_identity(const uint8_t ram[0x20000], uint8_t identity[16]);
void issd_stadium_scene_saved_art(const uint8_t identity[16], bool known);
void issd_stadium_scene_refresh_art(struct Ppu *ppu, uint8_t ram[0x20000]);
#endif
