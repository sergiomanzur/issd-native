# Host integration

## Layers

```text
Engine facade (snes_netplay / psx_netplay / …)
  ├── retcomm-rbengine   invent policy, hist, hash_confirm, snap ring
  └── recomp-net         RNetSession tips + RNetRbSession episodes
```

## Required gates for scheduler

Minimum for non-media digital titles (SNES / NES):

| Gate | Required? | Notes |
|------|-----------|--------|
| `now_ms` | **yes** | Use `rbe_mono_ms` |
| `rtt_ms` | recommended | ICE/POST sample; 0 = synth from D |
| `episode_active` | recommended | 1 during Seal/Replay/Verify |
| `tip_holding` | recommended | TipHold Live invent-cap |
| media / lockstep | optional | MotK FMV only |

NULL media gates → never lockstep-stall invent; auto-D always samples.

## Snap ring

```c
RbeSnapVTable vt = {
    .ctx = host,
    .serialize = host_save_blob,   /* mallocs *out */
    .deserialize = host_load_blob,
};
rbe_snap_ring_save(ring, tick, &vt);
rbe_snap_ring_load(ring, load_tick, &vt);
```

Or call `rbe_snap_ring_store` with a prebuilt blob.

## MotK migration aliases

When switching MotK off in-tree helpers, map:

| Old | New |
|-----|-----|
| `np_sched_*` / `PsxNpSchedBridge` | `rbe_sched_*` / `RbeSchedBridge` |
| `netplay_hc_*` / `NetplayHashConfirm` | `rbe_hc_*` / `RbeHashConfirm` |
| `netplay_ih_*` / `NetplayInputHist` | `rbe_ih_*` / `RbeInputHist` |
| `netplay_snap_ring_*` | `rbe_snap_ring_*` (+ vtable save/load) |
| `netplay_rb_peer_post_tip_ok` | `rbe_rb_peer_post_tip_ok` |
| `PSX_RB_*` env | `RBE_RB_*` (PSX_* still accepted as fallback) |

Keep MotK `psx_netplay_rb_*` episode driver in the PSX runtime.
