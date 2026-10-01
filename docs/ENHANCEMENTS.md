# ISSD Native — Enhancements & Modern Features

This document combines implemented enhancements and older design proposals.
Entries outside the save and gameplay tweaks sections are not a certification of current
behavior; use [the README](../README.md) for implementation status.

---

## 1. Visual & Presentation Enhancements

### Resolution & Scaling
- **Integer Scaling:** Crisp, uniform pixel scaling (1x, 2x, 3x, 4x) avoiding pixel distortion.
- **Aspect Ratio Control:**
  - `4:3 Aspect (CRT standard)`: Scaled with authentic SNES aspect ratio.
  - `8:7 Aspect (Pixel perfect 1:1)`: Displays square pixels directly.
  - `Widescreen (16:9 / 16:10)`: Expanded horizontal viewport with extended pitch visibility.
- **CRT Shader & Scanlines:** Configurable scanline strength and aperture grille filters for authentic retro display feel.
- **Borderless / Fullscreen:** Smooth borderless windowed mode and exclusive fullscreen.

### Refresh Rate & Fluidity
- **Decoupled Simulation & Rendering:** Authoritative 60 Hz gameplay simulation with interpolated 120 / 144 / 165 / 240 Hz presentation rendering for buttery smooth camera scrolling.
- **Eliminated Slowdown:** Hardware cycle constraints removed in Enhanced Mode while preserving authentic simulation physics.

---

## 2. Audio & Sound Enhancements

- **Crystal-Rate Host Resampling:** High quality band-limited sinc/linear resampling targeting 44.1 kHz, 48.0 kHz, and 96.0 kHz sound cards with zero pitch drift and no buffer starvation.
- **Volume Controls:** Independent slider controls for Master Volume, Crowd / SFX Volume, and Background Music.
- **MSU-1 / CD Audio Stream:** Support for uncompressed CD-quality orchestral and modern ISSD soundtrack replacement packs.

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

- **Instant Rematch / Quick Restart:** Skip lengthy menu navigations to immediately restart a match with identical teams and stadium conditions.
- **Intro Skip Option:** Optionally bypass Konami logo and opening cinematics on startup directly to the Title Screen / Main Menu.
- **Universal Pause:** Pause anywhere safely during cutscenes, replays, or gameplay.
- **Controller Remapping:** Interactive in-game input configurator with per-player controller profiles.
