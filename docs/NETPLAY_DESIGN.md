# Cross-platform netplay design

Date: 2026-10-01

Status: Future implementation design; netplay is not implemented. Graphics and resolution improvements take priority. This document preserves the proposed scope for later development; transport selection and the initial internet-service scope remain design decisions.

## Goals

- Support cross-play between Windows, Android and other supported builds of the recomp.
- Require the same released game version, without requiring users to manually match gameplay settings.
- Discover hosted games on the same local network automatically.
- Support direct connections by IP address and port.
- Support up to four total players, combining local controllers and remote devices.

Examples include one player on each of four devices, two players on Windows plus two on Android, and supported original-game team arrangements such as 1v1, 2v2 and cooperative play against the CPU.

## Proposed first beta

Add a **Netplay** section to the overlay with **Host**, **Find LAN Games**, and **Join IP** actions.

### Hosting and joining

- Hosts choose a room name, an optional password and available player slots.
- LAN discovery advertises active hosted rooms and lists compatible rooms with player count and connection information.
- Direct joining accepts an IP address and port. Show the host's local address and port in the hosting screen.
- Explain version mismatches and connection failures before entering a match.
- Keep direct joining available when guest Wi-Fi, network isolation or other network configuration prevents discovery.
- Save recent connection addresses for convenient reuse.

### Lobby

- Display connected devices, player names, assigned slots, team assignment, readiness and connection quality.
- Allow several local players on one device, within the four-player session limit.
- The host controls match setup and original-game menu navigation. Assign gameplay input to explicit session slots rather than relying on each device's controller connection order.
- Show the selected teams, rules and gameplay tweaks before players ready up.
- Start only after compatibility checks and initial state synchronization succeed.

### Session compatibility

The visible requirement is the same released game version. Internally, the handshake must verify protocol/build compatibility and gameplay-affecting content so incompatible simulations cannot start silently. Development builds may share a version string while containing different code; they need a compatibility identity beyond that string.

The host supplies session rules and gameplay tweak settings, including AI improvements and Original Bug Fixes. Apply these temporarily and restore each client's local configuration after leaving.

Gameplay-affecting mod data must also match. Automatically synchronize supported session content where practical and permitted; otherwise, explain unsupported mod conflicts before starting. Do not claim arbitrary mod combinations are compatible. Do not transfer the original ROM: each device uses its own supported local game image.

Graphics, resolution and controller preferences should remain local where they do not affect simulation. Audit settings that appear cosmetic but change emulated behavior before classifying them as safe to differ.

### Match behavior and recovery

- Coordinate pause, substitutions, formation changes, rematch and restart across devices.
- Keep teams, kits, weather and rules consistent when rematching.
- Pause when a player disconnects, offer bounded reconnection, and show an explicit outcome if recovery fails.
- Treat Android backgrounding or suspension as a possible interruption requiring pause and reconnection.
- Detect simulation divergence and recover using a validated host snapshot. Repeated failure must end or pause the session visibly rather than allow different matches to continue.
- Restrict independent save/load, resets and gameplay-mod changes during a session. Host-controlled operations must synchronize all peers.
- Define save ownership explicitly: initially, only the host writes shared-session campaign progress and autosaves.

## Recommended simulation architecture

Start with synchronized frame inputs and a small adjustable input delay. The host coordinates frame input delivery, session settings and recovery; every peer runs the game simulation locally.

The existing four-player input transport, 60 Hz simulation boundary and memory snapshots provide integration points. They do **not** establish Windows x86-64 versus Android ARM64 determinism or portable serialization. Prove both before shipping cross-play.

Required foundations:

1. A network-independent session state machine for hosting, discovery, joining, lobby, synchronization, playing, interruption and leaving.
2. Portable, versioned messages with explicit scalar sizes, byte order and bounded payloads.
3. Frame-numbered inputs for all assigned slots, with duplicate and late-packet handling.
4. Deterministic initial state transfer and canonical simulation-state checksums.
5. Validated, transactional resynchronization that clears stale input and resets relevant host-side caches.
6. Clear separation of simulation from local presentation, audio, wall-clock timing and file-writing side effects.
7. A transport abstraction that can later accommodate relays without changing gameplay synchronization.

Choose the transport library after checking supported platform builds and requirements. Reliable lobby/state delivery and timely input delivery are distinct needs; the exact TCP/UDP or library arrangement is not yet selected.

### Alternatives and trade-offs

- **Input delay first — recommended:** simpler correctness and recovery model; latency becomes noticeable on slower internet connections.
- **Rollback later:** can improve responsiveness, but requires deterministic restoration and safe handling of audio, presentation, saves and other replayed side effects.
- **Host video streaming:** reduces client simulation requirements, but adds bandwidth, image-quality and latency compromises. It is not the recommended direction for this recomp.

Expose ping and connection quality, with automatic delay selection and a manual adjustment where useful. Desync recovery is a safeguard, not a substitute for deterministic simulation.

## Internet play and later features

LAN discovery and direct IP are the recommended initial scope. Whether invite-code internet rooms and relay fallback belong in the first beta remains undecided.

Direct internet connections require a reachable host and may need firewall or router configuration. LAN discovery does not solve internet NAT traversal.

Preserve these follow-up features in the roadmap:

- Invite codes and relay fallback for joining friends without manual router setup.
- Spectators, particularly for tournaments.
- Frame-input replays for sharing matches and diagnosing desynchronization.
- QR joining to reduce address typing on Android.

Relay infrastructure, service ownership and operating costs need a separate decision before implementation. Do not make a public matchmaking service a prerequisite for LAN play.

## Platform integration

- Windows: provide useful firewall and address diagnostics when hosting fails.
- Android: implement the required network permissions, discovery lifecycle and suspend/resume handling for supported target SDKs and devices.
- All platforms: support explicit cancellation, responsive UI during networking and cleanup when a session ends.

Android local-network access requirements are evolving; verify current requirements when implementation begins. Reference: [Android local network permission documentation](https://developer.android.com/privacy-and-security/local-network-permission).

## Acceptance criteria

Netplay is ready for a beta only when the following are demonstrated:

- Windows-to-Windows and Windows-to-physical-Android sessions run from identical initial state and inputs without unexplained simulation divergence.
- Two, three and four total players work, including mixed local and remote controller assignments.
- LAN rooms appear, disappear and reject incompatible versions correctly; direct IP remains usable when discovery is unavailable.
- Host rules and supported gameplay tweaks synchronize without permanently changing client preferences.
- Full matches cover goals, extra time, penalties, substitutions, formation changes, pause and rematch.
- Controlled latency, jitter, packet loss, reordering and duplication do not produce silent divergence or unbounded stalls.
- Disconnect, reconnect, host loss and Android suspend/resume have clear, tested outcomes.
- Snapshot transfer and recovery reject invalid or incompatible state safely.
- Local graphics and resolution differences do not change simulation results.
- Session exit releases network resources and restores local settings and input behavior.

## Implementation order when resumed

1. Audit cross-platform determinism, canonical state hashing and snapshot portability.
2. Build and test the session protocol, input scheduling and initial synchronization.
3. Add hosting, direct joining and the lobby.
4. Add LAN discovery and Android platform integration.
5. Add coordinated match controls, interruption handling and resynchronization.
6. Run the cross-platform acceptance matrix and document supported behavior and limitations.
7. Evaluate rollback, relays, spectators, QR joining and replay support as subsequent milestones.

No netplay implementation or release commitment is implied by saving this design. Resume it after the graphics and resolution work, using this document as the starting point.
