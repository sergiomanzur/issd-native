# Added stadium selection and match acceptance

This investigation repairs the existing expanded stadium path requested on
2026-10-09. Acceptance requires original menu selection, preserved logical
identity, healthy match initialization, actual gameplay/replay, and native
save/load. The original eight stadiums remain regression cases. New geometry,
new stadium artwork and expanded competitions are separate work.

## Original-code findings

Ghidra 12.1.3 exported focused disassembly from the user's private USA ROM.
The ROM SHA-256 is
`d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9`.
Evidence stays in ignored
`build/ghidra-investigation/stadium-constructor-evidence.txt`.
Embedded menu dispatch words are data; the broad linear export crosses those
words, so original assembly labels govern those portions of the trace.

* `$A4:9F3F` loads the left-wrap limit 7. Expansion previously changed only
  the right-wrap comparison at `$A4:9F6C`.
* `$85:A4FE` loads logical selection `$1FA2`; `$85:A501` masks it with 7;
  `$85:A504` stores the match layout in `$0086`; `$85:A507` then overwrites
  the logical selection. Expanded packs now suppress that last store only.
* The generated `CODE_85A3FB_M0X0` contains the baked store. Both `$05:A3FB`
  and `$85:A3FB` are routed through the existing interpreter deny list so
  the patched cartridge instructions are authoritative.
* `$82:FAFD` is an eight-byte name-sprite index table, read without doubling
  the index at `$A4:A03E`. The old expansion copied it as words and read into
  neighboring weather data. It now copies bytes with the correct stride.

The original playable graphics, palette, geometry and camera tables have
eight entries. `$0086=8` is explicitly an introduction layout, and some map
tables contain further cutscene entries. Expanded logical stadiums therefore
use `logical_id & 7` for the original match layout while preserving `$1FA2`.
They retain custom names and menu metadata; this does not create new playable
geometry. Existing HD replacements use tile/palette identity and inherit the
aliased layout assets rather than becoming independent per-stadium art.

All newly patched instruction bytes are checked before relocation or code
mutation. Expansion retains the existing maximum of 32 logical slots.

## Validation status

Synthetic ROM tests first failed at the missing left-wrap patch, then passed
after the fix. They cover unchanged eight-slot behavior, 12-slot expansion,
the 32-slot cap, byte/word/name copy strides and rejection of altered original
instructions before relocation. The serializer test locates the actual
generated owning function and checks both deny-list mirrors.

The previous optional four constructor failures placed logical slots 8–11
directly into `$0086`, bypassing original normalization and selecting special
or invalid layouts. Those failures are retained as diagnostic history, not
evidence that the corrected constructor path fails. The updated background
fixtures use the real logical/layout distinction and run for all four added
slots by default. Fixture selection is explicitly separate from ordinary menu
selection acceptance.

Native results are retained under `build/extra-stadium-validation/`.
The full project regression run completed with **325 passed, 5 skipped and
8 subtests passed** in 3111.74 seconds, with no failures. Command:
`python -m pytest tests -q -rs --basetemp=build/extra-stadium-validation/full-suite`.
Evidence: `full-suite.log`. That run collected 330 tests before the six new
native stadium tests were added; those six passed separately in the final
199.49-second native run described below. Current collection contains 336 tests.
Focused reruns are overlapping evidence and are not added to these totals.

The five skips were physical audio timing (idle machine/default device),
opt-in 35-match World acceptance, opt-in extended period visuals, the Mod
Studio tile GUI (the host Python installation lacks `ttk/altTheme.tcl`), and
a missing penalty-area replay snapshot. These were not passing checks and
were not failures of the stadium path.

The ordinary Exhibition-input tests passed for logical slots 8–11 (Akron,
Azteca, BBVA and Guadalajara), reaching live gameplay with preserved logical
IDs and the expected layout aliases. Native quicksave reload advances gameplay
and preserves both IDs and save bytes. The original selector also passes
left-wrap from 0 to 11 and right-wrap from 11 to 0. The five focused cases
passed in 68.62 seconds after structural selector and on-disk save checks.

The same five native cases were independently run against the retained
pre-fix Windows executable and original deny file: **all five failed** in
62.05 seconds. Added selections were reduced to their original slot IDs and
left wrap returned 7 instead of 11. Evidence: `red-native-input.log` and its
owned temporary directory. These ordinary-input failures establish the actual
defects; the old malformed constructor fixtures do not.

## Naturally timed match and replay

The added Azteca entry (logical 9, stock layout 1) completed an original
Exhibition match with ordinary input and **no guest RAM, clock or score edits**.
This proof deliberately uses authentic native checkpoint saves/reloads across
multiple processes; it is not an uninterrupted single-process playthrough.
The initial picker was reached from fresh startup with ordinary inputs. A
caught player-controlled goalkeeper ball required normal pass input, and normal
Start input exited goal replays. Those are input requirements, not patched game
rules. Blind A/B presses can pause or rewind a replay; the final checkpoint
controller chooses input according to the actual mode.

The retained lineage is `native-tests/native-stadium-picker0` (the fresh picker),
`native-natural9`, `native-natural9-continue`, then
`native-adaptive/stage-00` through `stage-27`. Intermediate exploratory blind
input runs are not part of this lineage. The 28-stage `history.json` retains
logical 9/layout 1 at every boundary. Stage 12 has original halftime statistics
(mode `$12`, submode 7, period 0, clock 0, score 0–5). Ordinary A then advances
to second-half live play at stage 13. Stage 27 has original fulltime statistics
(mode `$12`, submode 7, period 1, clock 0, score 0–11), with a native quicksave.
Brazil losing is a valid completion result; this is not a competitive win proof.

A new-process fulltime reload independently passed for 180 frames, preserving
both stadium IDs, period, zero clock, scores and the fulltime statistics mode.
Neither source nor cloned save bytes changed. Evidence:
`fulltime-restore/verification.json`; snapshot SHA-256
`ac473831b73056e5a277ef701206a97c444d1751c9af6f467ffd1c200216536c`.

An actual naturally scored goal supplied the replay snapshot. Ordinary A clears
rewind, and X resumes playback. The replay cursor advances from 10 to 258;
75 consecutive captures contain 60 distinct images. At 4:3, 16:10, 16:9 and
21:9, endpoint whole WRAM and VRAM agree, and **all 75 full 256×224 native-center
images match exactly**. Added margins do not alter the original replay center.
Captures begin after the temporary host mod notification expires; an earlier
90-frame comparison included different host-toast wrapping and is not claimed
as a game-rendering failure. The renderer itself decrements that notification's
lifetime in headless mode.

Reproducible sustained native command:

```powershell
$env:ISSD_NATURAL_STADIUMS='1'
python -m pytest tests/test_extra_stadium_native.py -q
```

The final independent run of all six native cases passed in **199.49 seconds**,
including the updated 75-frame center comparisons and explicit replay movement
checks. Evidence: `native-final-root.log` and `native-final-root/`. Read-only
review confirmed the code guards, actual serializer dispatch route, fixture
scope, checkpoint lineage and halftime/fulltime boundaries.

Only stadium 9 has complete natural-match and goal-replay coverage here. All
four added entries have ordinary menu/live/save-load and fixture background
coverage. Custom playable geometry, independent stadium art, expanded Cup/World
schedules and physical device operation remain outside this acceptance scope.

## Existing expanded-pack saves

This repair changes the effective patched ROM for packs with more than eight
stadiums. Save/password compatibility contexts include that effective ROM
digest. An owned copy of a pre-fix expanded-pack quicksave was tested against
the repaired build: it was rejected with `Save is incompatible with applied
gameplay data`, and both source and cloned save bytes remained unchanged.
Evidence: `old-expanded-save-context/verification.json` and `run.log`.
Start a fresh match when using the repaired expanded pack; no migration or
compatibility bypass was implemented. The acceptance saves above were created
with the repaired build.

A pre-fix unmodded quicksave from the earlier clean-install validation was
also cloned and restored successfully with the repaired executable. Its natural
0–1 goal replay advanced, accepted pause input, and neither source nor clone
was rewritten. Evidence: `old-unexpanded-save-context/verification.json`.
This checks one actual older baseline save; it is not a claim about every
historical save format or mod combination.

The current Windows executable SHA-256 is
`013516ed0db678a0d80e41790147b3552df2abf824b7397c5fe7285c5d997289`.
`source-artifact-manifest.json` records 86 source, mod-data and platform-artifact
hashes for this validation snapshot; no version bump, commit or publication
was created.
The focused run passed **16 tests** in 168.99 seconds: mod ROM, serializer
dispatch, mod stacking/JSON and all twelve logical stadium background cases.
Its log is `build/extra-stadium-validation/green-focused.log`.

Linux and Android incremental rebuilds passed; Linux also passed an isolated
240-frame startup smoke. Android includes the current canonical deny asset
and both native ABIs, with CRC and signature checks. These establish build
and packaging results, not expanded-stadium device gameplay. See
[platform verification](EXTRA_STADIUM_PLATFORM_VALIDATION.md).
