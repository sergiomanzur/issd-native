#include "issd_pose_history.h"
#include <string.h>

#define SLOTS   ((ISSD_POSE_END_OBJECT - ISSD_POSE_FIRST_OBJECT) / 0x100)
#define SEQ_MAX 26u         /* recent distinct poses kept per object */
/* Measured on a live match, ISSD run cycles are 8 to 10 poses long
 * (object $0900 repeats 4324 432C 4336 4340 434A 4354 435E 436A 4374 437C).
 * Detection needs two full periods of history, hence SEQ_MAX >= 2*CYCLE_MAX. */
#define CYCLE_MAX 12u       /* longest locomotion period we try to detect */
#define CADENCE_MAX 32u     /* beyond this the motion is not a run cycle */

typedef struct {
  bool     seen;
  int      last_x, last_y;
  bool     had_pos;
  bool     moving;      /* moved since the previous observed frame */

  uint16_t seq[SEQ_MAX];    /* distinct poses, oldest .. newest */
  uint8_t  seq_len;
  uint16_t last_pose;
  uint16_t since_change;    /* frames the live pose has been unchanged */
  uint16_t cadence;         /* observed frames between pose changes */

  /* Last locomotion cycle this object actually demonstrated. Kept once
   * learned: a run is constantly interrupted by turns, tackles and ball
   * control, and requiring two clean periods immediately before the object
   * crosses out left 87% of margin frames with nothing to continue. */
  uint16_t cyc[CYCLE_MAX];
  uint8_t  cyc_len;

  bool     running;         /* currently continuing in the margin */
  uint8_t  phase;
  uint16_t tick;
  uint16_t emitted;         /* pose handed out while continuing */
} Slot;

static Slot s_slots[SLOTS];

void issd_pose_history_reset(void) { memset(s_slots, 0, sizeof(s_slots)); }

/* CODE_83CFE5 flags an object off-screen when (x + 32) or (y + 32) reaches 320,
 * compared unsigned, which is exactly x,y in [-32,288). */
bool issd_pose_history_live_window(int x, int y) {
  return (unsigned)(x + 32) < 320u && (unsigned)(y + 32) < 320u;
}

static Slot *slot_for(unsigned object) {
  if (object < ISSD_POSE_FIRST_OBJECT || object >= ISSD_POSE_END_OBJECT) return NULL;
  if (object & 0xFFu) return NULL;   /* auxiliary records are not on the grid */
  return &s_slots[(object - ISSD_POSE_FIRST_OBJECT) / 0x100];
}

/* Smallest period whose repetition the tail of the sequence agrees with.
 * Needs two full periods of evidence so a there-and-back pair is not mistaken
 * for a cycle on first sight. */
static uint8_t detect_cycle(const uint16_t *seq, uint8_t len) {
  for (uint8_t p = 2; p <= CYCLE_MAX; p++) {
    if (len < 2u * p) continue;
    bool ok = true;
    for (uint8_t i = 0; i < p; i++) {
      if (seq[len - 1u - i] != seq[len - 1u - i - p]) { ok = false; break; }
    }
    if (ok) return p;
  }
  return 0;
}

static void push_pose(Slot *s, uint16_t pose) {
  if (s->seq_len == SEQ_MAX) {
    memmove(s->seq, s->seq + 1, (SEQ_MAX - 1) * sizeof(s->seq[0]));
    s->seq_len--;
  }
  s->seq[s->seq_len++] = pose;
}

void issd_pose_history_observe(unsigned object, int x, int y, uint16_t pose) {
  Slot *s = slot_for(object);
  if (!s) return;

  s->moving = s->had_pos && (x != s->last_x || y != s->last_y);
  s->last_x = x; s->last_y = y; s->had_pos = true;
  const bool moving = s->moving;

  if (!issd_pose_history_live_window(x, y)) return;

  /* Inside the window the cartridge owns the animation: learn from it and
   * abandon any continuation that was running. */
  s->running = false;

  if (!s->seen) {
    s->seen = true;
    s->last_pose = pose;
    push_pose(s, pose);
    s->since_change = 0;
    return;
  }

  if (pose == s->last_pose) {
    if (s->since_change < 0xFFFFu) s->since_change++;
    return;
  }

  /* A pose change while moving is a locomotion step; time it. A change while
   * standing still is a posture switch and says nothing about cadence. */
  if (moving && s->since_change >= 1u && s->since_change <= CADENCE_MAX) {
    s->cadence = s->cadence ? (uint16_t)((s->cadence + s->since_change + 1u) / 2u)
                            : s->since_change;
  }
  s->last_pose = pose;
  s->since_change = 0;
  push_pose(s, pose);
  uint8_t p = detect_cycle(s->seq, s->seq_len);
  if (p) {
    for (uint8_t i = 0; i < p; i++) s->cyc[i] = s->seq[s->seq_len - p + i];
    s->cyc_len = p;
  }
}

uint16_t issd_pose_history_pose(unsigned object, int x, int y, uint16_t pose) {
  Slot *s = slot_for(object);
  if (!s) return pose;
  if (issd_pose_history_live_window(x, y)) return pose;

  /* Out in the margin. Only a moving object with a known cycle is continued;
   * anything else holds its last frame, which is the pre-existing behaviour.
   * Movement was measured in observe(), before last_x/last_y advanced. */
  if (!s->seen || !s->cyc_len || !s->cadence || !s->moving) {
    s->running = false;
    return pose;
  }

  if (!s->running) {
    /* Resume from wherever the frozen pose sits in the cycle so the first
     * continued step follows on from what is already on screen. */
    s->running = true;
    s->tick = 0;
    s->phase = 0;
    for (uint8_t i = 0; i < s->cyc_len; i++)
      if (s->cyc[i] == pose) { s->phase = i; break; }
    s->emitted = pose;
    return pose;
  }

  if (++s->tick >= s->cadence) {
    s->tick = 0;
    s->phase = (uint8_t)((s->phase + 1u) % s->cyc_len);
    s->emitted = s->cyc[s->phase];
  }
  return s->emitted;
}
