#ifndef ISSD_FRAME_PACING_H
#define ISSD_FRAME_PACING_H

#include <stdbool.h>
#include <stdint.h>

/* Permit short simulation catch-up bursts, but guarantee that sustained load
 * cannot starve SDL presentation forever. A configured render interval still
 * controls the maximum presentation rate. */
static inline bool issd_presentation_due(uint64_t now,
                                         uint64_t next_sim_time,
                                         uint64_t last_present_time,
                                         uint64_t next_render_time,
                                         uint64_t render_interval,
                                         uint64_t max_catchup_delay) {
    const bool render_due = render_interval == 0 || now >= next_render_time;
    const bool simulation_has_slack = now < next_sim_time;
    const bool catchup_budget_exhausted =
        now >= last_present_time &&
        now - last_present_time >= max_catchup_delay;
    return render_due && (simulation_has_slack || catchup_budget_exhausted);
}

#endif
