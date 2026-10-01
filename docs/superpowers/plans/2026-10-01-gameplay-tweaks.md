# Gameplay tweaks implementation

Authorized scope: a persistent Gameplay Tweaks overlay page with independent,
default-off goalkeeper and formation/substitution AI switches. Both may run
together, alongside existing ROM data packs, without restarting the game.

Implementation:
- Add configuration persistence, three-row submenu and save-context bits.
- Adjust goalkeeper lateral targets for the ball crossing the keeper's standing
  plane, after original save decisions, with a bounded correction.
- Adjust outfield off-ball targets using the active formation's role/anchors
  and the current substitute's native speed and condition. Preserve possession,
  human control, original animation/physics and cartridge random state.
- Use native basic-block policy callbacks and equivalent interpreter decision
  hooks. Do not edit generated cartridge code.
- Verify disabled no-op behavior, mirrored teams, formation/substitute changes,
  stacking, persistence, live execution, snapshot replay and compatibility.

These are initial deterministic AI tuning policies, not a replacement football
simulation. Retail password exchange requires the original gameplay context.

Completed verification (2026-10-01):
- Full project suite: 156 passed, 2 existing mod-pack failures, 1 skipped,
  7 subtests passed.
- Final focused AI, hook, live-match, menu, configuration, campaign, advancing
  replay, multiplayer and password suite: 34 passed, 1 optional fixture skipped.
- Menu/mod-stack/hook/policy regression checks: 4 passed. Visually checked the
  rendered page; panel clearing prevents stale page text and toggle values.
- Current Windows, Linux, and Android arm64-v8a/x86_64 release builds succeeded.
- Native/interpreted live goalkeeper and player target changes agree; enabled
  snapshot replay reproduces advancing frames and WRAM. Disabled hooks unregister.
- Native hooks check the execution deadline before changing targets to prevent
  double application when execution yields and resumes.

Gameplay balance remains a playtesting task. The two full-suite failures are
the existing appearance-nibble expectation and shipped-pack kit-sharing checks.
