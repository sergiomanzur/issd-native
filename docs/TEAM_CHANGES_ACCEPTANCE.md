# Original team changes: acceptance and limits

This pass checks original formation and substitution menus, their committed
match data, AI/readability consumers and save/reload. It does not add a new
substitution rule, replace the original management UI or claim improved balance.
Only actual input-driven changes establish original menu coverage. Directly
seeded lineup tests remain useful policy fixtures and are described separately.

## Data provenance

The original active lineups are WRAM $3F90/$3FA4, twenty entries per team.
Actor $68 is the field slot; actor $9A selects team record $0D00/$0E00. A lineup
entry's low five bits select a roster record in $3E00/$3EC8, ten bytes per player.
Original CODE_83C94F/CODE_83C9C5 refresh actor identity and attributes from that
mapping. CODE_83CA8F applies condition penalties. Actor speed, inverse speed and
skill caches are $62, $66 and $67.

Formation is team-record byte $A6. Original CODE_83DF33 uses formation position
tables $D000/$D140 and role tables $D280/$D320. Native player AI consumes the same
active lineup, formation and condition records at eligible live decisions;
human-controlled actors are excluded. Different positioning does not establish
that one formation is stronger or that the tweak improves football balance.

Name buffers $D478/$D518 contain all twenty roster names. Readability selects
through the current lineup rather than retaining the former starter's name.
Original selected-name indices $16DA/$16DC are refreshed by CODE_80CA96 and
CODE_80CAF6. Camera, draw-list and animation histories must follow the new scene
and actor data without leaking menu artwork into wider pitch margins.

## Acceptance requirements

- Generate the match seed by original inputs or the existing original exhibition
  constructor; keep private ROM/save snapshots outside version control.
- Enter the original management flow, change a formation, confirm and verify its
  real record and on-field roles/positions after original resumption.
- Swap a starter with a bench player through the original UI, verify the committed
  active lineup, actor attributes and corresponding roster name.
- Test cancel and original admission/limits where reproducible. Never force a menu
  callback or overwrite the lineup to claim input-driven coverage.
- Use compatible saves/configurations for AI Off and On. Confirm eligible original
  decision hooks execute after returning to play; preserve human input ownership.
- Compare original 256-pixel output with 16:10/16:9/21:9 scenes, identical scripts
  and frame counts. Require full endpoint WRAM equality and exact sampled native
  center pixels; retain images for margin/camera inspection.
- Save after the committed change, replay identical inputs before/after restore,
  and compare full WRAM and images. A frozen menu is not an advancing match replay.

Physical controller/device acceptance, every team/formation/weather combination,
complete natural tournaments and goalkeeper/shooting balance remain separate.

## Input-driven coverage (2026-10-01)

The fresh exhibition seed uses the existing original exhibition constructor;
subsequent changes use only controller input. No lineup, formation, scene,
callback, clock or score is overwritten. SELECT requests management at a
stoppage; a second SELECT cancels an unadmitted request during live play.
The scripted path visits formation and squad menus, commits formation 1 to 2,
replaces field slot 1's roster 1 with bench roster 12 (Bucario), resumes through
the original menu and takes the pending throw-in with B.

Both AI Off and both AI On configurations check:

- Original pending/completed substitution flags and the unchanged opponent lineup.
- Current formation roles and signed home-position anchors for all ten first-team
  outfield actors; the incoming player's native speed/inverse/skill caches are 7/2/8.
- Advancing save/reload with identical full WRAM, complete serialized guest payload
  and final screenshot under identical controller input.
- With policies On, eligible CPU opponent decisions report current roster/formation
  metadata and bounded target changes. Off leaves all policy counters zero.

This route does **not** witness the substituted human-team actor's optional
positioning-policy decision. Separate policy fixtures verify current-lineup,
formation and condition consumption, including a seeded bench replacement;
those fixtures do not certify every original menu-to-AI path.

The actual shipped readability name consumer returns Bucario for the visible
replacement actor $0600 without changing WRAM. The bootstrap actor has a culled
pose, for which the consumer correctly suppresses its label. Width comparisons use identical input,
AI configuration and save context, with exact full endpoint WRAM and native-center
pixels across original 256, 16:10 358, 16:9 398 and 21:9 504 pixels. Menu scenes
retain their original centered presentation; resumed pitch scenery widens.

## Corrections found

Player and goalkeeper trace logging now reads WRAM without CPU bus accessors.
Tests compare both gameplay RAM and CPU open-bus/bookkeeping state with tracing
On/Off. Player traces include the first twelve calls and at most two changed
samples per actor, so later eligible decisions can be inspected without an
unbounded log.

The original-return capture exposed an auxiliary fallback object ($09A0) with
an old pose at y=-8. The cartridge had culled it vertically, but wider drawing
re-added it and painted an 8-by-7 patch over the native top edge. The fallback
now preserves the original vertical admission for the verified auxiliary
classes ($83D01E: unsigned y+32 < 320; $83D057: unsigned y < 320), and only recovers omitted objects whose original horizontal admission fails
(small records: unsigned x+32 >= 320; $0800/$0900 records: unsigned x+64 >= 384).
Later buffered generations can retain a pose with y>=0 while remaining absent
from the original list; they must not be revived at a center origin either.
Existing original draw-list entries still receive horizontally clipped edge
pieces. Regression fixtures cover this actual stale-pose case,
small-record vertical culling and a still-visible horizontal margin object.
Presentation restores game-visible VRAM/OAM and does not alter WRAM or save layout.

## Reproduce

Use a locally owned USA retail ROM and a current `build/ISSDNative.exe`:

```powershell
python -m pytest tests/test_team_changes_native.py tests/test_gameplay_trace.py -q
$env:ISSD_VISUAL_ARTIFACTS="$PWD/build/team-changes-visible-visual"
python -m pytest tests/test_team_changes_visual.py -q
```

Private snapshots, ROM data and captured images are retained locally rather than
committed. Manifests identify executable/ROM/seed hashes and viewport runs.
Both-team UI, halftime/extra-time replacements, all formations, original
replacement counts/admission limits and physical-controller/device checks remain
pending. No goalkeeper/shooting balance improvement is inferred from this pass.


## Build record

Windows, Linux (Ubuntu 24.04) and Android arm64-v8a/x86_64 builds succeeded.
Build logs are `build/team-changes-complete-{windows,linux,android}-build.log`.
Windows executable SHA-256:
`3b6ce81beb717fc1593cd131d982b1622a05a2cd3ca85086611c2a73aab74dcb`.
Android APK SHA-256:
`5420e317bd3e55056ea50894761cb475a0956a5298d96690b704bf8f429565a3`.
Physical Android/controller acceptance is still pending.

## Final automated results

- **19 passed in 117.33 seconds** for original-menu/native replay, gameplay
  diagnostics/hooks/integration, formation/readability, transactional snapshots
  and widescreen checks (`build/team-changes-final-acceptance-focused.log`).
- After the final horizontal-admission correction, **3 passed in 4.94 seconds**
  for widened auxiliary preparation, descriptor continuation and animation
  regression (`build/team-changes-complete-regression.log`).
- **1 passed in 112.92 seconds** for all nine original input-driven stages at
  four widths: 36 viewport runs, 468 captures, exact endpoint WRAM and sampled
  native-center pixels, plus the shipped replacement-name lookup. Log:
  `build/team-changes-accepted-visual.log`; images/manifests:
  `build/team-changes-accepted-visual`.

Widened formation/squad menus, match resumption and live-play margins were
inspected. The full project suite was not repeated; the previously recorded
appearance-nibble and shipped-pack kit-sharing failures remain unresolved.
