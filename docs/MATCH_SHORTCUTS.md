# Exhibition rematches, drills and presets

## Pause actions (current source after v0.4.0-beta.1)

Open the native overlay with Escape/F1 or the controller overlay shortcut.
**Restart Match** and **Back to Main Menu** are directly below Resume.
Restart restores the first healthy kickoff captured for the resident exhibition,
International Cup or World Series match, including teams, kits, weather, rules,
competition state, clock and score. It does not advance campaign progress.
Back returns to the actual cartridge main menu and retains the campaign save
already on disk. Any play since that save is abandoned.

These session snapshots are not synthesized from a mid-match manual save or
Continue. Restart is unavailable without a captured kickoff. When no compatible
main-menu snapshot exists (including a fresh Continue), Back runs the original
startup flow with the applied cartridge and automatically stops at the actual
main menu. A progress message appears during that transition; campaign files
are retained. Mod or gameplay-context changes invalidate resident snapshots.
Scenario, training and standalone penalty-mode restart remain unsupported.
Unavailable actions report their reason.

## Exhibition shortcuts

Open **Gameplay Tweaks → Match Shortcuts / Presets** in the native overlay.
These features are included in v0.3.0-beta.1.

Play an **Open Game / exhibition** through the original setup once. Teams,
kits, stadium, weather, time of day, referee, lineups, conditions and human/CPU
assignments are retained together in a full runtime checkpoint.

| Action | Behavior |
| --- | --- |
| Instant rematch | Return to the first live kickoff of the most recently captured exhibition. Score, clock and match state return to that kickoff. |
| Mark drill checkpoint | Mark the current healthy, live exhibition position. It is separate from kickoff and normal saves. |
| Restart drill | Return to the marked position. An intervening rematch does not erase the drill. |
| Save favorite setup | Save the retained pre-match setup, rather than the current score/clock. One favorite is stored independently of campaign, quicksave and numbered slots. |
| Play favorite | Load the saved setup, including its recorded rules, and let original pre-match initialization continue. It works after application restart, from title or exhibition; it does not interrupt campaigns. |
| Start with selected rules | Restart the retained setup with the selected rules, letting original initialization rebuild clocks and difficulty. It replays the match introduction and replaces kickoff/drill checkpoints. |

Checkpoint restores clear held keyboard, pad and touch input. Release controls
before playing again. Failed captures/restores display an error; a failed rule
recapture restores the previous guest state and preserves its checkpoints.
Shortcuts are unavailable before a healthy completed frame.

Rematch and drill checkpoints last only for the resident session. A new
exhibition, external manual load, application restart, or applied gameplay/AI
context change invalidates them. Campaigns, scenarios, training and standalone
penalty competitions are outside this feature. Return to a normal exhibition
to capture a new setup. A raw loaded mid-match state does not invent a kickoff.

Full checkpoints also retain RNG and player conditions: rematch/drill replay is
repeatable, rather than a fresh random draw of the same teams. A favorite uses
the same setup checkpoint but runs the original introduction and constructors.

## Rules

Choose a preset with Left/Right or A/Enter. Changes persist and apply at the next
verified exhibition initialization. They do not rewrite a match in progress.
Use **Start with selected rules** to apply them to the retained exhibition now.

| Preset | Duration | Difficulty | Offside / fouls / cards | Extra time |
| --- | --- | --- | --- | --- |
| Original (default) | Original setup | Original setup | Original setup | Original setup |
| Classic | 5 minutes | Level 3 of 5 | On | Off |
| Casual | 3 minutes | Level 1 of 5 | Off | Off |
| Custom | 3, 5 or 7 minutes | Levels 1–5 | Independently on/off | On/off |

Editing a rule switches to Custom, seeded from the displayed named preset (or
the last saved custom rules when Original is selected). Original leaves the
cartridge's selected options intact. The hidden duration index is not exposed.
Extra time uses the original game's behavior; no new golden-goal or shootout
policy, match speed, ball power, stamina or physics modifier is introduced.

Playing a favorite always uses **its saved rules**, even when a different preset
is currently selected. Select a new preset and explicitly start with those
rules if you want to change it. Save favorite again to replace the saved setup.

Configuration keys: `match_preset` (0 Original, 1 Classic, 2 Casual, 3 Custom),
`match_duration` (0/1/2 for 3/5/7), `match_difficulty` (0–4),
`match_offside`, `match_fouls`, `match_cards`, `match_extra_time`.
The four rule switches use cartridge encoding: **0 on, 1 off**.

The favorite is `matches/favorite.sav` under the selected save root. Integrity,
runtime format, ROM, applied mod data and AI/debug compatibility are checked
before loading. Incompatible files remain intact. Restore the corresponding
configuration or deliberately save a replacement. Presentation and input
profiles do not change save compatibility. The favorite is atomically replaced;
it does not have the campaign's two backup generations.

## Balance playtesting

No playable scoring exploit has been verified, so this update does not change
shooting or goalkeeper balance. The existing optional goalkeeper AI remains
available. A synthetic ball-position/velocity fixture changed keeper targets
but lacked authentic shot input and animation provenance; it cannot establish
a scoring exploit or demonstrate a save-rate improvement.

To report a repeatable problem, mark a live exhibition drill before taking the
shot. Record the original team/player/keeper, lineup and condition, difficulty,
weather, direction, shot charge/release and aftertouch. Repeat the same inputs
at least three times without WRAM edits. Record shot state, advancing game
frames, keeper response and score change. Vary location, goal direction and
attributes to distinguish a missed response from an expected strong shot.
Compare AI off/on with separately compatible checkpoints. Attach input scripts
and recordings when available; keep private ROM and owned save captures out of Git.

## Automated evidence

`tests/test_match_shortcuts.py` covers phase admission, rules persistence/ranges,
independent checkpoints, unsafe-scene/health rejection and recapture rollback.
`tests/test_match_favorite_storage.py` covers integrity, compatibility, atomic
publication failures and isolation from campaign/numbered saves.
`tests/test_match_shortcuts_native.py` exercises original exhibition setup,
clock/actor initialization under all four presets, exact WRAM replay after
rematch/drill, favorite process restart and incompatible-context rejection.

The headless diagnostic option `--match-action FRAME:ACTION` can be repeated up
to 16 times. Actions are `rematch`, `mark-drill`, `restart-drill`, `save-favorite`,
`play-favorite`, and `start-rules`. Use frame 1 or later for a favorite from boot,
after the first completed frame. Capture logs identify setup and kickoff frames.
These diagnostics do not replace normal input navigation or device playtesting.
