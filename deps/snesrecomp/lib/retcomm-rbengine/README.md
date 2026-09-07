# retcomm-rbengine

Portable **rollback host helpers** for RetComM recomp engines. Sits next to
[`recomp-net`](https://github.com/TechnicallyComputers/recomp-net): that library
owns the episode FSM, input contract, and wire; this one owns MotK-proven
**host policy** that every engine needs for playable invent/resim feel.

| Module | Role |
|--------|------|
| `sched` | Admit pacing: invent grace, pcap freeze, cushion rebuild, timesync, auto-D |
| `input_hist` | Per-slot `RNetRbFrame` history + hold-last / idle invent + promote |
| `hash_confirm` | Local↔peer FRAME_COMMIT watermark for contract promotes |
| `snap_ring` | Tick-keyed opaque snapshot ring (+ optional serialize vtable) |
| `rb_post` | Tip filter for stale `RB_POST` after tip-extend |
| `mono_ms` | QPC / CLOCK_MONOTONIC helper for `RbeSchedGates.now_ms` |

## Dependency

Requires **recomp-net** (rollback on `main`). CMake looks for:

1. `-DRECOMP_NET_ROOT=/path/to/recomp-net`
2. `../recomp-net` (sibling checkout)
3. `external/recomp-net`

## Build

```bash
cmake -S . -B build -DRNET_ENABLE_ICE=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

```cmake
add_subdirectory(path/to/retcomm-rbengine)
target_link_libraries(your_host PRIVATE retcomm_rbengine)
```

## Host integration sketch

```c
#include "retcomm_rbengine/retcomm_rbengine.h"

static uint32_t host_now(void *ctx) { (void)ctx; return rbe_mono_ms(); }

RbeSchedBridge br = {
    .session = &session,
    .input_delay = &D,
    .input_prediction = &P,
    .local_slot = &slot,
    .gates = { .now_ms = host_now /* + optional FMV/RTT gates */ },
};
rbe_sched_bind(&br);

/* Live admit loop (engine owns tip publish / hist / snaps): */
uint32_t sim = …, wire = rbe_sched_wire_for_sim(sim);
rnet_session_get_stats(session, &st);
if (rbe_sched_pre_admit(sim, wire, &st)) { /* stall-present */ return; }
if (!remote_row) {
    const char *why;
    if (rbe_sched_on_remote_miss(slot, sim, wire, &st, P, &why))
        return; /* wait */
    rbe_ih_invent_hold_last(&hist, slot, wire, &frame);
}
rbe_sched_post_admit(invented);
```

Game-specific digests, savestate serialize, FMV lockstep, and episode pump stay
in the engine. Bind them through `RbeSchedGates` / `RbeSnapVTable`.

## Env knobs (scheduler)

| Variable | Effect |
|----------|--------|
| `RBE_RB_ZERO_DELAY=1` | Legacy consume wire=`sim+D` (no cushion) |
| `RBE_RB_INVENT_GRACE_MS` | Floor ms before invent (default 8) |
| `RBE_RB_GAP1_GRACE_MS` | Flat gap=1 grace override |
| `RBE_RB_GAP1_INVENT=0` | Wait for tip-stale instead of gap1 invent |
| `RBE_RB_TIMESYNC=0` | Disable mispredict pacing debt |
| `RBE_RB_AUTO_DELAY=0` | Disable arrival-driven D controller |
| `RBE_RB_ADAPT_DELAY=0` | Disable pcap-freeze D bumps |
| `RBE_CROSS_OS_PACING_DIAG=1` | 1 Hz pacing diag line |

MotK-era `PSX_RB_*` / `PSX_NETPLAY_CROSS_OS_PACING_DIAG` names are still
honoured when the matching `RBE_*` variable is unset.

## MotK provenance

Lifted from `psxrecomp/runtime` (`psx_netplay_sched`, `netplay_hash_confirm`,
`netplay_input_hist`, `netplay_snap_ring`, `netplay_rb_post`) with PSX types
removed. Pad↔frame conversion and boot_state serialize stay in the PSX host.

## License

MIT — see [LICENSE](LICENSE).
