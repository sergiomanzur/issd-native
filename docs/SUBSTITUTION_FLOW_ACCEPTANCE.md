# Substitution flow acceptance (2026-10-09)

`tests/test_substitution_flows.py` adds real native guest input coverage to
`test_team_changes_native.py`. Every process owns its config, mods directory and
save directory. No production changes were needed for the passing routes.
The ROM and snapshots remain private under ignored build/test directories.

## Verified routes

- With optional AI off and both policies on, SELECT requests first-team management
  at an original stoppage. DOWN/A opens Squad directly. The first replacement
  installs roster 12 in field slot 1; RIGHT/DOWN/A/LEFT and bench navigation installs
  roster 14 in field slot 3. Opponent lineup remains unchanged.
- A save with one pending replacement restores and accepts the next replacement.
  Identical inputs produce identical complete guest payloads, WRAM and screenshots.
- Original exit/resume turns departed-player bench flags from $41/$43 to $81/$83,
  clears the pending pair at $1E6C to $FFFF and returns to live play. Advancing
  RIGHT input after restore preserves both replacements and replays identically.
- A fresh match started through original menus, rather than the exhibition shortcut,
  enters original halftime statistics and management. A bench replacement there
  persists into the original second half and into advancing save/load replay.

The halftime fixture edits only clock words $16D0/$16D2 in its own saved snapshot
and recomputes its envelope checksum. Period, callback, scene, teams, lineup,
replacement flags and score are all written by the guest. This proves the
halftime flow, not naturally elapsed match duration. The other cases use the
existing exhibition constructor without changing guest memory after construction.

## Replacement limit investigation

The original squad screen after the first swap displays `UP TO 2 Substitutes`;
this is the remaining allowance, not the initial cap. After the third it displays
`UP TO 0 Substitute`. Three original swaps commit: field slots1/3/5 hold roster
12/14/17 and departed bench flags become $81/$83/$85. A fourth attempted bench
replacement leaves both complete lineups unchanged. The automated off/on tests
check that admission rule, pending-save replay and live-save replay.

The same three-admitted/fourth-rejected behavior was reproduced at genuine original
Exhibition halftime; evidence is in ignored `original-limit2`, `original-limit3`,
`original-limit4` and `original-three-resume` under
`build/substitution-investigation`. No production rule defect was found.
Repeated SELECT requests still require condition-based stoppage admission:
a fixed 1800-frame wait did not always reach management and cannot be used to
infer rejection or a replacement cap.

## Remaining acceptance

Authentic second-team setup is now covered: Open Game ->1vs2, P1 chooses
Brazil, P2 chooses England. P2 SELECT requests management at a stoppage. Both
controllers choose Squad; the guest visits P1 squad, then P2 squad after P1 exits.
Each side replaces slot1 with roster12 using its own controller. Both sides
confirm Game Start, and both completed flags and lineups survive advancing
save/load replay. A pending second-side swap is replayed from a restored snapshot
with full guest-payload, WRAM and screenshot equality.

`test_substitution_remaining_flows.py` now adds SELECT-requested replacement
at an actual stoppage during live extra-time period2, resumption into that same
period and advancing full-payload/WRAM/image replay. Only clocks in the two prior
normal periods are shortened; the extra-time clock is untouched.

A second case commits three outfield replacements, resumes live play, requests
the next management opportunity and attempts a fourth using a genuinely unused
bench outfielder. It verifies both original candidate indices and roster IDs:
Squad DP=$1500, candidate $48=13/$4A=1, roster $C8=13/$CA=12, replacement
count $A4=3/$A6=0. The lineup stays unchanged and pending $1E6C remains $FFFF.
This excludes the weaker case of trying to reuse a departed bench entry ($81).
Original `CODE_86A4EC` validates the pair. Within `CODE_86A5C1`, the check at
$86A5DC loads $A4, subtracts $A6, compares against three at $86A5E1 and
branches to rejection at $86A5E4. The earlier $C4+$C6 comparison checks that
the pair crosses between bench and field.

A fresh cartridge-generated
Cup-final password fixture advances both normal halves into extra time, changes
the first-team bench at extra-time halftime, and verifies final period3 plus
advancing save/load replay. Only test-owned clocks are accelerated in this route;
its inherited password fixture is documented in `test_game_acceptance.py`.
Physical controllers, visible camera/name
continuity and every injury/red-card or competition-specific rule need human or
additional native fixtures. Existing first-team request cancellation remains in
`test_team_changes_native.py`.

## Reproduction

```powershell
python -m pytest tests/test_substitution_flows.py -q --basetemp=build/substitution-investigation/pytest-final
python -m pytest tests/test_substitution_remaining_flows.py -q --basetemp=build/substitution-investigation/pytest-remaining
```

Latest corrected first four cases: **4 passed in 207.17 seconds**. Added
both-human-sides case on the rebuilt executable: **1 passed in 60.64 seconds**. An earlier exploratory
run failed both AI variants because its exit script assumed the cursor after a
rejected fourth attempt. Rejection is now checked on a restored branch; the
normal commit/resume branch retains its original cursor and passes.
The two remaining-flow cases passed together in **142.35 seconds** on the rebuilt
executable.

This command skips if the user's retail ROM or native executable is absent.
