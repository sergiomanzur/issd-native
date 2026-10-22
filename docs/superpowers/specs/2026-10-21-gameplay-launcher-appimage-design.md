# Gameplay options, broadcast launcher, and Steam Deck AppImage

## Request and intent

Implement the three accepted ROM-hack-inspired feature groups: investigate and
correct the multiplayer penalty exploit; optional short goal kicks and
final-attack protection; team-strength summaries and broader duration settings.
Adapt the pre-game launcher behavior from Mabec-games/iss-deluxed with a completely
original retro football broadcast presentation, the visual direction selected by
the user. Add the changes necessary to build a Steam Deck AppImage.

This is one coordinated delivery with independently verifiable gameplay, launcher,
and packaging components. Original gameplay remains the default. Windows, Linux,
SteamOS/Steam Deck, and Android are the supported-platform scope. macOS and iOS
ports are not part of this request because the project does not currently support
them. Hardware acceptance must be distinguished from compilation and automation.

## Gameplay behavior

1. Trace the actual penalty controller routing, demonstrate the reported P1/P2
   interference with original cartridge code or an input replay, and fix its
   cause. Add only a reproduced correction to the existing Original Bug Fixes
   bundle. Test both shooters/sides and verify legitimate keeper inputs survive.
   Penalty replay input behavior is a separate question; do not mask inputs
   globally or claim that unrelated replay reports are fixed. If the claimed
   exploit cannot be reproduced, deliver the investigation evidence explicitly
   without inventing a correction.
2. Add a saved, default-off Short Goal Kicks option. During an eligible human
   goal kick, the native B/pass action offers a grounded short distribution.
   Preserve the other kick actions, AI behavior, and replay semantics. Use the
   existing per-player control mappings rather than physical button assumptions.
3. Add a separate saved, default-off Final Attack Protection option. At the
   original period-end decision, allow a controlled attack in the opposing
   penalty area to finish. End protection when control/territory is lost, play
   stops, or a bounded extension expires. Builder default: at most ten seconds
   of simulation time; period endings, extra time, and shootouts need explicit
   coverage. Do not alter unrelated physics or AI.
4. Gameplay-affecting options participate in save/favorite compatibility and
   original-password eligibility. Avoid unrecorded host timers: derive extension
   state from serialized guest data or serialize any added deterministic state.

## Team strength and duration

Show attack, midfield, defence, overall, and one-to-four-star strength summaries
for the highlighted team in native team selection, with a saved visibility
option. Calculate them from the applied roster/formation data, not mod titles or
hard-coded national rankings. Builder defines and documents the deterministic
formula, goalkeeper treatment, rounding, and star thresholds. Support stock,
replacement, added, and assembled all-star teams; do not show stale or fabricated
ratings if an unsupported record cannot be resolved.

Add an explicit native duration override with Original plus 1, 2, 3, 5, 7, 10,
15, and 20 minutes, without renumbering existing stored cartridge-duration enums.
Builder default: a minute setting uses the original game's full-match convention;
preserve its accelerated display clock, halftime, and extra-time relationships.
Apply at a verified match-construction boundary for exhibition, International
Cup, and World Series. Do not edit the duration of a match already in progress
or silently change scenario/training challenge clocks. Original preserves the
cartridge's selected duration. Record the effective override in compatibility.

## Original broadcast launcher

Adapt Mabec commit 54344cbddf6a7f3e02cee2000b60750d4f663be0 as a behavior reference:
pre-game ROM selection/validation, Play, settings, and controls with controller
navigation. Retain appropriate attribution for any adapted source. Do not import
the fork's online play, branding, artwork, forced presentation defaults, or removal
of the in-game overlay. Our existing mods, saves, control profiles, and settings
remain accessible.

Original visual treatment: deep navy backdrop, cream bitmap lettering, green
pitch diagram, amber focus bars, a broadcast-style title/header and information
panels. Draw it in the existing SDL stack with scalable code-native artwork.
Use the ISSD Native name. Primary navigation: Play, Choose ROM, Presentation,
Controls, Gameplay, Mods, and Quit; builder may group secondary settings while
keeping these functions reachable. Show the selected cartridge and clear errors.

Use one shared launcher implementation for all supported platforms, including
keyboard, mouse, gamepad D-pad/stick with repeat control, and touch. Layout must
remain readable at Steam Deck 1280x800, desktop 720p/1080p, and Android landscape
sizes with safe insets. Android uses its existing Storage Access Framework ROM
import and lifecycle instead of desktop dialogs. Deck Gaming Mode needs a usable
fallback ROM browser when desktop dialog programs are absent. Handle controller
hotplug and clear held launch input before passing control to the game.

Display launcher during ordinary interactive startup; headless/automation and an
explicit --no-launcher path bypass it. Preserve the existing overlay during play.
Avoid creating an incompatible second Android window; reuse platform-compatible
SDL window ownership as necessary. CLI ROM paths and config selection continue
to work. ROM validation must describe and preserve the supported baseline rather
than silently accepting unsupported ROM hacks.

## Steam Deck packaging

Adapt the useful AppDir/AppRun packaging concepts from Mabec, replacing its
machine-specific Debian/tool paths and branding. Provide configurable build,
output, and tool locations, dependency discovery, desktop metadata, an original
icon, license/attribution material, and ROM-free assets. Build against the
existing Steam Linux Runtime sniper or another validated compatible glibc floor;
never package an arbitrary newer-host executable as Deck-compatible.

Use a writable per-user XDG data directory for saves/configuration and keep
bundled assets accessible from the read-only AppImage. Preserve user mods on
updates; never recursively replace the user's mod directory. Resolve supplied
relative paths before changing directories. Support --appimage-extract-and-run
for environments without FUSE, and document adding the AppImage as a Non-Steam
Game and using Steam Input. Do not bundle the ROM or host GPU/glibc libraries.

## Verification and delivery

Use focused tests that execute original cartridge paths for penalty routing,
goal kicks and period-end behavior; test off/on, both sides, repeated periods,
save/load, and interactions with existing tweaks. Measure actual duration across
halftime and campaign construction. Test ratings against quantized applied data
and mod stacking. Exercise launcher navigation, rejection/cancellation, input
handoff, and responsive rendering, including Android build integration.

Build Windows, Linux, Android ABIs, and the Steam Deck AppImage where local
toolchains permit. Test AppRun with paths containing spaces, writable state,
preservation of user mods, dependency resolution, and ROM exclusion. Capture the
launcher for visual inspection. Report exact passing checks, unverified hardware,
and any concrete toolchain blocker. Update user-facing guides and attribution.
Do not claim device certification from a successful build alone.

## Decisions left to the builder

Exact hooks, module boundaries, original numeric tuning, strength formula,
responsive layout details, and packaging tool selection are engineering choices
within the behavior above. Inspect existing regression expectations before
changing shared interfaces. Keep changes reviewable; no unrelated network play,
expanded team capacity, tournament redesign, or new supported OS is included.
