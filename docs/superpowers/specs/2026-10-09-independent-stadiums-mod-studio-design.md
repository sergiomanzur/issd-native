# Independent stadiums and Mod Studio improvements

## Purpose and approved scope

The user approved improving the existing modding tool and implementing truly
independent stadium geometry and artwork. The intended user can create a stadium,
see what its settings mean, assign its assets, validate the pack, and test it in
the native game without editing JSON or managing hashed tile files manually.

Success requires gameplay differences, not merely different labels or previews.
Two added stadiums derived from the same original layout must have different
playable boundaries and stadium-specific artwork. The original eight stadiums,
existing roster packs, global HD packs, campaigns, replays, and save/load remain
regression cases. The existing 32-stadium ceiling remains.

The work extends Mod Studio and the original engine; it does not replace either
with a new application or renderer. No release publication is part of this work.

## Evidence and current limitations

`tools/mod_studio/app.py` is a Tk/Pillow editor with a tree and one scrolling
detail panel. Stadium editing currently exposes names and displayed yard values.
Artwork is in a separate tile dialog. Its duplication action clones a stadium
without allocating a new ID, and addition can reuse the last occupied slot when
capacity is exhausted. Help text has drifted from runtime capabilities.

The runtime preserves logical stadium identity at WRAM $1FA2 but initializes
the playable template through $0086 = logical ID & 7. Original constructor data
controls world maps, pitch geometry, camera limits, palettes, and presentation.
Introduction layout 8 is a separate original use and must remain intact.

Previous Ghidra evidence identifies construction paths 8B8000, 8386BF, 83B082,
83B0D8, 8B8C90, 8B82AA and related palette/presentation routines. These are
starting points, not proof that all physics consumers have been identified.
Actual reads, writes, generated AOT callers, and replay reconstruction must be
traced before implementing geometry changes. See `docs/EXTRA_STADIUM_ACCEPTANCE.md`.

Global HD replacement keys currently identify graphics and palette, not stadium.
Consequently two logical stadiums sharing a template also share replacements.
The existing compositor preserves pixel ownership; that behavior must survive.

## Architecture

### A. Validated stadium profile

Add an optional versioned `stadium_profile` to each stadium entry. Legacy entries
without this object retain their existing behavior. The profile contains:

- `version`: 1.
- `base_layout`: explicit original playable template, 0 through 7.
- `geometry`: playable pitch rectangle, goal positions, penalty/goal-area
  markings, camera limits, and world/scenery extent in engine world coordinates.
- `artwork`: a relative path to a stadium asset manifest.

The precise geometry field names and safe numeric limits must come from the
constructor/physics trace and be frozen before writing the implementation plan.
This is an investigation dependency, not permission to silently invent limits.
The trace must produce a field contract specifying coordinate units, origin,
consumers, allowed ranges, and the associated map/VRAM budget. Existing
`pitch_length` and `pitch_width` remain display metadata; do not silently change
their historical meaning. The editor must distinguish those values from playable
geometry and offer a measured conversion only if one can be established.

The native loader, command-line validator, and editor use equivalent field and
cross-field rules. Reject invalid rectangles, goals outside the pitch, invalid
markings, impossible camera extents, unsupported versions and oversized maps.
Fail the profile with an actionable diagnostic instead of showing a custom name
while silently playing an unmodified layout. Keep unknown editor fields on save.

### B. Original-engine construction and gameplay

Introduce a focused stadium runtime module with these responsibilities:

1. Resolve enabled profiles by logical ID using the existing pack precedence.
2. Construct geometry, map and art data for the selected profile from the chosen
   original template and validated overrides.
3. Apply it at proven original construction boundaries and update every measured
   dependent gameplay value, including ball-out decisions, restarts, goal
   detection, player bounds and camera clamps.
4. Reconstruct the same profile for replays, transitions and restored saves.

Prefer original table relocation and cartridge-data generation where consumers
permit it. Use a narrowly scoped native hook only where a traced dynamic lookup
cannot safely be represented by relocated data. A hook must run independently
of optional AI/bugfix toggles. Trace AOT and interpreter routes explicitly; do
not rely on changing ROM bytes that generated C still reads as baked constants.
Generated recompilation files are not hand-edited.

World maps and metatile data must stay within measured allocation/VRAM limits.
Do not increase ROM capacity or change mapping without a separate documented
mapping assessment. Preserve the introduction/cutscene layout and stock tables.
Rendering and widescreen reconstruction consume the same selected profile.

### C. Stadium-local artwork

Add a portable asset manifest resolved relative to the pack. It describes the
stadium's background layers/map references and palette/tile resources, plus
optional HD replacements. The editor imports ordinary PNG/BMP files and exports
the runtime format; users are not required to name images by hashes.

Normal-resolution artwork must work at internal 1X. Convert imported native art
to the measured SNES palette/tile budgets with a preview of the converted result.
Report palette loss and allocation overflow before saving a runnable profile.
Do not promise arbitrary unlimited-color images in the original PPU.

Stadium-local HD lookup uses logical stadium identity in addition to the original
tile key. A local replacement overrides a global replacement for that stadium;
an absent local replacement falls back to the existing global pack, then native
art. Clear derived caches on stadium/context changes. Original sprite ownership,
transparency, flips, layer priorities and color math remain authoritative.
Scenery in widened margins must use the selected stadium's data and assets.

Assets and manifests use portable pack-relative paths. Validate actual file
format, dimensions, tile/map references and resource budgets. Pack export copies
dependencies, rewrites paths, and excludes the user's cartridge and saves.

### D. Editor workflow

Keep Tk/Pillow and the existing pack model. Split new stadium UI, profile model,
asset compilation and game-launch responsibilities into focused modules rather
than growing `app.py` further.

The stadium workspace has a persistent visual preview and three sections:
Identity, Geometry, and Artwork. A beginner starts by cloning an original
template; the editor allocates a free logical ID and adjusts stadium count.
At capacity, addition/duplication explains the limit and makes no changes.

Geometry controls update a top-down preview showing pitch, goals, markings,
scenery extents and camera bounds. Numerical controls and draggable handles edit
the same model. Invalid intermediate edits are visible but cannot be launched.
Provide reset-to-template and clear distinction between display yards and engine
coordinates. Preview the compiled map/art rather than claiming a screenshot is
the result of unimplemented geometry.

Artwork import, palette preview, region/layer selection and local HD replacements
live in the selected stadium workspace. Preserve the existing tile dialog for
advanced/global pack editing. Missing assets show their paths and repair actions.

Add document undo/redo for edits, addition, duplication and deletion, with grouped
drag/typing operations. Save remains atomic and preserves unknown fields.
Inline validation identifies the affected stadium/control, supplemented by the
same full pack report used by `validate_mod.py`. Update outdated help text.

`Test in game` validates and stages a self-contained copy into an owned test
directory, then launches the chosen native executable with the user's ROM and
isolated config/mod/save paths. It must not overwrite personal configuration,
pack files or saves. Use existing input scripting to reach the original stadium
picker and select the chosen entry; do not inject guest RAM to fake selection.
Report launch errors and provide the log. Process launch is asynchronous so the
editor stays responsive. Stop only the process owned by that launch.

## Compatibility and persistence

No-profile packs and the original eight layouts remain behaviorally unchanged.
The highest enabled stadium count remains effective. Later packs override the
same logical stadium, and geometry/art resolution follows that same precedence.

Gameplay compatibility must include canonical effective profile/compiled-data
identity, even if a field is supplied by host hooks rather than patched ROM bytes.
Changing gameplay geometry rejects incompatible saves/passwords without rewriting
them. Cosmetic HD-only changes do not unnecessarily invalidate gameplay saves.
Save/load reconstructs derived assets/caches and preserves the selected profile.
Document compatibility consequences and test actual owned cloned save files.

## Acceptance and evidence

1. Trace and publish the coordinate/consumer contract, allocation budgets and
   AOT/interpreter routing decisions. Unknown geometry consumers block claiming
   independent geometry complete.
2. Author two distributable test profiles with the same base layout, visibly
   different original artwork and measurably different pitch/camera geometry.
   Do not distribute extracted cartridge assets as the examples.
3. Select both through original menus. Demonstrate ball-out/restart and goal
   behavior against their actual boundaries, not just different WRAM bytes.
4. Verify isolated artwork at 1X and with HD enabled: changing one stadium must
   not change the other stadium or its stock base layout. Exercise sprite
   occlusion, fading, scrolling and widescreen margins.
5. Complete a natural match on each custom profile, including halftime,
   fulltime, an actual goal replay and native save/load. Check 4:3, 16:10, 16:9
   and 21:9. Record any checkpoint continuation honestly.
6. Regress the original eight stadiums, existing added aliases, introductions,
   substitutions, shootouts, campaign transitions and global HD behavior.
7. Test the editor's full author/import/edit/undo/validate/save/reopen/export/test
   workflow, duplicate allocation and full-capacity rejection. Inspect actual GUI
   screenshots; a skipped Tk test is not evidence of a usable packaged editor.
8. Package and launch the Windows editor with its bundled Tcl/Tk, Pillow,
   validators and data. The current host Python Tcl installation has a missing
   theme file; resolve the build/runtime environment and prove the executable
   opens rather than counting that failure as a passing GUI check.
9. Build Windows, Linux and Android runtime artifacts; verify packaging and
   platform startup where available. State device gameplay coverage explicitly.

Use targeted failing tests before implementation, then focused integration
checks. Run the full project suite once the integrated source is stable; avoid
repeating hour-long acceptance runs after documentation-only changes.
Retain ROM-private investigations and native snapshots in ignored build folders.

## Sequence and responsibilities

First establish the geometry/asset contract with read-only source/Ghidra tracing.
Then implement loader/validation and runtime construction, stadium-scoped artwork,
the editor workflow, packaged GUI verification, and integrated native acceptance.
The implementation plan must give exact files, dependency order, failing tests and
commands after the trace resolves the contract. Preserve all existing dirty work.
The user has requested execution in this chat; no new sidebar task is needed.

## Scope boundaries

This delivers independent configurable 2D stadiums within measured original
engine budgets. It does not add 3D geometry, arbitrary sprite replacement,
unlimited maps, additional team slots, new competition schedules or new menus
inside the original game. These exclusions do not excuse missing pitch physics,
art isolation or editor usability within the approved stadium scope.
