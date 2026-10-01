# ISSD Native — Enhancements & Modern Features

This document combines implemented enhancements and older design proposals.
The implemented behavior and inactive settings are inventoried in
[Supported features](SUPPORTED_FEATURES.md); [Acceptance checks](ACCEPTANCE_TESTS.md)
records the remaining game/device validation. Items labeled proposed below are
roadmap ideas, not advertised working behavior.

---

## 1. Visual & Presentation Enhancements

### Resolution & Scaling
- **Integer Scaling:** Crisp, uniform pixel scaling (1x, 2x, 3x, 4x) avoiding pixel distortion.
- **Aspect Ratio Control:**
  - `4:3 Aspect (CRT standard)`: Scaled with authentic SNES aspect ratio.
  - `8:7 Aspect (Pixel perfect 1:1)`: Displays square pixels directly.
  - `Widescreen (16:9 / 16:10)`: Expanded horizontal viewport with extended pitch visibility.
- **CRT effect (implemented):** CPU CRT processing and a separate legacy 1x scanline path. Adjustable strength/aperture grille shaders are proposed, not implemented.
- **Fullscreen (implemented):** SDL desktop fullscreen. Exclusive fullscreen is not implemented.

### Refresh Rate & Fluidity
- **Decoupled Simulation & Rendering (implemented):** 60 Hz gameplay simulation and requested 120 / 144 / 165 / 240 Hz presentation. Higher presentation rates repeat frames; interpolation is proposed, not implemented.
- **Alternate timing modes (proposed):** The legacy Classic/Enhanced setting has no runtime timing consumer. The overlay labels the single engine baseline as fixed; no selectable slowdown-removal mode is established.

---

## 2. Audio & Sound Enhancements

- **Host resampling (implemented):** Linear interpolation to the obtained SDL device rate, with bounded occupancy correction and continuity protection. This is not sinc resampling and cannot guarantee no starvation or pitch drift.
- **Volume (implemented):** Master volume. Persisted music/SFX volume fields have no independent runtime consumer; separate mixing is proposed.
- **MSU-1 / CD Audio Stream (proposed):** No runtime replacement soundtrack implementation.

---

## 3. Modern Save Management

- **Save states:** Quicksave and eight numbered manual slots with integrity and applied gameplay compatibility checks. Legacy raw files require confirmation.
- **Continue and autosave:** Cup/World Series checkpoints at verified setup, committed result, completion, and accepted password-import transitions. Two previous valid generations provide recovery.
- **Password bridge:** Native import/export for all six original Cup/World Series formats, using the cartridge's packing, checksum, and restore routines. Requires unmodified retail gameplay and settled eligible screens.

See [campaign saves](CAMPAIGN_SAVES.md) and [password interoperability](PASSWORD_BRIDGE.md) for controls, limitations, storage, and verification.

---

## Gameplay tweaks (implemented)

The overlay now has **Gameplay Tweaks** with independent, persistent goalkeeper
and formation/substitution-aware player AI switches. Both are off by default,
can be combined, and apply immediately. Bounded target adjustments run in both
execution tiers and exclude human-controlled actors. Their enabled combination
is checked by saves and retail-password eligibility. See
[the gameplay tweaks guide](GAMEPLAY_TWEAKS.md) for scope and validation.

## 4. Quality of Life (QoL) Features

- **Instant Rematch (proposed):** Native Restart resets the game; it does not recreate the same match directly.
- **Intro Skip (proposed):** The persisted `skip_intro` setting has no runtime consumer.
- **Native pause (implemented):** Overlay pauses guest execution. Complete acceptance across every cutscene/replay and Android recreation is pending.
- **Controller Remapping (implemented):** In-game per-player preset/custom profiles, thresholds, P1 keyboard remapping and touch layout controls. Physical device acceptance remains pending.
