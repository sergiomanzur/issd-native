#include <assert.h>
#include <stdint.h>

#include "issd_frame_pacing.h"

int main(void) {
  const uint64_t tick = 100;

  /* Present normally when simulation has slack and the render tick is due. */
  assert(issd_presentation_due(1000, 1050, 900, 950, 100, tick * 4));

  /* A briefly late simulation may spend a few frames catching up. */
  assert(!issd_presentation_due(1000, 950, 700, 950, 100, tick * 4));

  /* Sustained lateness must never starve presentation indefinitely. */
  assert(issd_presentation_due(1100, 950, 700, 950, 100, tick * 4));

  /* A configured presentation interval remains authoritative. */
  assert(!issd_presentation_due(1100, 950, 700, 1200, 100, tick * 4));

  /* Uncapped presentation still obeys the bounded catch-up window. */
  assert(!issd_presentation_due(1000, 950, 700, 0, 0, tick * 4));
  assert(issd_presentation_due(1100, 950, 700, 0, 0, tick * 4));
  return 0;
}
