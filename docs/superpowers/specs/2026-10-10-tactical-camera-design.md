# Tactical cameras for 1.0

## Outcome and scope

The user approved two wide tactical zoom-outs as part of the 1.0 work. Add
Classic (existing behavior), Tactical (4/5 scale), and Tactical Wide (2/3
scale). The tactical views reveal additional world content in both axes;
they preserve the original shear projection and simulation camera tracking.
Changing the camera must not change guest execution, input, audio timing,
collision, activation, replay payloads, or match outcomes.

At 16:9 the existing logical view is approximately 398 by 224 pixels.
Tactical requires approximately 498 by 280 world pixels; Tactical Wide
requires approximately 597 by 336. Exact dimensions follow the existing
aspect geometry and integer rounding. This exceeds the current 504-pixel
safe tilemap-ring width in the wider mode, so expanding that ring in place
is insufficient. The same calculation applies at 16:10 and 21:9.

## Selected approach and alternatives

Use a separate presentation viewport backed by direct world sampling. Reuse
the existing metatile definitions, tile graphics, palettes, animation
resolution, stadium artwork selection, and sprite composition rules. Sample
the complete tactical viewport independently of the 512-pixel map ring and
8-bit sprite vertical coordinates. This preserves classic scanout while
allowing genuinely larger coverage.

An alternative is tiled PPU scanout with several shifted camera transactions.
It reuses scanline composition but introduces overlapping sprite admission,
scroll wrapping, and seams; it is not selected. Scaling the existing rendered
frame is simpler but reveals no additional field and does not satisfy scope.

## Interfaces and ownership

Introduce a focused host camera module for mode, rational scale, viewport
bounds, coordinate conversion, scene eligibility, and composition. Keep world
tile and object decoding shared with widescreen rather than copying their
implementations. Expose read-only presentation helpers where needed. Separate
world-space geometry from native OAM packing so tactical actors can use signed
coordinates beyond the original clip and wrap boundaries.

The expanded renderer samples BG1/BG2 through stadium world maps and preserves
tile flips, transparency, priorities, palette selection, brightness, and
relevant color math. Goal/net geometry and actors must share the same origin
and scale, including height, shadows, and ordering. Custom geometry and artwork
must use the active stadium data rather than hardcoded retail pitch assumptions.
Avoid rendering stale vertically culled auxiliary records solely because their
coordinates intersect the expanded view; require valid frame identity/pose.

All presentation transactions must restore original PPU VRAM/OAM/register state
before guest code resumes. Use the same previous-frame WRAM generation as the
native OAM and replay companion. No guest camera patch or simulation hook may
be used to reveal the additional field.

## Composition and user settings

Keep the output aspect ratio independent of camera scale. Calculate the larger
world viewport around the existing native camera center, render it, then scale
the field into the selected output area. Preserve readable scoreboard, timer,
replay controls, notifications, and touch controls using existing host overlay
capture where applicable. Classify field sprites separately from HUD objects.
Maintain the existing filtering and output-resolution choices.

Add a persistent Camera setting with the three modes to desktop and Android
graphics settings. Classic is the default for existing configurations. Mode
changes take effect at a frame boundary and invalidate presentation caches;
they must not reset the match. Unknown configuration values resolve to Classic.
Keep the selected mode on save/load; saves restore game and recorded history,
while the user's display preference remains a host configuration setting.

Menus, tournament cards, team screens, coin toss, and unsupported cinematic
layouts retain their existing presentation. Live pitch scenes, substitutions
on the pitch, shootouts, and replay pitch scenes use tactical rendering only
when their world maps and scene records are verified. An unsupported scene
falls back cleanly to Classic for that scene without changing the setting.
Document those fallbacks rather than silently claiming tactical coverage.

## Replays and boundaries

Use recorded companion positions and animation descriptors for actors omitted
by original replay recording, aligned to the decoder cursor and previous-frame
presentation. Audit whether the current 22-player history covers ball, officials,
shadows, goal objects, and newly visible vertical areas. Extend versioned host
history only where actual recording gaps require it, preserving acceptance of
older save formats. Never synthesize old motion from stale live records.

When old recordings lack required history, preserve original actors and render
available world coverage; document that missing history cannot be recovered.
Fresh recordings must supply all newly visible active actors. Extend authored
stadium boundary surfaces according to existing world-bound rules, without
wrapping to another world page or revealing uninitialized metatiles.

## Acceptance and validation

1. Unit tests prove rational viewport sizes and actor transforms at all supported
   aspects, including negative coordinates, vertical expansion, tile boundaries,
   and viewports wider than 512 pixels. Classic remains unchanged.
2. Scene tests prove pitch content extends vertically and horizontally at both
   scales, with correct grass lines, goals, palette/priority, and no map-ring
   aliasing or sprite-coordinate wrapping.
3. Fresh natural match/replay recordings show complete players, ball, shadows,
   and verified officials across all viewport edges and internal boundaries;
   pause, rewind, mode switches, save/load, and custom stadiums remain coherent.
4. Deterministic identical-input runs produce identical full guest WRAM and
   original replay bytes for Classic and both tactical modes. Presentation
   transactions restore guest-visible PPU state.
5. HUD elements retain size and placement, while menus and campaign cards retain
   their existing rendering. Verify shootouts and substitutions explicitly.
6. Windows, Linux, and Android compile; run visual native checks on Windows and
   measure frame-time/memory overhead against Classic. Android runtime performance
   is reported as unverified unless tested on an available device. Do not claim
   1.0 readiness solely from compilation.

## Delivery

Implement and document locally on the current development branch, preserving
the pending replay/card fixes. Provide private before/after captures and test
results. Update the 1.0 readiness documentation with completed coverage and
remaining runtime validation. Do not change the public beta, bump the release
version, or publish cartridge-derived fixtures as part of this work.
