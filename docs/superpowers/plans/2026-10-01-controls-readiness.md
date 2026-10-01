# Controls and 1.0 readiness implementation plan

**Goal:** Deliver in-game controller/keyboard remapping, adjustable deadzones,
saved P1–P4 profiles, configurable touch layout, and an evidence-based feature
inventory and acceptance checklist.

**Architecture:** Retain SDL sampling and fixed overlay navigation. Profiles
map physical source masks to native SNES actions independently per player slot.
Touch positions use normalized viewport centers and percentage sizes. Android
background events pause simulation/audio, clear held input and require explicit
resume. Configuration remains outside gameplay save compatibility identity.

**Constraints:** Existing defaults and legacy profiles keep working; no ROM
or private captures enter Git. Keep beta status and separate automated fixture
checks from complete unmodified tournaments and physical-device validation.

- [x] Input/config: failing virtual-pad and persistence tests; implement four
  profiles, presets, bounded independent stick/trigger deadzones and safe parsing.
- [x] Touch: failing moved/scaled/rotation/reset tests; add per-control layout
  and held-point clearing without changing the default layout.
- [x] Overlay: failing menu tests; add Controls, Keyboard and Touch Layout pages,
  binding capture with fixed navigation, cancel/reset and immediate persistence.
- [x] Host: consume configured keyboard bindings, register profiles, apply touch
  layout; test background/foreground input clearing and simulation/audio pause.
- [x] Audit: inventory every user-facing feature and setting against code/tests;
  remove active UI claims for inert engine selection, document remaining no-ops.
- [x] Acceptance: exercise tournament/result/save reload, extra-time/penalty,
  substitution and disconnect scenarios with bounded native tests where possible;
  publish reproducible hardware checks and mark unexecuted checks honestly.
- [x] Integration: run focused then project tests, build Windows/Linux/Android,
  verify the new pages visually, and update docs with actual verification evidence.

Ownership: controls worker owns input/config; touch worker owns touch; audit
worker owns feature inventory/acceptance documents and tests; root owns menu,
host integration and final verification. Do not commit/push without a new request.

Verification: 174 passed / 2 existing failures / 1 skipped / 7 subtests; final focused suite 30 passed. Separately added fresh-start accelerated Cup: nine matches and result/completion Continue, 1 passed in 266.03 seconds. Windows/Linux/Android builds and overlay visual check succeeded. Untouched World/Cup, shootout, original substitutions and physical pad/Android checks remain documented acceptance gaps.
