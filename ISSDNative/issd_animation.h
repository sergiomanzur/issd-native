#ifndef ISSD_ANIMATION_H
#define ISSD_ANIMATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Portable little-endian records, including zeroed reserved bytes. */
#define ISSD_ANIMATION_STATE_SIZE (22u * 32u)
void issd_animation_save_state(uint8_t *state);
bool issd_animation_validate_state(const uint8_t *state, size_t size);
bool issd_animation_load_state(const uint8_t *state, size_t size);

void issd_animation_reset(void);
void issd_animation_forget(unsigned object);
/* Read-only presentation copy of $84E641/$84E683. True means the authored
 * script owns the result, including deliberate terminal holds. */
bool issd_animation_pose(unsigned object, const uint8_t *ram,
                         const uint8_t *rom, size_t rom_size,
                         bool native_window, uint16_t *descriptor);
/* Second half of a verified running keyframe, using the same presentation
 * phase as culled-player continuation. Does not advance clocks or guest state. */
bool issd_animation_running_pair(unsigned object, const uint8_t *ram,
                                 const uint8_t *rom, size_t rom_size,
                                 uint16_t *current, uint16_t *next);

#endif
