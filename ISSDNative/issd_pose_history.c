#include "issd_pose_history.h"
#include <string.h>

#define SLOTS   ((ISSD_POSE_END_OBJECT - ISSD_POSE_FIRST_OBJECT) / 0x100)
#define SEQ_MAX 26u         /* recent distinct poses kept per object */
/* Measured on a live match, ISSD run cycles are 8 to 10 poses long
 * (object $0900 repeats 4324 432C 4336 4340 434A 4354 435E 436A 4374 437C).
 * Detection needs two full periods of history, hence SEQ_MAX >= 2*CYCLE_MAX. */
#define CYCLE_MAX 12u       /* longest locomotion period we try to detect */
#define CADENCE_MIN 1u      /* fastest locomotion period (2 frames/step: 1 extra frame) */
#define CADENCE_MAX 5u      /* slowest locomotion period (6 frames/step: 5 extra frames); beyond this is idle */
/* A player held up by a tackle or pinned against a boundary still jitters
 * a pixel or two every frame. Treating that as movement made the
 * continuation replay a full run cycle on the spot, so a stuck player
 * appeared to sprint without going anywhere.
 *
 * Movement is therefore net displacement across a window of frames, not
 * distance walked. Summing per-frame steps does not work: a player
 * shuffling one pixel back and forth accumulates plenty of steps while
 * going nowhere, which is precisely the case this exists to reject.
 *
 * A step beyond anything a player covers in a frame is a reposition, not
 * travel - a set piece or a camera cut - and clears the window, so the
 * object is not credited with having sprinted across the pitch.
 *
 * 8 frames at 60Hz is well under one animation cycle; 6 pixels is more
 * than jitter and less than a walking pace covers. */
#define TRAVEL_WINDOW 8u
#define TRAVEL_MIN_PX 6u
#define TRAVEL_TELEPORT_PX 32   /* beyond a frame's travel: reposition */

typedef struct {
  bool     seen;
  int      last_x, last_y;
  bool     had_pos;
  bool     moving;      /* travelling, not merely jittering */
  int      trail_x[TRAVEL_WINDOW], trail_y[TRAVEL_WINDOW];
  uint8_t  trail_n, trail_head;

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

/* The cartridge's 4:3 area starts at x = 0. Although CODE_83CFE5 uses [-32, 288)
 * for coarse culling, player locomotion updates freeze when x < 0 in the left
 * widescreen margin. Control is handed back to the live cartridge pose at x >= 0. */
bool issd_pose_history_live_window(int x, int y) {
  return (unsigned)x < 288u && (unsigned)(y + 32) < 320u;
}

static Slot *slot_for(unsigned object) {
  if (object < ISSD_POSE_FIRST_OBJECT || object >= ISSD_POSE_END_OBJECT) return NULL;
  if (object & 0xFFu) return NULL;   /* auxiliary records are not on the grid */
  return &s_slots[(object - ISSD_POSE_FIRST_OBJECT) / 0x100];
}

/* Locomotion cycle detection. Require period >= 4 so 2-frame or 3-frame
 * idle jitters/turns are not mistaken for run cycles. */
static uint8_t detect_cycle(const uint16_t *seq, uint8_t len) {
  /* First check for 2 full periods of evidence */
  for (uint8_t p = 4; p <= CYCLE_MAX; p++) {
    if (len < 2u * p) continue;
    bool ok = true;
    for (uint8_t i = 0; i < p; i++) {
      if (seq[len - 1u - i] != seq[len - 1u - i - p]) { ok = false; break; }
    }
    if (ok) return p;
  }
  /* If not enough length for 2 full periods of a long cycle (e.g. p=8..10),
   * check if at least 4 consecutive poses match at period p with len >= p + 4 */
  for (uint8_t p = 4; p <= CYCLE_MAX; p++) {
    if (len < p + 4u) continue;
    bool ok = true;
    for (uint8_t i = 0; i < len - p; i++) {
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

  if (s->had_pos) {
    int sx = x - s->last_x, sy = y - s->last_y;
    if (sx < 0) sx = -sx;
    if (sy < 0) sy = -sy;
    if (sx + sy > TRAVEL_TELEPORT_PX) s->trail_n = 0;   /* repositioned */
  }
  if (s->trail_n) {
    unsigned oldest =
        (unsigned)((s->trail_head + TRAVEL_WINDOW - s->trail_n) % TRAVEL_WINDOW);
    int dx = x - s->trail_x[oldest], dy = y - s->trail_y[oldest];
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    s->moving = (unsigned)(dx + dy) >= TRAVEL_MIN_PX;
  } else {
    s->moving = false;
  }
  s->trail_x[s->trail_head] = x;
  s->trail_y[s->trail_head] = y;
  s->trail_head = (uint8_t)((s->trail_head + 1u) % TRAVEL_WINDOW);
  if (s->trail_n < TRAVEL_WINDOW) s->trail_n++;
  s->last_x = x; s->last_y = y; s->had_pos = true;
  const bool moving = s->moving;

  if (!issd_pose_history_live_window(x, y)) return;

  /* Inside the window the cartridge owns the animation: learn from it and
   * abandon any continuation that was running. */
  s->running = false;

  if (!pose) return;

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
   * standing still is a posture switch and says nothing about cadence.
   * Reject gaps > CADENCE_MAX so coming out of idle does not poison cadence. */
  if (moving && s->since_change >= CADENCE_MIN && s->since_change <= CADENCE_MAX) {
    s->cadence = s->cadence ? (uint16_t)((s->cadence + s->since_change + 1u) / 2u)
                            : s->since_change;
    if (s->cadence < CADENCE_MIN) s->cadence = CADENCE_MIN;
    if (s->cadence > CADENCE_MAX) s->cadence = CADENCE_MAX;
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

uint16_t issd_pose_history_last_pose(unsigned object) {
  Slot *s = slot_for(object);
  if (!s) return 0;
  if (s->emitted) return s->emitted;
  if (s->last_pose) return s->last_pose;
  return 0;
}

uint16_t issd_pose_history_pose(unsigned object, int x, int y, uint16_t pose) {
  Slot *s = slot_for(object);
  if (!s) return pose;
  if (issd_pose_history_live_window(x, y)) return pose;

  /* If cartridge supplied 0 or an invalid pose in the margin, fall back to last known */
  if (!pose) {
    pose = s->emitted ? s->emitted : s->last_pose;
  }

  /* Out in the margin. Only a moving object with a known cycle is continued;
   * anything else holds its last frame, which is the pre-existing behaviour.
   * Movement was measured in observe(), before last_x/last_y advanced. */
  if (!s->seen || !s->cyc_len || !s->moving) {
    s->running = false;
    return pose;
  }

  uint16_t cadence = (s->cadence >= CADENCE_MIN && s->cadence <= CADENCE_MAX) ? s->cadence : 3u;

  if (!s->running) {
    /* Resume from wherever the frozen pose sits in the cycle so the first
     * continued step follows on from what is already on screen. */
    s->running = true;
    s->tick = 0;
    s->phase = 0;
    for (uint8_t i = 0; i < s->cyc_len; i++)
      if (s->cyc[i] == pose) { s->phase = i; break; }
    s->emitted = pose ? pose : s->cyc[0];
    return s->emitted;
  }

  if (++s->tick >= cadence) {
    s->tick = 0;
    s->phase = (uint8_t)((s->phase + 1u) % s->cyc_len);
    s->emitted = s->cyc[s->phase];
  }
  return s->emitted;
}
