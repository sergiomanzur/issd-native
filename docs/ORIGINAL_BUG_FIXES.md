# Original game bug fixes

Open **Gameplay Tweaks → Original Bug Fixes** in the overlay. One master switch
controls the bundle: **On enables all verified fixes below; Off preserves the
original behavior**. It defaults to Off and persists as `gameplay_bug_fixes=0/1`.
Keyboard, gamepad, mouse and touch use the same setting. It stacks with both AI
tweaks and data packs. This is unreleased source after v0.2.0-beta.1.

| Original issue | Behavior with fixes On |
| --- | --- |
| Multiplayer goalkeeper speed stacking | Move a shared human-controlled keeper once per controller loop. Preserve inputs, original velocity and the first movement after switching to the keeper. |
| Infinite Edit Player Skills points | Refresh the actual goalkeeper cache after the starting-roster footer transition. Decreasing phantom skills no longer generates points; normal purchases and refunds remain original. |
| Repeated behind-goal awards | Require a crossing from the playable side of the original goal boundary. An already-behind-goal ball cannot repeatedly award goals. Normal crossings retain the original four-unit threshold. |
| Two-digit score wrap | Keep match scores at 99 when another goal would exceed the original HUD range. Accounting below 99 is unchanged. |
| Free kicks inside/behind goals or outside the pitch | Repair invalid foul restart coordinates, their aliases and quadrant. Place only an invalid axis four pitch units inside its boundary; retain valid free-kick locations. |
| Completed-name graphics lookup overflow | After all three letters, skip the nonexistent fourth caret upload. Preserve the name and original uploads for zero, one or two letters. This prevents a stray VRAM upload that can become visible with altered graphics layouts. |

The goal fix prevents false awards; it does not guarantee recovery from every
state where a player escapes the pitch. The foul fix repairs the invalid restart
location. There is no general shooting-power, ball-physics or difficulty rebalance.

## Saves and switching

The switch is part of gameplay compatibility, alongside applied ROM data and
AI/debug settings. Continue, manual saves and favorites require the saved setting.
Incompatible files are preserved; switch back to load them. Changing the toggle
invalidates resident rematch/drill checkpoints. Capture a new compatible
exhibition or drill before using shortcuts. Retail password import/export requires
this switch and both AI/debug tweaks Off.

Fixes act at their original routines after toggling. They do not undo past goals
or reconstruct attributes already altered using an exploit. The keeper movement
ledger resets at controller-loop entry, context changes, loading and boot/reset.
Completed-frame save/reload reproduces guest RAM.

## Unresolved reports

Stuck balls at posts, free-kick camera timing after substitutions and goalkeeper
control during penalty replays are **not verified fixes**. The replay scene reads
recorded poses rather than live controller movement; masking its buttons would
break normal playback controls without demonstrating a fix. These reports need
failing input replays on the supported USA revision before adding corrections.

This list does not claim every original defect has been discovered or fixed.
Historical reproduction leads are in
[Ferrel's SNES guide](https://gamefaqs.gamespot.com/snes/588391-international-superstar-soccer-deluxe/faqs/16057)
and the [TASVideos discussion](https://tasvideos.org/Forum/Topics/8929?CurrentPage=3&Highlight=250255).
The name issue is documented beside `CODE_8AB3FE` in the cartridge disassembly.

## Verification

`test_bugfix_keeper.py`, `test_bugfix_skills.py` and `test_bugfix_name.py` execute
original cartridge opcodes through the production interpreter, plus policy tests.
`test_bugfix_goal.py` executes unchanged generated goal/score routines and restart
blocks. Cases cover both teams, all eight pitch dimensions, valid goals/refunds,
invalid data, normal name uploads and disabled no-op behavior.

`test_bugfix_native.py` starts a healthy exhibition with isolated config and saves,
then constructs checksummed fixtures assigning one to four humans to one moving
keeper. The real input reader, cartridge movement and production hooks verify
original stacking, corrected displacement, held inputs, unchanged single-owner
movement, interpreter parity, full WRAM save/replay, incompatible-load rejection
and AI coexistence. These are controlled fixtures, not exhaustive natural
controller playtests or Android device certification.

`--dump-state` prints `[BugFixes]` correction counts for keeper, skills, goal,
score, restart and name policies. Generated cartridge sources are unchanged.

The final Windows artifact passed **16 focused tests in 63.86 seconds** on
October 1. Windows, Linux and Android arm64-v8a/x86_64 builds succeeded; both
On/Off menu states were visually checked. See [acceptance records](ACCEPTANCE_TESTS.md)
for the broader project-suite result and remaining device checks.

The full project suite returned **217 passed, 2 failed, 3 skipped, 7 subtests
passed** in 970.62 seconds. Both failures are the existing mod appearance-nibble
expectation and shared-kit warnings; all new bug-fix checks passed.
