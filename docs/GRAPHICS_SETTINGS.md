# Graphics and readability

This guide describes v0.4.0-beta.1, including enhanced running, Color Boost,
CRT Strength and widened coin-toss/halftime presentation.
Open the overlay with Escape/F1 or Guide/Back+Start, scroll to **Graphics /
Readability**, and use Left/Right to change a setting. Main and Graphics pages
scroll to keep their selected row visible. Settings save immediately.

## Visual presets and preview

**Visual Preset** applies Original, Sharp or Enhanced. The displayed preset is
derived from the active settings: changing a constituent setting shows Custom.

| Preset | Presentation |
| --- | --- |
| Original | 4:3, nearest filtering, 1x intermediate, readability off, original radar and running animation. |
| Sharp | Original presentation and running animation with Sharp filtering and automatic prescaling. |
| Enhanced | 16:9 widened view, Sharp filtering, enhanced running animation, ball shadow and selected-player labels on, 2x radar at bottom right with 75% background opacity. |

Presets retain output resolution, overlay text size, fullscreen, VSync, engine
mode, audio, controls, mods and gameplay settings. They set integer scaling and
the standalone scanline switch off. **Reset Graphics** restores Original plus
Auto window output and Auto overlay text, retaining configured window dimensions,
fullscreen and VSync.

**Running Animation** selects Original or Enhanced and saves as
`enhanced_running_animation=0` or `1`. New installs and older configuration files
default to Original. Enhanced adds intermediate palette-indexed lower-body poses
to the verified eight-direction running/dash loop (hold **Y / Dash**), derived
from sprites in the loaded ROM. Eight original keyframes plus eight new
intermediate poses make a 16-pose cycle per direction. Ordinary walking,
goalkeepers, shots, tackles, special states and other scenes stay original.
It changes the displayed poses while retaining the game's original movement,
animation timing and simulation. It does not replace other actions or install
external sprite artwork. Original and Sharp presets, and Reset Graphics, disable
it; Enhanced enables it. Changing this setting independently makes the visual
preset Custom. The illustrative Preview remains a static sample, so inspect the
animation during a match.

**Preview** opens an illustrative sample pitch showing the current aspect,
filter, ball effects, player-label size and radar controls. It is labeled Sample:
it is a paused diagram, not a screenshot or new sprite artwork.
Enter, Escape, gamepad Confirm/Back or a tap returns to Graphics. Tiny layouts
keep the sample clipped within the same safe rectangle as other overlay pages.

## Resolution and scaling

**Window Output** selects Auto, 1280x720, 1920x1080, 2560x1440 or 3840x2160 on
desktop. Auto uses configured window dimensions; manual resizing remains
available. These are window sizes, not exclusive display modes. Desktop
fullscreen follows the monitor's current resolution; Android follows its device
display and shows an inactive Device Display row. The footer reports the actual
renderer output dimensions, including high-DPI pixels.

**Aspect** controls the presentation proportions. **Integer Scaling** is now an
independent setting. It uses whole multiples of the native image height while
retaining the chosen aspect ratio; 4:3 horizontal pixel correction remains
possible. Choose square pixels or the legacy integer aspect for uniform square
pixel multiples. A window smaller than the native image falls back to fitting
the whole image instead of cropping it.

**Game Filter** offers Nearest, Linear, CRT Scanlines and **Sharp**. Sharp
first enlarges by an integer factor with nearest sampling, then uses linear
filtering to fit the viewport. It reduces fractional-scaling harshness without
the full blur of filtering the original small surface directly. Its automatic
factor is bounded to 8x. **Sharp Minimum** chooses the minimum intermediate
factor; the actual factor can increase with output size. CRT and HD tiles also
use the selected intermediate scale. Nearest/Linear without HD replacements
keep the native game surface and mark the intermediate setting inactive.

Intermediate factors are 1x, 2x, 3x, 4x, 6x and 8x. Dimensions use the actual
game width, including widened views. For example, 8x of the original 256x224 is
2048x1792; it does not mean 3840x2160 or new high-definition sprite artwork.
The footer reports the actual intermediate surface separately from output.

**CRT Strength** selects 0%, 25%, 50%, 75% or 100% when CRT Scanlines is
selected. 100% retains the previous CRT darkening; 0% disables its darkening.
This setting saves as `crt_strength`. **Color Boost** saves as `color_boost` and
can be switched independently of the filter. All visual presets leave Color
Boost off. Reset restores CRT Strength to 100%. Ball Outline has been removed;
legacy `ball_outline=1` is ignored and saved back as 0.

**Overlay Text** selects Auto or 1x–4x. Menus and notifications are composited
onto a separate output-sized surface after the game filter, using whole-pixel
glyph scaling. Their proportions and clicks no longer depend on game aspect or
filter. Specialized password/control/match pages retain their original logical
layouts and receive the same sharp scaling. Small screens reduce scale and
scroll the main/Graphics pages. This is crisp bitmap text, not a new vector font.
Android menus and notifications fit within reported cutout/system-bar safe
insets, subtracting space already consumed around the SDL surface. The same
safe rectangle is used for touch hit-testing; device acceptance remains pending.

Field-of-view changes while paused apply to game scanout when play resumes;
the paused backdrop retains its captured width so it cannot be uploaded with an
incorrect pixel stride. Output and overlay changes apply while paused.

## Match readability

All four switches default **Off**, and radar defaults to **1x**. They can be
combined independently and do not change save compatibility or game state.

| Setting | Behavior |
| --- | --- |
| Color Boost | Optional modest saturation increase for game artwork; defaults off. Host overlay text retains its original colors. |
| Ball Shadow | Strengthens the ground shadow for an airborne ball; avoids painting over the ball when it is on the ground. |
| Player Markers | Adds distinct colored P1–P4 labels and markers for human-controlled selected players. Human ordering follows the original controller processing order. |
| Player Names | Labels selected human players from the match's actual roster-name buffers, including substitutions and loaded mod names. Unsupported/invalid records are omitted. |
| HUD Scale | Enlarges the added selected-player text/markers from 1x to 3x. Does not resize the cartridge scoreboard or all original HUD artwork. |
| Radar | 1x preserves the original. 2x/3x draws an enlarged map, proportionally fitted to the available native surface. |
| Radar Position | Bottom center/left/right or top left/right for the enlarged map, below the original top HUD band. |
| Radar Opacity | 25%, 50%, 75% or 100% background opacity for the enlarged map; map lines and team/ball dots remain opaque. |

Readability rendering is limited to the verified live match scene and uses the
same previous-frame object generation as native scanout. It does not label
every CPU player or add new ball physics. Offscreen drawing is clipped.
Labels avoid each other, markers and both radar areas, as well as the top 24
native HUD lines. A crowded surface can suppress a label when no unobstructed
space exists. Team dots use the actor's actual roster-team record.

The cartridge radar remains visible when the enlarged map is moved: the final
framebuffer does not contain the pitch pixels hidden beneath it. Position and
opacity settings apply only to the enlarged overlay; 1x keeps original pixels.

## Widescreen reliability

Expanded pitch/stadium rendering keeps the native center and extends metatiles
and supplemental sprites at the sides. Locomotion continuation now observes
complete animation descriptors, pairing geometry with the correct ROM graphics
uploads. It distinguishes player travel from camera-only movement and restores
temporary VRAM/OAM changes before executing more game code.

Verified culled player states now decode their original bank-$82 direction,
frame and duration tables without needing a previously learned cycle. Native
timer, action and direction changes take priority; completed one-shot actions
retain their final pose. Other states keep their native descriptor, with learned
locomotion as the fallback for unsupported scripts. This does not widen the
original simulation's update boundaries. Wider views remain experimental and
cannot guarantee every offscreen action. 4:3 remains the fidelity baseline; Authentic 320 remains a
conservative widened option. Menu and pre-match scene guards remain in place.
The 21:9 preset currently caps the game surface at 504x224. It preserves that
surface's proportions with borders on a wider monitor, rather than stretching
the artwork to fill every ultrawide display. The 512-pixel background tile ring
must hold all visible columns, including a partial tile at each scroll phase;
124 pixels per side is the largest safe margin. PPU scratch buffers have been
enlarged accordingly, outside serialized game state.

Scene/submode/layout changes now discard the prior frame's object history
immediately, including the first frame of a stats or pre-match transition.
Inactive or reused actor slots drop their previous locomotion evidence. Resetting
an open rendering transaction restores VRAM/OAM before clearing its history.
Cold valid ROM descriptors can render their authored graphics/geometry before
any animation cycle has been learned. Only the verified animation handlers
receive authored continuation; arbitrary state-dependent actions are not inferred. See the reproducible
[match graphics checks](GRAPHICS_MATCH_ACCEPTANCE.md) for tested transitions and
remaining full-match coverage gaps.

## Penalty camera

With widened gameplay enabled, the goal-facing penalty screen now extends the
original tiled crowd, stadium advertising and grass into 16:10, 16:9 and 21:9
margins. The goal/net, kicker, keeper and HUD retain their original size and
center positions. This uses the dedicated penalty BG2 layout, not the ordinary
pitch metatile reconstruction; it does not change aiming, shots or camera bounds.
The same layout guard covers shootouts and original awarded-penalty submode.
4:3 and widened-gameplay Off retain their original presentation.

Real-ROM acceptance covers shootout setup, a human shot, the opponent's turn and
the following turn across all three wider aspects. The shot sequence compares
consecutive frames to the original center. An actual awarded foul-to-penalty
transition and a completed shootout remain unverified; the awarded-penalty layout
selection is covered by native fixtures and original-code tracing.

## Display recovery and timing reports

SDL device/target-reset events invalidate all presentation textures. The next
presentation rebuilds and uploads game, intermediate and overlay textures,
including when the game is paused. Android background/resume still leaves the
game in its pause overlay until explicit Resume.

For reproducible performance evidence, run a graphical session with an explicit
config and add `--graphics-report report.json`. Combine `--frames 1200` and
`--auto-start 60` to end after a fixed game replay. The report records simulation
frames, presentation count, actual output/native/intermediate dimensions, filter,
renderer resets and mean/maximum presentation time. CPU time excludes the
`SDL_RenderPresent` call; total time includes it and can include VSync waiting.
These are renderer measurements, not simulation FPS or input-latency claims.
Headless runs have no SDL presentation and produce zero presentation samples.

## Verification and limits

Native harnesses cover configuration, viewport fitting, sharp prescaling,
overlay pixels/clicks, small-screen scrolling, notification timing, readability
clipping and animation/PPU transaction safety. Real-ROM checks compare all five
viewports and prove identical guest WRAM and native-center pixels at frames 600
and 1200, including the enlarged 504-pixel view.
Readability on/off changes actual match pixels while preserving guest WRAM.

Windows/Linux builds and Android arm64-v8a/x86_64 APK builds are exercised.
Physical high-DPI display, controller ergonomics, Android touch/cutout layouts
and full-match visual acceptance still require device testing. High-refresh
interpolation, a shader suite, HDR and general HD player replacements remain
outside this milestone. See [acceptance evidence](ACCEPTANCE_TESTS.md).

## Coin-toss introduction

Current sources widen the verified pre-match introduction and actual hand/coin
minigame for 16:10, 16:9 and 21:9. Select one of those Aspect settings with true
widescreen enabled. Wider sections of authored crowd artwork extend
into the added columns, retaining complete close-up fans and their variation;
the original panels, team sprites, hand/coin action and
framed display retain their native placement and clipping. This is a wider
background presentation, without new gameplay or replacement stadium artwork.
Unsupported presentation layouts retain their prior fallback.

The sky-to-television vertical scroll retains the native scroll timing. The
side tiles follow the same vertical scroll; the television is never repeated.
Crowd-flag sprites are explicitly clipped at the original scene edges.

## Halftime statistics

True widescreen also extends the verified halftime/fulltime statistics
background for 16:10, 16:9 and 21:9. The stadium's outer tiles continue into
the margins; the statistics card, score, team labels and native sprites keep
their original placement and clipping. This is a scenery extension, without
additional visible simulation or a stretched card. Unsupported/loading layouts
retain their fallback until the verified card layout is ready.
