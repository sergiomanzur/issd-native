#ifndef RETCOMM_RBENGINE_H
#define RETCOMM_RBENGINE_H

/*
 * retcomm-rbengine — portable rollback host helpers for recomp engines.
 *
 * Depends on recomp-net (RNetSession tips, RNetRbFrame, input contract).
 * Owns MotK-proven host policy that does not belong in the episode FSM:
 *   - admission scheduler (invent / cushion / timesync / auto-D)
 *   - input history + invent helpers
 *   - hash_confirm watermark
 *   - opaque snapshot ring
 *
 * Engines supply serialize/digest/FMV gates; this library never advances sim.
 */

#include "retcomm_rbengine/hash_confirm.h"
#include "retcomm_rbengine/input_hist.h"
#include "retcomm_rbengine/mono_ms.h"
#include "retcomm_rbengine/rb_post.h"
#include "retcomm_rbengine/sched.h"
#include "retcomm_rbengine/snap_ring.h"

#endif /* RETCOMM_RBENGINE_H */
