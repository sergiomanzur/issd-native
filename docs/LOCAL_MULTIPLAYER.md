# Local multiplayer

Connect up to four SDL2-compatible gamepads before launching or while the game
is running. Controllers take P1–P4 in connection order. The notification on
connection identifies the player, and SDL's player index is set for controllers
that support player indicators. Keyboard and Android touch remain additional
controls for P1; four independent gamepad players require four gamepads.

Use Open Game and the cartridge's player-count options to choose the match.
The original modes include P1+P3 versus P2+P4 and all four players versus CPU.
Each P1–P4 slot has its own saved Classic/FIFA/PES/custom profile. Open
**Controls / Profiles** to remap its native actions and adjust independent stick
and trigger thresholds. Profiles belong to slots, not particular controller IDs.
See [the controls guide](CONTROLS.md).

Removing a controller clears its input and leaves every other player's slot
unchanged. A live match pauses in the overlay. Reconnecting fills the first
vacant slot; resume when ready. With multiple empty slots, connections fill
them in ascending player order. Extra controllers beyond four are ignored.

Any assigned gamepad can open the overlay with Guide/Home or Back+Start and
navigate it with the D-pad, A, and B. Gamepad controls held during the overlay must be
released before they affect gameplay again. Unfocused windows send no gameplay
input, and held gamepad controls are also suppressed when focus returns.

The host samples each gamepad separately at the simulation boundary. D-pad,
stick, buttons, and triggers are combined as independent sources, so releasing
one source does not cancel another held source. Opposite directions cancel.

## Runtime and saves

The game sees a controller on SNES port one and a MULTI5 multitap on port two.
P2 and P3 use the first pair of multitap data lines; P4 uses the first line of
the second pair. The fifth multitap controller is absent. Automatic polling
populates JOY1, JOY2, and JOY4 and advances the first pair's serial counter;
the cartridge selects the second pair through WRIO for its P4 read.

Save format v8 appends the multitap latches, pair selection/counter, connection
mask, and automatic-read words. Existing v4–v7 saves remain readable. The legacy
two-controller `RtlRunFrame` API continues to work for other runner clients.

Input scripts can address individual players without overwriting the others:

```text
0 P1 B
0 P2 A
0 P3 X
0 P4 RIGHT
20 P2 NONE
```

Unlabelled entries still address P1. Only labelled/scripted slots are replaced;
P1 auto-start can be combined with a script addressing P2–P4.

## Verification

Run `python -m pytest tests/test_local_multiplayer.py -q` after building.
The checks use real SDL virtual gamepads, the production multitap protocol,
legacy two-pad fixtures, the production SNES snapshot serializer, and ROM-backed
guest input and save/load runs. ROM-backed checks require the user's ROM and
`build/ISSDNative.exe`. No physical four-controller match or Android device
playtest has been performed by this change.

The initial multiplayer verification reproduced five failures from the original
checkout. The subsequent campaign-save work repaired the two menu harness
linking failures and the advancing snapshot replay mismatch. Current snapshots
preserve native CPU/timing data as well as multitap state; title, classic live
play, and widescreen replay checks compare every advancing frame and full WRAM.

The remaining baseline failures are `test_mod_rom_patching` (an appearance-nibble
expectation) and `test_shipped_packs_validate` (kit-sharing warnings treated as
validation failures). See [the README](../README.md#testing-and-diagnostics) for
the latest full-suite result.

Collecting tests from the entire repository root also collects the vendored
SNESRecomp tooling tests, which currently report 53 collection errors. Use
`python -m pytest tests -q` for this project's suite.
