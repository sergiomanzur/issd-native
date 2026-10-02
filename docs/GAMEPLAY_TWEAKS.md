# Gameplay tweaks

Open the overlay with **Escape/F1**, a gamepad's **Guide/Home** or
**Back+Start**, then select **Gameplay Tweaks**. Toggle each option with
**A/Enter**, **Left/Right**, or a mouse/touch tap. **B/Escape** returns to the
main overlay. AI tweaks and the Original Bug Fixes master switch start off,
persist in settings, and can be enabled together alongside data packs.

| Option | Effect |
| --- | --- |
| Goalkeeper AI | Corrects positioning for the projected ball crossing at the keeper's standing plane. Angled incoming shots receive a lateral target correction of at most 12 pitch units per decision. Original dive/catch decisions, attributes and movement remain active. |
| Original Bug Fixes | Enables all verified original-game corrections in one switch. See [included fixes and limitations](ORIGINAL_BUG_FIXES.md). |
| Player AI | Gives computer-controlled off-ball positioning stronger formation lanes and role-dependent depth. Uses the active lineup player's native speed and condition to bound reactions, so substitutes and formation changes affect the next eligible decision. |

The goalkeeper and player AI options are initial tuning policies.
The cartridge already handles formations
and substitute attributes; these options add positioning adjustments. They do
not replace all decision-making, buff every player, alter ball physics or
teleport actors. Human-controlled players and manual goalkeeping are excluded.
Pitch dimensions, player data and formation tables come from the running game,
including applied data packs. Disabling a tweak restores original target
computation on subsequent decisions.

Each enabled combination has a separate campaign save context. Manual states
and Continue require matching gameplay tweaks and applied gameplay data.
Switch back to the saved combination to continue it. Retail cartridge passwords
require both AI tweaks and Original Bug Fixes off, along with the other
original-gameplay requirements.

Configuration keys are `gameplay_goalkeeper_ai=0/1` and
`gameplay_player_ai=0/1`; the master bug-fix key is `gameplay_bug_fixes=0/1`.
Old configuration files default to all three off.

The page also links to **Match Shortcuts / Presets** for exhibition rematches,
repeatable drills and a saved favorite. These actions retain complete setups
and do not roll back campaigns. See [the match shortcuts guide](MATCH_SHORTCUTS.md).
Rule presets apply at exhibition initialization, while the AI switches apply
immediately. No new goalkeeper/shooting balance patch is claimed without a
reproducible input-origin exploit; the guide includes the playtest protocol.

## Implementation and validation

Native generated code exposes optional basic-block policy callbacks even when
diagnostic tracing is disabled. Equivalent interpreted opcode hooks run at
the original `$84DEDE` keeper positioning branch and `$84C638/$84C63D`
outfield target stores. The latter check C5CE's guest return addresses to avoid
changing unrelated users of the shared C631 routine. There is no host routing
state to serialize, and no RNG or host-time input. Generated sources are
unchanged. Only adjusted player target registers and native attribute caches
are published; keeper changes affect only its lateral target.

Tests cover default-off no-op behavior, both teams, controller ownership,
role/formation changes, active bench-player substitution and condition,
invalid/inactive data, bounded corrections, independent stacking, configuration
persistence, menu input, save compatibility, live target changes and advancing
save replay with both options active. Gameplay balance still needs playtesting.

`--dump-state` logs policy change/call counts and execution-tier counts.
`ISSD_GAMEPLAY_TRACE=1` prints a bounded sample of AI decisions for debugging.

Original team management is requested with **SELECT** and admitted at the next
eligible stoppage. The cartridge commits formation choices when leaving that
menu and completes replacements on resumption. Automated input-driven checks
now cover a first-team formation change, one bench replacement, request
cancellation and advancing save/reload with both AI policies off and on.
See [original team-change evidence and limits](TEAM_CHANGES_ACCEPTANCE.md).
That route verifies native replacement attributes and roles; it does not yet
witness the substituted human-team actor's optional AI decision.

Decision tracing records current roster, formation, condition, role and cached
attributes. After the first twelve calls, at most two changed decisions per
actor are printed. Both player and keeper diagnostics read WRAM directly, so
logging does not modify CPU open-bus or cartridge bookkeeping state.
