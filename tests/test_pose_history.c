/* Locomotion continuation for objects the cartridge has stopped animating. */
#include <assert.h>
#include <stdio.h>
#include "issd_pose_history.h"

#define OBJ 0x0900u

/* One frame inside the live window: the game owns the pose, we learn from it. */
static uint16_t live(int x, uint16_t pose) {
  issd_pose_history_observe(OBJ, x, 100, pose);
  return issd_pose_history_pose(OBJ, x, 100, pose);
}

/* One frame out in the margin: the game has frozen `pose`, we may continue it. */
static uint16_t margin(int x, uint16_t frozen) {
  issd_pose_history_observe(OBJ, x, 100, frozen);
  return issd_pose_history_pose(OBJ, x, 100, frozen);
}

int main(void) {
  /* The window matches CODE_83CFE5 exactly: x,y in [-32,288). */
  assert(issd_pose_history_live_window(-32, 0));
  assert(!issd_pose_history_live_window(-33, 0));
  assert(issd_pose_history_live_window(287, 0));
  assert(!issd_pose_history_live_window(288, 0));
  assert(!issd_pose_history_live_window(0, 288));

  /* A player running left through the live window, pose stepping every 2
   * frames through a 4-pose cycle. */
  issd_pose_history_reset();
  const uint16_t cyc[4] = { 0x4010, 0x4020, 0x4030, 0x4040 };
  int x = 200;
  for (int rep = 0; rep < 4; rep++) {
    for (int i = 0; i < 4; i++) {
      uint16_t p = cyc[i];
      assert(live(x, p) == p);   /* inside the window we never substitute */
      x -= 3;
      assert(live(x, p) == p);
      x -= 3;
    }
  }

  /* It crosses out into the margin. The cartridge now holds one pose forever;
   * we should keep stepping the cycle it just demonstrated. */
  uint16_t frozen = cyc[3];
  x = -40;
  int changes = 0;
  uint16_t prev = margin(x, frozen);
  for (int f = 0; f < 24; f++) {
    x -= 3;
    uint16_t got = margin(x, frozen);
    if (got != prev) changes++;
    prev = got;
    /* Whatever we emit must be a pose the game itself used. */
    int known = 0;
    for (int i = 0; i < 4; i++) known |= (got == cyc[i]);
    assert(known);
  }
  assert(changes >= 8);          /* ~every 2 frames, not frozen */

  /* A player standing still in the margin must hold, not moonwalk. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 4; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  x = -40;
  margin(x, frozen);
  for (int f = 0; f < 12; f++) assert(margin(x, frozen) == frozen);

  /* No clean cycle observed: hold the last frame rather than invent one. */
  issd_pose_history_reset();
  x = 200;
  const uint16_t noisy[6] = { 0x4100, 0x4200, 0x4300, 0x4700, 0x4500, 0x4900 };
  for (int i = 0; i < 6; i++) { live(x, noisy[i]); x -= 4; }
  x = -40;
  margin(x, noisy[5]);
  for (int f = 0; f < 10; f++) { x -= 3; assert(margin(x, noisy[5]) == noisy[5]); }

  /* Real ISSD run cycles are 8 to 10 poses long, not 3 or 4. A detector
   * capped at period 4 found a cycle in under 2% of margin frames, so this
   * case pins the length the cartridge actually uses. */
  issd_pose_history_reset();
  const uint16_t run10[10] = { 0x4324, 0x432C, 0x4336, 0x4340, 0x434A,
                               0x4354, 0x435E, 0x436A, 0x4374, 0x437C };
  x = 250;
  for (int rep = 0; rep < 3; rep++) {
    for (int i = 0; i < 10; i++) {
      live(x, run10[i]); x -= 2;
      live(x, run10[i]); x -= 2;
      live(x, run10[i]); x -= 2;
    }
  }
  x = 300;                       /* out past the right edge of the live window */
  uint16_t seen[10] = {0};
  uint16_t held = run10[4];
  margin(x, held);
  for (int f = 0; f < 60; f++) {
    x += 1;
    uint16_t got = margin(x, held);
    for (int i = 0; i < 10; i++) if (got == run10[i]) seen[i] = 1;
  }
  int distinct = 0;
  for (int i = 0; i < 10; i++) distinct += seen[i];
  assert(distinct >= 8);         /* walks the whole 10-pose cycle, not 4 of it */

  /* A cycle once demonstrated is remembered: a player is interrupted by turns
   * and tackles constantly, and requiring two clean periods immediately before
   * crossing out left almost every margin frame with nothing to continue. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 3; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  live(x, 0x46CE); x -= 3;       /* interruption: a turn */
  live(x, 0x4000); x -= 3;
  x = -40;
  margin(x, 0x4000);
  changes = 0;
  prev = margin(x, 0x4000);
  for (int f = 0; f < 24; f++) {
    x -= 3;
    uint16_t g = margin(x, 0x4000);
    if (g != prev) changes++;
    prev = g;
  }
  assert(changes >= 6);

  /* Auxiliary records are not on the object grid and are passed through. */
  issd_pose_history_observe(0x04A0, -40, 100, 0x1234);
  assert(issd_pose_history_pose(0x04A0, -40, 100, 0x1234) == 0x1234);

  /* Re-entering the live window hands control straight back to the game. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 4; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  x = -40;
  for (int f = 0; f < 6; f++) { x -= 3; margin(x, frozen); }
  assert(live(10, 0x4777) == 0x4777);

  puts("pose history tests passed");
  return 0;
}
