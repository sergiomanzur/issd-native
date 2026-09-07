# recomp-net

Portable **delay-sync** netcode library for recompilation / modern-runtime hosts
(N64Recomp, PSX recomp, and similar). Classic lockstep: every peer stalls until
remote inputs for `wire = sim + D` arrive. Optional **ICE** transport via
[libjuice](https://github.com/paullouisageneau/libjuice).

BattleShip’s netplay stack is a **design reference only** — this repo does not
vendor SSB64 code and does not implement rollback, automatch, or game UI.

## Features (v0.1)

- Opaque per-slot input blobs (`RNetInputSample`)
- Fixed input delay `D` with `try_admit` / `advance` host loop
- UDP LAN transport and optional ICE mux
- Portable, ranked local IPv4 interface discovery for LAN launchers
- Synchronous RFC 5389 external IPv4 discovery through configurable STUN
- Host-owned signaling callbacks (ICE); lobby / matchmaking is **not** in this
  repo — see [`docs/lobby.md`](docs/lobby.md) and open-source
  [`recomp-net-server`](https://github.com/TechnicallyComputers/recomp-net-server)
- C11, CMake, MIT license

## Lobby

This library has no lobby binary. The MotK / psxrecomp / SNES WebSocket lobby
server is the open-source sibling
[`recomp-net-server`](https://github.com/TechnicallyComputers/recomp-net-server)
(default `ws://netplay.retcomm.net:8765`, or self-host locally).
Client-facing protocol notes: [`docs/lobby.md`](docs/lobby.md).

## Build (LAN, no ICE)

```bash
cmake -S . -B build -DRNET_ENABLE_ICE=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Build with ICE

Optional ICE transport uses [libjuice](https://github.com/paullouisageneau/libjuice).

By default (`RNET_ICE_BUNDLE_STATIC=ON`) CMake **FetchContent**-builds a
**static** libjuice and links it into `recomp_net`, so host binaries do not
need a distro `libjuice.so` at runtime (network required at configure if
juice is not already cached). Set `-DRNET_ICE_BUNDLE_STATIC=OFF` to use
`find_package(Libjuice)` / `RNET_LIBJUICE_ROOT` / `third_party/libjuice`
instead (shared system juice OK).

```bash
cmake -S . -B build-ice -DRNET_ENABLE_ICE=ON
cmake --build build-ice -j
```

ICE needs juice at link time — without a local copy or FetchContent access,
`-DRNET_ENABLE_ICE=ON` will fail configure. Host apps supply signaling via
`RNetHostVTable.on_signal` / `rnet_session_push_signal` (no automatch server).

## Quick LAN demo

```bash
# terminal A (slot 0 / sim authority)
./build/lan_delay_2p 0 7777

# terminal B (slot 1)
./build/lan_delay_2p 1 0 127.0.0.1:7777
```

## Integrate

```cmake
add_subdirectory(path/to/recomp-net)
target_link_libraries(your_host PRIVATE recomp_net)
```

```c
#include "recomp_net/recomp_net.h"

/* Implement RNetHostVTable: sample_local, publish, optional on_signal/now_ms */
RNetSession *s = rnet_session_create(&cfg, &host);
/* Host-style listen: pass NULL/empty peer and learn the first inbound client. */
rnet_session_start_lan(s, "0.0.0.0:7777", NULL);
for (;;) {
    rnet_session_pump(s);
    if (rnet_session_try_admit(s, rnet_session_sim_tick(s))) {
        /* run one authoritative sim step */
        rnet_session_advance(s);
    }
}
```

See [docs/host_integration.md](docs/host_integration.md).

## Recommended host / recomp-engine patches

recomp-net alone does not make a recomp title feel good online. The **host
loop and engine** (snesrecomp / psxrecomp / game `main`) need a few patterns
that shipping MotK / SNES titles already use. Prefer putting these in the
**shared engine / facade** so every game inherits them — not one-off copies in
each title’s `main.c`.

### Frame loop (stability + feel)

| Do | Why |
|----|-----|
| On admit **stall**: re-present the last framebuffer, poll events, pace ~1 frame (~16 ms). **Do not** busy-spin or skip present. | Avoids frozen window / OS “not responding” and audio underrun while waiting on the peer. |
| Prefer `rnet_session_wait_recv` (LAN) over bare `Sleep` in the admit barrier. | Wakes when UDP is readable instead of burning the delay budget blind. |
| Latch delay-sync **starvation**: after sustained admit misses, keep `pump` (retransmit) while holding; clear when `remote_lead >= D` for a few frames. | Recovers from jitter without inventing inputs. |
| After starvation clears, resume **~1 sim / wall frame** and let the input buffer rebuild. Avoid large “recovery burst” catch-up by default. | Turbo catch-up feels worse than a short soft buffer rebuild. |
| Optional catch-up only when `remote_lead > D`, capped (e.g. env `SNES_NET_CATCHUP_CAP`; default **0** in snesrecomp). | Drains surplus lead without fighting the delay runway. |

Reference shape (SNES facade): `snes_host_barrier_admit` +
`snes_host_catchup_budget` in snesrecomp — stall → held present; admit →
`RtlRunFrame` (+ optional burst).

### Pads, threading, audio (determinism)

| Do | Why |
|----|-----|
| Use `publish` / published pads as the **only** controller source for locked sim ticks. | Local-only pad reads desync peers immediately. |
| One thread owns `pump` + `try_admit` + sim `advance` (or an external mutex). | Session API is not internally locked. |
| Disable wall-clock APU / audio “catch-up” while netplay is active; keep guest-frame–coupled audio. | Independent audio catch-up advances time differently per peer. |
| Keep RNG, timers, and frame pacing deterministic; put host-authoritative entropy in the pad blob if needed. | Library does not fix host desyncs. |

### Session lifecycle

| Do | Why |
|----|-----|
| Poll `rnet_session_peer_disconnected(~1500)` and `input_desync` while waiting; soft-exit to lobby. | Snappy leave instead of infinite stall. |
| Call `rnet_session_send_bye` before destroy. | Peer can drop without waiting out the silence timeout. |
| After LOAD / savestate: host probe → apply → `hard_resync` + `prime_delay_inputs` on both sides; keep the app barrier until `try_admit` works again. | Avoid tip/history collisions across the load epoch. |
| Rematch / soft-return: `session_reset` sticky LLE / frame gates; do not reuse a stale UDP `session_id`. | Second match in one process otherwise inherits dead state. |

### Transport / ICE (online)

| Do | Why |
|----|-----|
| Online MotK-style lobbies: always ICE (do not demote to LAN because of a rewritten private peer IP). | Hairpin / wrong advertise breaks P2P. |
| Mint Coturn TURN; for carrier CGNAT / mobile hotspot, host **Force TURN** (`force_relay`) for all peers. | STUN/`prflx` “success” is often flaky on CGNAT. |
| Keep library auto TURN fallback (FAILED / stuck / dead non-relay path) — both peers need a build that supports it. | Answerer alone cannot re-offer; controlling must restart. |
| Ensure INPUT bundles cover the full delay prefix at start (`D+1` ticks including 0). `RNET_MAX_BUNDLE` is **21**; never truncate the low end of the window. | Truncation deadlocks admit at `sim_tick==0` (`wait_remote_input`). |
| Lobby `match_caps` host-authoritative for `input_delay`, `force_turn`, `force_input_relay`. | Guests must not overwrite host delay/TURN policy on fill. |

### Where to implement

| Layer | Put here |
|-------|----------|
| **recomp-net** | Session, ICE, TURN fallback, bundle size, protocol |
| **Engine facade** (e.g. `snes_netplay` / `psx_netplay`, `snes_host_*`) | Barrier admit, starvation latch, catch-up budget, soft-exit, diag JSONL |
| **recomp-ui** | Lobby Settings (delay, Force TURN, server input relay), UDP bind policy |
| **Game title** | Thin identity / `fill_match_caps`, pad sample hook, present-held callback, connect-timeout modal |

SNES-oriented checklist: snesrecomp `docs/RECOMP_NET.md` (“Per-game patches” + host loop). Library API detail: [docs/host_integration.md](docs/host_integration.md).

## Docs

| Doc | Topic |
|-----|--------|
| [docs/architecture.md](docs/architecture.md) | Layers, phases, admission |
| [docs/protocol.md](docs/protocol.md) | Wire packets |
| [docs/signaling.md](docs/signaling.md) | ICE signaling contract |
| [docs/host_integration.md](docs/host_integration.md) | Hooking a recomp host |
| [docs/address_discovery.md](docs/address_discovery.md) | Selecting a LAN address to advertise |
| [docs/lobby.md](docs/lobby.md) | Lobby server contract (sibling repo) |
| [docs/rollback.md](docs/rollback.md) | Rollback mode contracts (`feat/rollback`) |

## Modes

- **Delay-sync (main, v0.1):** shipped `RNetSession` lockstep used by MotK / snes / psx.
- **Rollback (`feat/rollback` branch):** shared rollback architecture; first layer is the
  portable input contract (`recomp_net/input_contract.h`), with episode orchestration over
  a host snapshot/hash vtable planned next. See [docs/rollback.md](docs/rollback.md).

## Non-goals

- Automatch or matchmaking HTTP clients
- Game-specific pad layouts, snapshots, or determinism fixes (host responsibility)
