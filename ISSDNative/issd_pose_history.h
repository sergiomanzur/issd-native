#pragma once
#include <stdbool.h>
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
 * It replays the last cycle; it cannot invent an action that began out of view.
 * A player who starts a slide tackle in the margin still shows a run cycle. */

#ifdef __cplusplus
extern "C" {
#endif

/* Object grid understood here: $0400..$1AFF on $0100 boundaries. */
#define ISSD_POSE_FIRST_OBJECT 0x0400u
#define ISSD_POSE_END_OBJECT   0x1B00u

/* Forget every observation. Call when continuity with the previous presented
 * frame is broken (state load, scene cut). */
void issd_pose_history_reset(void);

/* Record one object once per frame, before reconstructing it. */
void issd_pose_history_observe(unsigned object, int x, int y, uint16_t pose);

/* Pose to reconstruct this object with: the live pose whenever the game is
 * still animating it, otherwise a continued one. Falls back to the live pose
 * -- holding the last frame, today's behaviour -- when the object is standing
 * still or its recent history shows no clean cycle to continue. */
uint16_t issd_pose_history_pose(unsigned object, int x, int y, uint16_t pose);

/* Exposed for tests: the window inside which the cartridge still animates. */
bool issd_pose_history_live_window(int x, int y);

#ifdef __cplusplus
}
#endif
