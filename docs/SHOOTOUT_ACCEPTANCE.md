# Completed shootout acceptance

`tests/test_shootout_completion.py` exercises the original retail-ROM Cup final
from a cartridge-generated semifinal password. It runs first-half, second-half,
and both extra-time exits through the original statistics and continuation
screens, then completes the original shootout using ordinary B presses/releases.

Only the four pre-shootout clocks and tied regulation scores are edited in the
test-owned save envelope. Period, stage, callback, shootout score, kick counters,
champion and campaign data are never seeded. The existing continuation script
contains ordinary B presses that already take the first shootout turns; its
90-frame ready capture therefore shows round 2 rather than round 0.

The test verifies a real mid-shootout snapshot and deterministic new-process
reload, requiring every byte of final WRAM to match the uninterrupted suffix.
The cartridge must leave Cup-final stage 8 for terminal stage 9, install terminal
callback `c0c88b`, and write the higher-scoring shootout team as champion. The
settled shootout ceremony must create exactly one new campaign generation with
a valid checksum and `Cup complete:` label. The original terminal A action may
return to the title screen. Continue must restore the completed terminal Cup state without resaving it.

Original disassembly grounds the assertions: `CODE_8C9D18` compares score bytes
`$7ED442/$7ED443`, determines the winning side in `$1700`, and checks the remaining
kicks before deciding a winner. `$1704` tracks shootout rounds. `$1706/$1708` are
shot/keeper direction fields, **not** penalty scores. These fields must not be
used to infer the winner. The champion is the original table byte `$7EDDCE`.

Reproduce with the user's private retail ROM and a current native build:

```powershell
python -m pytest tests/test_shootout_completion.py -q --basetemp=build/shootout-investigation/verified
```

Private run artifacts include input scripts, per-period and mid-shootout WRAM,
replayed terminal WRAM and `shootout-evidence.json`; no ROM or full snapshot is
committed. The evidence manifest records executable/ROM hashes, actual penalty
scores, champion, callback, label and replay equality.

This acceptance covers ordinary and sudden-death completed shootouts and their
campaign/save handoff. Both possible retail winners, two-human shootouts, every
team/difficulty, naturally elapsed pre-shootout periods and physical input remain
unverified. The
shootout itself executes at original frame timing, without clocks/results being
edited, while its match setup is explicitly accelerated.

## Verified result (2026-10-09)

The fresh production run passed **2 tests in 130.28 seconds** (completed-shootout
integration and campaign observer regression). Brazil won **5â€“3** over Uruguay;
the original champion was team 60, callback `c0c88b`, and checkpoint label
`Cup complete: Brazil`. The import generation 1 advanced exactly once to
completion generation 2. Original terminal exit and Continue kept that saved
generation byte-identical. The mid-shootout replay matched all 131,072 WRAM bytes.

Artifacts: `build/shootout-investigation/green/test_drawn_cup_final_completes0`.
Executable SHA-256:
`8d9a0fbe558d49a47e4c3c0e183c2209aac01efbadfb54dd94452454014a51c4`.
This is focused verification; the whole-project suite is reported separately.

The run exposed a native checkpoint omission: after a completed shootout, the
original settled terminal ceremony can return directly to the title screen,
bypassing the ordinary Cup completion-table callback. Before the fix, the
original winner, champion, terminal callback and deterministic replay all
succeeded, but the checkpoint stayed at the imported semifinal generation 1.
The failed run is retained in `build/shootout-investigation/verified`.

`ISSDNative/issd_campaign.c` now recognizes this exact terminal shootout path:
Cup completion flags, stage 9, zero round counter, period 3, settled ceremony
callback, unequal original penalty scores, original winning side and matching
valid champion. Existing two-frame stability and duplicate suppression remain
in force. The unit regression failed before this change and passed afterward;
it covers both winning sides, unfinished stage 8, invalid champion, unsettled
timers, tied scores, missing winner, wrong flags/callback/mode context and a
subsequent ordinary table callback without duplicate saving.


## Original sudden death (2026-10-09)

`tests/test_shootout_variants.py` passed **1 test in 102.67 seconds** against the
same executable hash recorded above. It regenerates the original Cup final and
four accelerated period exits, then holds RIGHT while issuing ordinary released
B presses. The original first five rounds tie **3–3**: `$1704=5`, `$170E=1`, winner
`$1700=$FFFF`, Cup stage 8. The imported checkpoint remains byte-identical, so
sudden-death entry cannot be mistaken for completion.

The cartridge then opens its original kicker-selection screen. Released DOWN
inputs select an unused kicker; A selects him and another released A confirms.
The real sixth-round shot/keeper inputs produce **Brazil 4–3 Uruguay**, original
champion 60 and a single `Cup complete: Brazil` checkpoint. All controls are
released for the settled ceremony check. Continue restores the completed Cup
state without rewriting its checkpoint.

Artifacts are retained in
`build/shootout-investigation/sudden-fresh/test_original_sudden_death_kic0`,
including `variant-evidence.json`, original tied/final WRAM and explicit
`mid-shootout.sav` and `sudden-death.sav` envelopes (302,378 bytes each). The
primary completion test retains its intermediate WRAM and validates its snapshot
through replay; its quicksave slot is subsequently overwritten. The variant
preserves separate snapshot files to make both states available afterward.

No shootout score, turn, counter, aim or winner field is edited. The production
observer needs no additional change for this path. Actual opposing-winner
completion remains unverified; both winning sides are covered in the observer's
unit regression only.
