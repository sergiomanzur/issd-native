# Graphics priorities one and two implementation plan

**Goal:** Implement the approved resolution/scaling/overlay milestone and widescreen/readability milestone before netplay.

**Architecture:** Keep simulation at 60 Hz and make the improvements in host presentation. Separate output dimensions, aspect ratio, integer scaling and intermediate texture scale. Composite overlay text after filtering the game. Widescreen reconstruction uses paired animation descriptors and graphics in a reversible PPU transaction.

**Tech stack:** C11, SDL2, shared desktop/Android CMake sources, native C harnesses and pytest.

## Constraints

- Preserve the existing uncommitted gameplay, saves and match-shortcut work.
- Preserve existing configuration enum values and menu action indices.
- Changes to presentation must not change gameplay compatibility flags or guest WRAM.
- Original rendering and all readability settings remain available; readability settings default off and radar defaults to its original size.
- No netplay, CRT shader suite, general HD sprite replacement or high-refresh interpolation in this milestone.

## Tasks

- [x] Display utilities/config: actual resolution labels, output size presets, sharp integer prescaling followed by linear filtering, independent integer viewport scaling, persisted overlay/readability settings. Test small windows, invalid values, aspect ratios and legacy configuration.
- [x] Overlay: separate output-sized alpha surface, sharp scaled text, coherent mouse/touch transform, scrolling main and Graphics pages. Preserve specialized password/control/match pages. Test page navigation, setting changes, row hit-testing and borders.
- [x] Widescreen: reconstruct learned locomotion using complete descriptors rather than mismatched pose/graphics; preserve native OAM, wrapped top-edge sprites and scene guards. Test animation, no guest writes and full PPU restoration.
- [x] Readability: optional ball outline/shadow, P1–P4 markers and selected-player names, enlarged pitch radar. Read the same object generation as native scanout; validate records and clip drawing. Test disabled/unsupported scenes, player ownership, roster names, offscreen bounds and WRAM immutability.
- [x] Runtime integration: output sizing, high-DPI renderer dimensions, per-texture filters, separate menu composition, mouse/touch alignment, lifecycle and cleanup. Compile the shared Windows/Linux/Android paths and run affected regression checks.
- [x] Visual verification/documentation: render representative menus and match captures, inspect readability and clipping, update supported-feature descriptions with exact limitations and evidence.

## Review focus

Small/resized/high-DPI windows must remain navigable. Presentation settings must not affect saves. Filter switching must update existing textures. Android coordinates must match both physical output and logical touch viewport. Animation fallback must never invent a new action or corrupt sprite uploads. Output size selection controls desktop window dimensions; fullscreen/Android follow the actual display rather than advertising unsupported display modes.
