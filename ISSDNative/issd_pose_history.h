#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Continues locomotion animation for objects sitting in the widescreen margin.
 *
 * The cartridge culls objects outside x,y in [-32,288) (CODE_83CFE5) and stops
 * advancing their pose, so a 16:9 or 21:9 view -- which reaches x in [-71,327)
 * or [-95,351) -- shows frozen players in the outer band. Widening that window
 * in guest code was measured and rejected: it only lifted margin animation from
 * 7.1% to 11.9% of moving frame-pairs, and it diverged the simulation.
 *
 * This is presentation only. It observes each object's pose while the game is
 * still animating it, and while the object is out in the margin and moving it
 * replays the locomotion cycle it last saw, at the cadence it saw. Nothing is
 * written back to WRAM, so the simulation is untouched.
 *
 * The widescreen renderer supplies complete animation descriptors as tokens,
 * pairing each continued geometry with its actual graphics. It only continues
 * a frozen member of a demonstrated cycle. Changed or unlearned descriptors
 * retain the cartridge action; it cannot invent an action begun out of view. */

#ifdef __cplusplus
extern "C" {
#endif

/* Object grid understood here: $0400..$1AFF on $0100 boundaries. */
#define ISSD_POSE_FIRST_OBJECT 0x0400u
#define ISSD_POSE_END_OBJECT   0x1B00u

/* Portable little-endian records with signed 32-bit world coordinates. */
#define ISSD_POSE_HISTORY_STATE_SIZE (23u * 176u)
void issd_pose_history_save_state(uint8_t *state);
bool issd_pose_history_validate_state(const uint8_t *state, size_t size);
bool issd_pose_history_load_state(const uint8_t *state, size_t size);

/* Forget every observation. Call when continuity with the previous presented
 * frame is broken (state load, scene cut). */
void issd_pose_history_reset(void);

/* Record one object once per frame, before reconstructing it. A zero token
 * marks an inactive animation and forgets all evidence for that object slot. */
void issd_pose_history_observe(unsigned object, int x, int y, uint16_t pose);
/* Screen coordinates determine culling; world coordinates determine travel,
 * so camera panning cannot make a stationary player run in place. */
void issd_pose_history_observe_world(unsigned object, int x, int y,
                                     int world_x, int world_y, uint16_t pose);

/* Pose to reconstruct this object with: the live pose whenever the game is
 * still animating it, otherwise a continued one. Falls back to the live pose
 * -- holding the last frame, today's behaviour -- when the object is standing
 * still or its recent history shows no clean cycle to continue. */
uint16_t issd_pose_history_pose(unsigned object, int x, int y, uint16_t pose);

/* Returns the last known valid pose for an object (either emitted or observed). */
uint16_t issd_pose_history_last_pose(unsigned object);

/* Exposed for tests: the window inside which the cartridge still animates. */
bool issd_pose_history_live_window(int x, int y);

#ifdef __cplusplus
}
#endif
