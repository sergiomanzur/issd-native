/* Locomotion continuation for objects the cartridge has stopped animating. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
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

static void reject_state(const uint8_t *bad, size_t size) {
  uint8_t before[ISSD_POSE_HISTORY_STATE_SIZE], after[ISSD_POSE_HISTORY_STATE_SIZE];
  issd_pose_history_save_state(before);
  assert(!issd_pose_history_validate_state(bad, size));
  assert(!issd_pose_history_load_state(bad, size));
  issd_pose_history_save_state(after);
  assert(!memcmp(before, after, sizeof(before)));
}

static void test_state(void) {
  const uint16_t cycle[] = {0x4010, 0x4020, 0x4030, 0x4040};
  uint8_t saved[ISSD_POSE_HISTORY_STATE_SIZE], after[ISSD_POSE_HISTORY_STATE_SIZE];
  uint8_t bad[ISSD_POSE_HISTORY_STATE_SIZE];
  uint16_t expected[40];
  issd_pose_history_reset();
  int x = 200;
  for (unsigned i = 0; i < 32; i++, x -= 3) live(x, cycle[(i / 2) % 4]);
  x = -40;
  for (unsigned i = 0; i < 15; i++, x -= 3) margin(x, cycle[3]);
  /* Include a separately observed slot with negative world coordinates. */
  issd_pose_history_observe_world(0x400, 100, 100, -1234, -5678, 0x1234);
  issd_pose_history_save_state(saved);
  assert(issd_pose_history_validate_state(saved, sizeof(saved)));
  assert(saved[1] == 0x2e && saved[2] == 0xfb && saved[3] == 0xff && saved[4] == 0xff);
  int saved_x = x;
  for (unsigned i = 0; i < 40; i++, x -= 3) expected[i] = margin(x, cycle[3]);
  issd_pose_history_reset();
  assert(issd_pose_history_load_state(saved, sizeof(saved)));
  issd_pose_history_save_state(after);
  assert(!memcmp(saved, after, sizeof(saved)));
  assert(issd_pose_history_last_pose(0x400) == 0x1234);
  x = saved_x;
  for (unsigned i = 0; i < 40; i++, x -= 3) assert(margin(x, cycle[3]) == expected[i]);
  reject_state(NULL, sizeof(saved));
  for (size_t n = 0; n < sizeof(saved); n++) reject_state(saved, n);
  reject_state(saved, sizeof(saved) + 1);
  const unsigned offsets[] = {0, 9, 10, 75, 76, 129, 134, 160, 161, 162, 163, 169, 171,
                             ISSD_POSE_HISTORY_STATE_SIZE - 1};
  const uint8_t values[] = {2, 2, 2, 9, 8, 27, 6, 13, 2, 12, 5, 8, 1, 1};
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
    memcpy(bad, saved, sizeof(bad));
    bad[offsets[i]] = values[i];
    reject_state(bad, sizeof(bad));
  }
  /* Serialized signed 32-bit extremes remain valid. Travel arithmetic must
   * safely handle both subtraction and absolute values after restoration. */
  const uint32_t extremes[] = {UINT32_C(0x80000000), UINT32_C(0x7fffffff),
                              UINT32_C(0xffff7fff), UINT32_C(0x00008000)};
  for (unsigned coordinate = 0; coordinate < 18; coordinate++) {
    unsigned offset = coordinate < 2 ? 1u + coordinate * 4u
                                     : 11u + (coordinate - 2u) * 4u;
    for (unsigned i = 0; i < sizeof(extremes) / sizeof(extremes[0]); i++) {
      memcpy(bad, saved, sizeof(bad));
      for (unsigned byte = 0; byte < 4; byte++)
        bad[offset + byte] = (uint8_t)(extremes[i] >> (byte * 8u));
      assert(issd_pose_history_load_state(bad, sizeof(bad)));
      issd_pose_history_observe_world(0x400, 100, 100, INT_MAX, INT_MAX, 0x1234);
      issd_pose_history_observe_world(0x400, 100, 100, INT_MIN, INT_MIN, 0x1234);
      issd_pose_history_save_state(after);
      assert(issd_pose_history_validate_state(after, sizeof(after)));
    }
    const uint32_t boundaries[] = {UINT32_C(0xffff8000), UINT32_C(0x00007fff)};
    for (unsigned i = 0; i < 2; i++) {
      memcpy(bad, saved, sizeof(bad));
      for (unsigned byte = 0; byte < 4; byte++)
        bad[offset + byte] = (uint8_t)(boundaries[i] >> (byte * 8u));
      assert(issd_pose_history_validate_state(bad, sizeof(bad)));
    }
  }
}

int main(void) {
  test_state();
  /* The live window requires x in [0, 288) and y in [-32, 288). */
  assert(issd_pose_history_live_window(0, 0));
  assert(!issd_pose_history_live_window(-1, 0));
  assert(!issd_pose_history_live_window(-32, 0));
  assert(issd_pose_history_live_window(287, 0));
  assert(!issd_pose_history_live_window(288, 0));
  assert(issd_pose_history_live_window(0, -32));
  assert(!issd_pose_history_live_window(0, -33));
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

  /* A player held up by a tackle or pinned against a boundary still jitters
   * a pixel every frame. That must not read as running: replaying a run cycle
   * on the spot is what made stuck players appear to sprint without moving. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 4; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  x = -40;
  margin(x, frozen);
  for (int f = 0; f < 20; f++) {
    x += (f & 1) ? 1 : -1;              /* jitter, going nowhere */
    assert(margin(x, frozen) == frozen);
  }

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

  /* A new action outside the learned cycle must retain its own descriptor.
   * Continuing a previous run would turn tackles and turns into running. */
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
  assert(changes == 0);

  /* Advancing cartridge descriptors in the margin are authoritative. The
   * hold window prevents replacing a slower, still-live animation step. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 4; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  x = -20;
  for (int f = 0; f < 24; f++) {
    uint16_t actual = cyc[(f/2) % 4];
    x -= 2;
    assert(margin(x, actual) == actual);
  }

  /* Auxiliary records are not on the object grid and are passed through. */
  issd_pose_history_observe(0x04A0, -40, 100, 0x1234);
  assert(issd_pose_history_pose(0x04A0, -40, 100, 0x1234) == 0x1234);

  /* Re-entering the live window hands control straight back to the game at x=0. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 4; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  x = -40;
  for (int f = 0; f < 6; f++) { x += 3; margin(x, frozen); }
  assert(!issd_pose_history_live_window(-1, 100));
  assert(issd_pose_history_live_window(0, 100));
  assert(live(0, 0x4777) == 0x4777);

  /* A disabled animation must not revive a previous action or run cycle. */
  issd_pose_history_reset();
  x = 200;
  for (int rep = 0; rep < 4; rep++)
    for (int i = 0; i < 4; i++) { live(x, cyc[i]); x -= 3; live(x, cyc[i]); x -= 3; }
  x = -40;
  for (int f = 0; f < 20; f++) { x -= 3; margin(x, frozen); }
  assert(margin(x - 3, 0) == 0);
  assert(issd_pose_history_last_pose(OBJ) == 0);
  for (int f = 0; f < 20; f++) {
    x -= 3;
    assert(margin(x, frozen) == frozen); /* reused slot requires fresh evidence */
  }

  puts("pose history tests passed");
  return 0;
}
