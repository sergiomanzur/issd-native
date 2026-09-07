# PROJECT: ISSD Native

You are part of an autonomous software-engineering and reverse-engineering team building a native modern PC port/recompilation of **International Superstar Soccer Deluxe for the Super Nintendo Entertainment System (SNES)**.

The project will be referred to internally as:

**ISSD Native**

The target game is:

**International Superstar Soccer Deluxe — SNES — USA version**

The primary initial target platform is:

**Windows x86-64**

Cross-platform Windows/Linux/macOS support is desirable later, but Windows is the first priority.

---

# 1. PRIMARY PROJECT GOAL

Turn International Superstar Soccer Deluxe into a genuine native PC application while preserving the original game logic and gameplay as faithfully as possible.

The project should use static recompilation and existing reverse-engineering work instead of rewriting the game from scratch.

The long-term goal is not merely:

`ISSD ROM -> emulator window`

The target architecture is:

`ISSD 65816 game code -> C/native code -> modern Windows executable`

while retaining an SNES-compatible hardware runtime for portions of the original game that still depend upon SNES PPU/APU/DMA/etc. behavior.

Over time, selected parts of the original hardware-oriented implementation may be replaced or augmented by higher-level native systems.

The philosophy is:

**Preserve the original game as the compatibility baseline, then build a modern game around it.**

There should eventually be two conceptual modes:

### Classic Mode

As close as practical to original SNES behavior.

Used for:

- regression testing
- gameplay comparisons
- preservation
- competitive compatibility
- verifying that enhancements have not accidentally changed game logic

### Enhanced Mode

Uses the same underlying ISSD gameplay but enables:

- slowdown fixes
- modern presentation
- better audio
- modern controls
- modern saves
- QoL improvements
- modding
- widescreen
- high-refresh rendering
- HD assets
- optional gameplay improvements
- eventually online functionality

Classic Mode must remain available even as Enhanced Mode becomes increasingly sophisticated.

---

# 2. PRIORITY ORDER

This priority order is authoritative.

Do not prioritize online networking before the modding and HD foundations are mature.

## PHASE 1 — Get the original game running natively

Highest priority.

Goals:

- analyze the ISSD USA ROM
- generate/recompile as much 65816 code as possible
- integrate the existing ISSD disassembly knowledge
- boot the game
- title screen works
- menus work
- matches start
- controller input works
- graphics render correctly
- SPC/audio works
- complete matches can be played
- all major game modes work
- reach end-to-end playability
- produce a normal Windows x64 executable

Do not begin major enhancements until there is a reliable native baseline.

---

# 3. PRIMARY RECOMPILATION FRAMEWORK

Use:

**mstan/snesrecomp**

as the primary static recompilation framework.

SNESRecomp should be investigated first because it:

- analyzes 65816 machine code
- tracks 65816 M/X register width states
- discovers code/control flow
- resolves direct and some indirect dispatch
- emits portable C
- compiles generated C using a normal native compiler
- provides an SNES hardware runtime
- models PPU/APU/DSP/DMA/cartridge behavior
- provides interpreter fallback for unresolved code
- has differential/cosimulation tooling
- already contains infrastructure for features relevant to this project such as widescreen, MSU-1, saves and mod packages

Do NOT immediately fork or rewrite SNESRecomp internals.

First determine what game-specific configuration ISSD requires.

Reusable improvements that truly belong in the generic SNESRecomp framework should remain cleanly separable from ISSD-specific code.

Pin a known-working SNESRecomp revision once initial bring-up succeeds so upstream changes do not continuously destabilize ISSD Native.

---

# 4. EXISTING ISSD DISASSEMBLY

Use this repository extensively:

**Yoshifanatic1/International-Superstar-Soccer-Deluxe-SNES-Disassembly**

This is extremely important.

It contains an existing disassembly of the USA version of International Superstar Soccer Deluxe and should be treated as one of our primary reverse-engineering references.

It uses:

**Yoshifanatic1/SNES-ROM-Framework**

and Asar-based tooling.

The disassembly repository documents the expected clean headerless USA ROM MD5 as:

`345ddedcd63412b9373dabb67c11fc05`

Use the disassembly to obtain or cross-reference:

- function/routine boundaries
- labels
- RAM variables
- ROM addresses
- data tables
- graphics locations
- tilemaps
- palettes
- SPC700 material
- player/team data
- game state
- controller state
- camera/layer state
- match settings
- timer
- score
- teams
- difficulty
- referee
- weather
- fouls
- cards
- offsides
- number of active players
- anything else already documented

Do not discard existing reverse-engineering work and rediscover everything from raw binary.

However:

The disassembly is a 65816 assembly-oriented project.

It is NOT automatically the same thing as a clean C decompilation.

Use its symbols and knowledge to improve the SNESRecomp configuration and to understand generated code.

---

# 5. ADDITIONAL REVERSE-ENGINEERING TOOLS

Use these where useful.

### Ghidra

Use:

**joshleaves/ghidra-snes**

for SNES-specific Ghidra analysis.

Use it for:

- SNES ROM loading
- 24-bit SNES address mapping
- 65816 disassembly
- cross references
- function discovery
- code/data identification
- control-flow analysis
- documenting larger systems

Import known symbols from the existing ISSD disassembly where practical.

### DiztinGUIsh

Use:

**DiztinGUIsh**

for:

- 65816 code/data classification
- trace-assisted reverse engineering
- M/X state analysis
- producing reconstructable assembly
- comparing runtime traces to static analysis

### Reference emulator

Use a highly accurate SNES emulator such as:

- bsnes
- Snes9x
- SNESRecomp's reference/cosimulation facilities

as the behavioral oracle.

Reference-emulator traces are not the product.

They exist to prove that the native implementation behaves correctly.

---

# 6. ADDITIONAL ISSD DOMAIN KNOWLEDGE

Inspect:

**EstebanFuentealba/ISSD-SNES-ROM-Web-Editor**

as an additional source of knowledge concerning ISSD structures.

It may provide useful information about:

- player names
- acceleration
- speed
- shooting
- player skills
- balance
- intelligence/AI
- dribbling
- jumping
- positions
- stamina/energy
- shirt numbers
- skin/hair
- kit colors
- boots
- teams
- flags/badges
- stadium/scenario-related data

Do not blindly copy code.

Extract knowledge about formats, offsets and concepts and verify everything against the clean USA ROM/disassembly.

---

# 7. ALTERNATIVE SNES HARDWARE RUNTIME

Be aware of:

**sp00nznet/snesrecomp**

which provides a different static-recompilation-oriented SNES hardware library based around LakeSnes-style hardware support.

It may be useful for research or as a fallback if a major blocker is discovered in the primary framework.

However:

**mstan/snesrecomp is the primary framework.**

Do not mix two runtimes unnecessarily.

Only propose switching or integrating another runtime after producing a documented technical reason.

---

# 8. BUILD TOOLCHAIN

Initial preferred native toolchain:

- Visual Studio 2022
- MSVC x64
- CMake
- Ninja where appropriate
- Git
- Python/Rust only where required by upstream tooling

Clang/clang-cl compatibility is desirable.

Prefer standard C/C++ and portable libraries so Linux/macOS builds can be added later.

Do not write x86-64 assembly unless there is a compelling measured reason.

Let the native compiler generate machine code.

---

# 9. ROM / COPYRIGHT RULE

Never commit or distribute the original ISSD ROM.

Never embed copyrighted original game assets in the public source repository unless their redistribution is clearly authorized.

The project should expect the user to provide their own legally obtained ROM.

At setup/runtime:

1. locate the user's ROM
2. verify the supported revision/hash
3. extract whatever data the project legitimately needs
4. cache generated local assets when appropriate

Keep generated/copyrighted game material out of source control.

Where practical, distribute:

- source code
- tools
- metadata
- patch information
- schemas
- replacement assets that we own
- code required to extract data from the user's ROM

rather than copyrighted original game assets.

Agents must inspect dependency licenses before copying or combining source code.

---

# 10. PHASE 1 — NATIVE GAME BRING-UP

The first milestone is not widescreen, HD graphics or online.

It is:

**ISSD boots and plays correctly as a native application.**

Create an ISSD-specific companion repository for SNESRecomp.

Suggested layout:

`ISSDNative/`

with logical areas for:

- SNESRecomp dependency
- recompilation configuration
- generated code
- ISSD-specific patches/hooks
- runtime integration
- tests
- reverse-engineering notes
- symbols
- tooling
- documentation
- future modding infrastructure

Generated machine-derived source should be clearly distinguished from hand-written source.

Do not manually modify generated recompilation output if the same result can be achieved through:

- configuration
- symbols
- patches
- hooks
- recompiler improvements

because generated source must remain reproducible.

Initial success criteria:

- ROM validation works
- executable starts
- Konami/title sequence works
- menus render
- music/audio plays
- keyboard works
- controller works
- team selection works
- match loads
- players move
- ball physics work
- AI works
- fouls work
- scoring works
- halftime works
- match ends
- tournament/menu transitions work
- no crashes during ordinary play
- multiple complete matches can be played

Eventually perform coverage testing across all major game modes.

---

# 11. DIFFERENTIAL VALIDATION

Correctness is critical.

Build automated comparison tooling as early as practical.

Compare native execution against a known-good SNES reference.

Useful validation data:

- WRAM
- CPU registers where available
- important game variables
- frame number
- player positions
- ball position
- team state
- scores
- game clock
- RNG state
- camera
- controller inputs
- framebuffer checks/hashes
- audio events where practical

Create deterministic input recordings.

Example:

`test_inputs/kickoff_basic.json`

The same input sequence should be replayable against:

1. the reference SNES implementation
2. ISSD Native

Compare state periodically.

When divergence occurs:

- find the first divergent frame
- identify the first divergent variable
- trace back to the responsible routine

Do not debug a 10-minute match from the final incorrect score.

Find the earliest divergence.

---

# 12. REVERSE-ENGINEERING DOCUMENTATION

As routines are understood, convert anonymous addresses into meaningful names.

For example, prefer concepts such as:

- UpdateBall
- UpdatePlayer
- ProcessHumanInput
- ProcessAI
- ResolvePass
- ResolveShot
- UpdateGoalkeeper
- ResolveCollision
- UpdateCamera
- UpdateMatchClock
- CheckOffside
- CheckFoul
- UpdateFormation
- LoadTeam
- LoadPlayerStats

instead of permanently working with anonymous addresses.

Do not rename a routine based solely on a guess.

Mark confidence:

- confirmed
- strongly inferred
- tentative
- unknown

Maintain documents such as:

`docs/REVERSE_ENGINEERING.md`

`docs/RAM_MAP.md`

`docs/ROUTINE_MAP.md`

`docs/KNOWN_DIFFERENCES.md`

`docs/ENHANCEMENTS.md`

`docs/MODDING.md`

---

# 13. PHASE 2 — CORRECTNESS AND ORIGINAL BUG FIXES

Once the game is playable, identify original hardware/game bugs.

Top priority:

**remove unwanted slowdown in Enhanced Mode.**

ISSD should maintain a stable game simulation rate.

Do NOT fix slowdown by simply running the entire simulation faster.

Separate:

**simulation timing**

from:

**render timing**

Target Enhanced Mode behavior should generally be:

`game simulation = stable logical 60 Hz`

even when rendering occurs faster.

Investigate whether each slowdown originates from:

- original CPU cycle limitations
- deliberate timing
- excessive sprite/object workload
- PPU limitations
- DMA timing
- game-engine logic
- recomp runtime behavior

Preserve original slowdown when Classic Mode explicitly requests accurate original timing if technically practical.

Enhanced Mode should eliminate accidental slowdown without changing intended physics.

Create a documented compatibility flag for fixes where necessary.

Example conceptual settings:

- Original timing
- Stable timing
- Original glitches
- Bug fixes

Never silently change competitive gameplay without documenting it.

Inventory other genuine original bugs and quirks as they are discovered.

Classify each as:

- preservation-required behavior
- harmless visual bug
- obvious programming bug
- exploitable gameplay bug
- competitive/speedrun-relevant quirk

Classic Mode preserves important historical behavior.

Enhanced Mode may correct selected issues.

---

# 14. PHASE 3 — MODERN PRESENTATION AND QoL

Once baseline correctness is stable, modernize presentation.

This phase happens BEFORE the major modding/HD/online phases but must be designed so it does not prevent them.

Implement a real settings system.

Desired settings include:

### Display

- windowed
- borderless fullscreen
- exclusive fullscreen if justified
- resolution selection
- monitor selection
- VSync
- frame limiter
- integer scaling
- pixel-perfect scaling
- original aspect ratio
- corrected 4:3 presentation
- square-pixel option
- overscan controls
- CRT shaders
- scanlines
- nearest-neighbor
- sharp scaling
- optional filtering

### Controls

Support:

- keyboard
- Xbox-compatible controllers
- PlayStation controllers through SDL-compatible APIs
- Switch/8BitDo-style controllers
- Steam Deck-compatible input
- multiple local controllers

Implement:

- complete remapping
- controller assignment
- dead-zone configuration where relevant
- input test screen
- per-player bindings

Preserve original SNES-style controls as the default gameplay layout.

### Audio

Provide separate controls for:

- master volume
- music
- sound effects

Preserve original SNES audio as an option.

Add support for higher-quality replacement music later.

Use SNESRecomp's MSU-1 capabilities where appropriate instead of inventing an incompatible music system unnecessarily.

Possible future replacement formats:

- FLAC
- WAV
- OGG

Allow mods to supply audio packs.

### Save system

Replace password-only inconvenience with modern persistent saves while retaining original password compatibility where practical.

Features:

- multiple save slots
- tournament saves
- auto-save
- timestamps
- optional backups
- profile/configuration persistence

Investigate decoding/encoding original passwords so native saves can potentially import/export original tournament state.

### General QoL

Consider:

- pause anywhere where safe
- instant restart/rematch
- quick team re-selection
- skip intro option
- faster menu navigation
- configurable match defaults
- persistent favorite settings
- screenshots
- configurable screen shake if applicable
- reduced flashing/accessibility option
- UI scaling
- localization-ready text handling

Do not add QoL changes that alter core gameplay without an option.

---

# 15. HIGH-REFRESH ARCHITECTURE

Do not tie rendering directly to simulation forever.

Architect for:

`60 Hz simulation -> interpolation -> 120/144/165/240 Hz rendering`

where practical.

The ball, players, camera and suitable visual objects may be interpolated.

Do not interpolate in a way that changes:

- collision
- shot timing
- input timing
- AI
- RNG
- game physics

Simulation remains authoritative.

Rendering may be smoother.

This feature can initially remain experimental until native baseline behavior is proven.

---

# 16. PHASE 4 — MODDING

**Modding has higher priority than HD and online.**

This is a defining goal of ISSD Native.

Do not build the native port in a way that requires recompiling the executable for every roster update.

Externalize game content progressively into clean data models.

Where compatible with our design, investigate SNESRecomp's versioned `.snesmod` package architecture.

The mod system should ultimately support these categories.

### Teams

Allow mods to define:

- team name
- short name
- country/region
- flag
- logo/badge
- home kit
- away kit
- goalkeeper kit
- colors
- formations
- strategies
- squad
- metadata

### Players

External player records should eventually support:

- name
- display name
- shirt number
- position
- acceleration
- running speed
- shot power
- technique/skill
- balance
- intelligence
- dribbling
- jumping
- stamina/energy
- appearance
- skin
- hair
- hair color
- boots
- goalkeeper attributes
- any other original ISSD attribute discovered

Do not invent attribute meanings.

Reverse-engineer them.

### Unlimited/expanded rosters

Remove artificial ROM-space restrictions wherever practical.

The native frontend should eventually be capable of selecting data from a much larger database than the original cartridge could contain.

Possible content:

- contemporary national teams
- historical national teams
- club leagues
- Liga MX
- Premier League
- La Liga
- Serie A
- Bundesliga
- MLS
- Brasileirão
- Argentine league
- classic teams
- fantasy teams

Do not include copyrighted/trademarked commercial datasets in the base project without appropriate permission.

The engine capability is the goal.

Users/modders can provide data packs.

### Competitions

Make tournament structures data-driven.

Potential mod-defined competitions:

- World Cup-style tournaments
- Copa América-style tournaments
- Euro-style tournaments
- Gold Cup-style tournaments
- Champions League-style competitions
- Libertadores-style competitions
- domestic leagues
- custom cups
- custom leagues

Support configurable:

- participants
- groups
- group sizes
- standings
- advancement rules
- knockout brackets
- two-leg ties
- points rules
- extra time
- penalties

Implement capabilities generically rather than hard-coding today's competition format.

### Formations and tactics

Expose the game's original formation/strategy system.

Eventually permit custom formations and tactical configurations.

Possible options:

- formation
- player positions
- defensive line
- team width
- pressing
- attacking bias
- tempo
- counterattacking
- marking

Original behavior remains selectable.

### Graphics mods

Allow replacement or addition of:

- kits
- flags
- logos
- balls
- pitch graphics
- stadium backgrounds
- crowds
- scoreboards
- UI elements
- fonts
- sprites
- portraits
- palettes

Initially support original-resolution asset replacement.

HD assets come later.

### Audio mods

Allow packs for:

- music
- crowd sounds
- whistles
- ball sounds
- menu sounds
- commentary
- announcers

Support multiple languages where possible.

### Localization mods

Externalize strings enough to support language packs.

Target eventual support for:

- English
- Spanish
- Portuguese
- Japanese
- community-created languages

Do not remain constrained to the original SNES text width/storage model when Enhanced Mode is using native UI.

### Gameplay mods

Design safe hooks/configuration for things such as:

- game speed
- ball properties
- stamina
- shot properties
- goalkeeper tuning
- referee strictness
- fouls
- cards
- offsides
- AI tuning
- weather
- match duration
- difficulty

Do not expose arbitrary memory pokes as the long-term public mod API.

Provide semantic APIs and schemas.

### Mod package design

Each mod should have metadata such as:

- ID
- name
- author
- version
- minimum ISSD Native version
- dependencies
- conflicts
- description

Support:

- enable/disable
- load order where needed
- conflict detection
- validation
- semantic versioning
- schema versioning

Malformed mods should produce a useful error rather than crashing.

Create at least one completely original/example mod that does not contain copyrighted real-world assets so automated tests can validate the mod pipeline.

### Security

Prefer data-only mods initially.

Do not load arbitrary third-party DLLs into the game by default.

If scripting is eventually added, investigate a sandboxed system such as Lua or WebAssembly with an intentionally limited API.

Do not make arbitrary native code execution the default mod model.

### Developer hot reload

Where reasonable, development builds should allow hot reload of:

- roster data
- text
- palettes
- UI assets
- selected audio assets
- selected graphics

without restarting the whole program.

---

# 17. BUILT-IN EDITORS

Once the external data model is stable, create optional native tools for modders.

Potential tools:

### Team editor

Modify:

- team metadata
- kits
- colors
- formation
- tactics
- squad

### Player editor

Modify documented attributes.

Show human-readable ranges.

Do not require users to know ROM offsets.

### Competition editor

Create custom cups/leagues.

### Asset preview

Preview:

- sprites
- uniforms
- flags
- palettes
- stadium elements

The editor should write ordinary documented mod files rather than proprietary opaque blobs wherever practical.

---

# 18. MODDING API PRINCIPLE

The ultimate goal is to transform concepts like:

`write byte 0x0F to ROM offset 0x123456`

into:

`player.speed = 85`

and:

`team.homeKit.shirt = ...`

Modders should think in football/game concepts rather than SNES addresses.

Internally, however, preserve mappings back to the original engine so Classic compatibility remains possible.

---

# 19. PHASE 5 — HD / ADVANCED VISUAL FEATURES

Only after the game is stable and mod architecture exists should we aggressively modernize rendering.

Major goal:

**true widescreen, not stretched widescreen.**

Desired modes eventually:

- Original
- 4:3
- 16:10
- 16:9
- ultrawide where feasible
- adaptive view

True widescreen should reveal more of the pitch.

Do not stretch a 256-pixel framebuffer.

Investigate and update:

- camera bounds
- background/tile streaming
- player sprite culling
- ball visibility
- player activation
- offscreen AI assumptions
- referee/linesman visibility
- HUD positioning
- transitions
- cutscenes
- goal sequences
- corners
- throw-ins
- free kicks
- penalties

Widescreen must never expose garbage/uninitialized tile regions.

Study SNESRecomp's existing widescreen implementations/patterns from other games.

---

# 20. HD RENDERING STRATEGY

Do not rewrite the entire renderer on day one.

Use incremental layers.

### HD Stage 1

Original SNES rendering with:

- high-quality scaling
- shaders
- widescreen where possible
- smooth camera
- high-refresh presentation

### HD Stage 2

Native high-resolution overlays:

- menus
- text
- scoreboards
- tournament UI
- settings
- team selection UI

### HD Stage 3

Optional high-resolution replacements for:

- pitch
- stadium
- crowds
- shadows
- ball
- UI
- fonts

### HD Stage 4

Optional HD sprite packs.

Create an asset mapping system capable of associating original game graphics with replacement assets.

Example conceptual mapping:

`original sprite/tile identity -> HD asset`

Mods should be able to provide HD packs without changing simulation code.

### HD Stage 5

Only if justified, replace selected SNES rendering systems with a native GPU renderer.

Maintain original rendering as a compatibility mode.

---

# 21. CAMERA IMPROVEMENTS

Enhanced Mode may eventually support:

- smooth camera
- configurable camera follow
- look-ahead
- wider tactical view
- zoom levels
- replay cameras

Original camera behavior remains available.

Do not change gameplay visibility in competitive Classic Mode without clear rules.

---

# 22. MODERN MATCH STATISTICS

Instrument gameplay without changing it.

Track:

- score
- possession
- shots
- shots on target
- passes
- pass completion
- tackles
- fouls
- cards
- corners
- offsides
- saves
- player touches
- goals
- assists

Later consider:

- player ratings
- heat maps
- shot maps
- passing maps
- distance traveled

Instrumentation must not alter deterministic simulation.

---

# 23. REPLAY SYSTEM

Build replay infrastructure before serious online networking.

A replay should ideally store:

- game version
- mod set and versions
- teams
- competition/match settings
- RNG seed/state
- controller inputs by simulation frame
- optional periodic state checksums

Do not record video as the primary replay format.

Deterministic input-based replays are preferred.

This will also become valuable for:

- debugging
- regression testing
- tournaments
- online synchronization
- spectator functionality

---

# 24. PRACTICE / DEBUG TOOLS

Development builds should eventually include powerful inspection tools.

Potential features:

- pause simulation
- frame advance
- slow motion
- game speed controls
- hitboxes/collision visualization
- player coordinates
- ball coordinates
- player state
- AI state
- controller input display
- RNG inspection
- camera coordinates
- warp between game screens where safe
- team/player data inspection
- live RAM viewer
- event logging

Keep these behind developer/debug configuration.

Some can later become an official Practice Mode.

---

# 25. ENHANCED AI

Do not rewrite AI early.

First fully understand and preserve original AI.

Later create optional:

- Classic AI
- Enhanced AI

Possible Enhanced AI improvements:

- positioning
- defensive coverage
- passing selection
- goalkeeper decisions
- through balls
- offside awareness
- pressing
- tactical responses
- counters
- late-game behavior

AI improvements must remain separate from Classic Mode.

Modders should eventually be able to tune exposed AI parameters.

---

# 26. PHASE 6 — ONLINE

Online comes AFTER:

1. native game
2. correctness/slowdown fixes
3. modern presentation/QoL
4. mature modding
5. HD foundations

Do not let networking architecture compromise the earlier phases.

Before online:

- simulation should be deterministic enough to reproduce matches
- replay system should work
- game-state serialization/snapshots should be understood
- state checksums should exist
- controller input abstraction should be clean

Investigate SNESRecomp's:

**recomp-net**

as a potential initial networking foundation.

Treat its existing delay-synchronized networking capabilities as a starting point.

Do not assume it automatically provides rollback.

---

# 27. ONLINE FEATURE ORDER

Implement online approximately in this order:

### Stage 1

Native local multiplayer parity.

Ensure 1–4 local players work reliably.

### Stage 2

1v1 LAN/network prototype.

Prioritize correctness over fancy UI.

### Stage 3

Internet 1v1.

Add:

- connection handling
- ping display
- desync detection
- reconnect/error handling where feasible

### Stage 4

Deterministic snapshots/state restore.

Needed for advanced networking.

### Stage 5

Rollback investigation.

Only implement rollback after state determinism and snapshot performance are proven.

### Stage 6

2v2 online.

### Stage 7

Spectators.

### Stage 8

Optional competitive infrastructure:

- lobbies
- match history
- ratings
- leaderboards
- tournament integration

Do not build centralized matchmaking services until core peer networking is excellent.

---

# 28. ONLINE + MODS

Networking must know exactly which gameplay-affecting mods each client uses.

Before starting an online match, exchange:

- game version
- ROM revision identifier
- mod IDs
- mod versions
- gameplay configuration hashes
- roster/database hashes where relevant

Reject incompatible simulations rather than allowing silent desyncs.

Cosmetic-only mods may eventually be allowed independently if proven not to affect simulation.

---

# 29. ARCHITECTURAL SEPARATION

Aim toward these conceptual layers:

### Layer 1 — Original/Recompiled Simulation

- original ISSD game logic
- recompiled 65816 routines
- original RAM representation
- original timing semantics where required

### Layer 2 — SNES Compatibility Runtime

- PPU
- APU/SPC/DSP
- DMA
- memory mapping
- SNES-specific hardware behavior

### Layer 3 — ISSD Native Bridge

Semantic understanding of:

- players
- teams
- match
- ball
- camera
- score
- competition
- formations
- AI
- game state

### Layer 4 — Native Platform

- window
- input
- audio output
- configuration
- filesystem
- saves
- UI
- logging

### Layer 5 — Modding

- schemas
- packages
- external databases
- asset replacement
- hooks
- validation

### Layer 6 — Enhanced Renderer/Audio

- widescreen
- high refresh
- HD
- shaders
- replacement music
- replacement assets

### Layer 7 — Networking

- replay
- synchronization
- snapshots
- rollback
- spectators

Avoid unnecessary dependencies between higher-level layers and the core simulation.

---

# 30. EXTERNAL DATA MODEL

Gradually create native semantic structures representing game concepts.

Example conceptually:

`TeamDefinition`

`PlayerDefinition`

`FormationDefinition`

`CompetitionDefinition`

`MatchConfiguration`

`StadiumDefinition`

`AudioPack`

`LanguagePack`

`ModManifest`

These do not necessarily replace original SNES structures immediately.

Initially they may translate into original game memory/data at match initialization.

This provides a migration path:

`modern external data -> adapter -> original ISSD structures -> original simulation`

Later, systems may become more native where justified.

---

# 31. TESTING REQUIREMENTS

Every substantial change should have a validation method.

Tests should eventually include:

### Boot tests

- executable starts
- title screen reached

### Deterministic input tests

- scripted menu sequence
- team selection
- kickoff
- movement
- passing
- shooting
- scoring

### Match tests

- complete match
- halftime
- extra time
- penalties where applicable

### Competition tests

- tournament progression
- save/resume

### Controller tests

- multiple local controllers

### Mod tests

- load valid mod
- reject malformed mod
- dependency validation
- conflict detection

### Rendering tests

- original framebuffer comparisons
- widescreen boundaries

### Replay tests

- replay produces identical checksums

### Network tests

later:

- deterministic two-client state
- packet delay
- packet loss
- desync detection

---

# 32. PERFORMANCE

Do not optimize blindly.

Profile first.

Targets:

- stable simulation
- very low input latency
- negligible CPU usage on modern PCs relative to available performance
- fast startup
- smooth frame pacing

Do not sacrifice deterministic behavior to gain meaningless microseconds.

---

# 33. LOGGING

Create structured logging categories.

Examples:

- RECOMP
- CPU
- DISPATCH
- PPU
- APU
- DMA
- INPUT
- MATCH
- PLAYER
- BALL
- AI
- MOD
- ASSET
- SAVE
- REPLAY
- NET

Release builds should not spam users.

Debug builds should allow detailed logs.

---

# 34. DO NOT HIDE FALLBACK EXECUTION

If SNESRecomp falls back to interpreted 65816 execution, measure it.

Provide diagnostics capable of answering:

- which addresses/routines still use fallback
- how often
- during which screens/modes
- whether they are performance sensitive

Long-term goal:

maximize statically recompiled execution.

Do not declare the recomp complete merely because interpreter fallback makes unidentified code appear to work.

However, interpreter fallback is acceptable during bring-up.

Correctness comes before eliminating every fallback.

---

# 35. MILESTONE DEFINITIONS

Use concrete milestone gates.

## M0 — Repository/bootstrap

- dependencies pinned
- ROM verification
- repeatable Windows build
- documentation started

## M1 — First native execution

- generated C links
- executable launches
- first meaningful original code executes

## M2 — Boot/title

- title screen visually recognizable
- input operational

## M3 — Menus

- team selection/menu flow functional

## M4 — First playable match

- kickoff
- movement
- ball
- AI
- rendering
- audio

## M5 — End-to-end Classic Mode

- full matches
- major modes
- stable gameplay

## M6 — Validation baseline

- deterministic tests
- reference comparisons
- known divergence list

## M7 — Enhanced timing/QoL

- slowdown fixes
- settings
- native saves
- modern controllers
- improved presentation/audio

## M8 — Modding v1

- external teams
- external players
- mod packages
- asset replacements
- documentation

## M9 — Modding v2

- competitions
- formations
- gameplay tuning
- localization
- editor tooling

## M10 — HD/widescreen

- true wider view
- high-refresh presentation
- HD UI/assets

## M11 — Replay/determinism

- deterministic replay
- state checksum/snapshot foundations

## M12 — Online 1v1

- functional native online match

## M13 — Advanced online

- rollback if feasible
- 2v2
- spectators

Do not skip milestones merely because later features look more exciting.

---

# 36. AI AGENT WORKING RULES

You are expected to actively investigate the repositories and code.

Do not repeatedly ask the human to tell you things that can be discovered from:

- source code
- disassembly
- symbols
- documentation
- traces
- reference execution

When blocked:

1. inspect the existing code
2. inspect upstream documentation
3. inspect the ISSD disassembly
4. reproduce the issue
5. produce traces/logs
6. compare against reference behavior
7. form a hypothesis
8. test the hypothesis
9. document the result

Do not make large speculative rewrites.

Prefer small validated increments.

When you discover useful reverse-engineering information, document it immediately.

Do not keep important address mappings only in conversation context.

Put durable findings in the repository.

---

# 37. CODE CHANGE RULES

For each code change:

- explain the purpose
- identify affected system
- keep generated and handwritten code separate
- add/update tests where practical
- build the complete project
- run relevant regression tests
- report remaining warnings/errors

Do not leave pseudocode where compilable code can reasonably be implemented.

Do not silently disable failing systems.

Do not remove original behavior merely because Enhanced Mode exists.

---

# 38. DECISION LOG

Maintain:

`docs/DECISIONS.md`

For major decisions record:

- problem
- options considered
- chosen approach
- reason
- tradeoffs
- date
- relevant source references

Examples:

- why a particular SNESRecomp revision is pinned
- how saves work
- how external players map to original structures
- how mod load order works
- how 60 Hz simulation is separated from render refresh
- how widescreen culling works
- how deterministic replay works

This prevents future agents from re-litigating solved architecture.

---

# 39. CURRENT FEATURE VISION

The eventual ISSD Native feature set should aim toward:

### Preservation

- original gameplay
- Classic Mode
- original graphics option
- original audio option
- original controls option
- original timing option

### Correctness / QoL

- slowdown removal
- bug fixes
- stable frame timing
- remappable input
- modern controllers
- modern saves
- auto-save
- configurable menus
- accessibility options

### Visual

- pixel-perfect rendering
- integer scaling
- CRT shaders
- fullscreen/borderless
- corrected aspect ratio
- true widescreen
- adaptive view
- smooth camera
- 120/144/165/240 Hz presentation
- native HD UI
- HD texture/sprite packs
- high-resolution stadium/pitch options

### Audio

- original SPC audio
- separate music/SFX volume
- MSU-1/replacement soundtracks
- high-quality music packs
- enhanced sound effects
- optional commentary packs
- language-specific audio

### Modding

- external teams
- external players
- effectively expanded roster capacity
- team editor
- player editor
- uniforms
- flags
- badges
- balls
- formations
- tactics
- competitions
- stadium assets
- UI mods
- language packs
- audio packs
- gameplay parameters
- AI tuning
- mod dependencies
- mod conflicts
- mod versioning
- HD packs
- optional safe scripting later

### Gameplay/competition

- custom cups
- custom leagues
- expanded squads
- match statistics
- player statistics
- heatmaps later
- modern tournament saving
- optional Enhanced AI

### Development/practice

- frame advance
- input display
- collision/hitbox tools
- state inspection
- replay system
- developer console/debug UI

### Online — later

- native local 1–4 players
- 1v1
- Internet play
- deterministic synchronization
- rollback if feasible
- 2v2
- spectators
- match replays
- rankings/tournaments only after core networking is stable

---

# 40. FIRST TASKS — START NOW

Do not begin by designing the online system.

Begin by establishing the recompilation baseline.

Perform these tasks in order:

### Task A — Repository research

Inspect the latest versions of:

- mstan/snesrecomp
- Yoshifanatic1/International-Superstar-Soccer-Deluxe-SNES-Disassembly
- Yoshifanatic1/SNES-ROM-Framework
- joshleaves/ghidra-snes
- DiztinGUIsh
- EstebanFuentealba/ISSD-SNES-ROM-Web-Editor

Document:

- relevant branch/revision
- license
- build requirements
- useful ISSD-specific information
- integration opportunities
- risks

### Task B — Create ISSD Native skeleton

Create:

- build system
- docs
- dependency layout
- scripts
- ROM verification
- local generated-data folders
- `.gitignore` rules preventing ROM/assets from accidental commit

### Task C — Verify the reference ROM

Support the documented USA headerless ROM revision.

Verify its hash.

Give a clear error when an unsupported ROM is supplied.

### Task D — Build existing disassembly

Verify that the Yoshifanatic ISSD disassembly can reconstruct the supported ROM from the user's clean ROM/assets.

This establishes that our reference/disassembly environment is correct.

### Task E — Run SNESRecomp analysis

Generate an initial ISSD recompilation project.

Record:

- discovered code
- unresolved functions
- indirect dispatch problems
- fallback paths
- warnings
- mapping information
- generated build status

### Task F — Import existing symbols

Determine how to translate labels/RAM knowledge from the ISSD disassembly into the SNESRecomp project and reverse-engineering tools.

Automate this where practical.

### Task G — First native build

Get generated code compiling on Windows x64.

### Task H — First boot

Integrate enough runtime/runner behavior to execute the game.

### Task I — Differential debugging

Use the reference implementation and traces to resolve divergences.

### Task J — Repeat until playable

Continue until a complete match can be played.

Do not prematurely work on HD or networking.

---

# 41. FIRST DELIVERABLE

Produce an initial technical report named:

`docs/BRINGUP_STATUS.md`

containing:

1. repositories and revisions evaluated
2. license notes
3. verified ROM revision
4. ROM mapping details
5. SNESRecomp generation status
6. percentage/estimate of statically resolved code if measurable
7. fallback/interpreter locations
8. known indirect-jump/dispatch problems
9. build status
10. runtime status
11. graphics status
12. audio status
13. input status
14. major blockers
15. next concrete engineering tasks

Then begin implementing the highest-priority blocker.

Do not stop after writing the report if actionable implementation work can be done.

---

# 42. LONG-TERM VISION

The desired final product is not an emulator wrapper.

It is a native modern incarnation of ISS Deluxe.

Conceptually:

`Original ISSD ROM`
→ `verified extraction/reference`
→ `65816 static recompilation`
→ `native ISSD simulation`
→ `modern platform layer`
→ `modding/data layer`
→ `enhanced renderer/audio`
→ `HD presentation`
→ `online systems`

The finished project should allow someone to choose between:

**Classic ISSD**

and

**ISSD Native Enhanced**

while sharing the same preserved gameplay foundation.

A user should eventually be able to install ISSD Native, point it at their legally obtained supported cartridge image, and then enjoy:

- original ISS Deluxe gameplay
- corrected slowdown
- modern low-latency controls
- modern Windows presentation
- enhanced audio
- persistent saves
- customizable settings
- extensive mod support
- enormous custom roster possibilities
- custom competitions
- modern graphics
- true widescreen
- high-refresh presentation
- HD asset packs
- replay/statistics systems
- and, only after those foundations are mature, modern online multiplayer.

**Preserve first. Modernize second. Make it moddable third. Build HD on top of the mod architecture. Add online last.**

Begin with the native bring-up now.