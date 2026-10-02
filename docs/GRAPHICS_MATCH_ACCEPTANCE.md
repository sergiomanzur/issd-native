# Reproducible retail match graphics acceptance

`tests/test_match_visual_acceptance.py` runs the owned retail ROM in the native
executable. It imports the cartridge-generated semifinal password through the
original Password screen, starts the original Cup final, and copies the resulting
save into isolated runs at 256, 398 and 504 pixels. Penalty stages additionally
compare the 358-pixel 16:10 view. The save envelope, input script
and number of simulated frames are identical across each scene's three runs.
No ROM, password-screen snapshot or full match snapshot is checked in.

Run the bounded first-half and original pause/replay checks:

```powershell
$env:ISSD_VISUAL_ARTIFACTS = "$PWD/build/visual-match-acceptance"
python -m pytest tests/test_match_visual_acceptance.py -q
```

Include the longer period-transition checks:

```powershell
$env:ISSD_VISUAL_EXTENDED = "1"
python -m pytest tests/test_match_visual_acceptance.py -q
```

The extended run accelerates each period by editing only the test-owned clock
and tied score through the existing `shorten_clock` helper. Original ROM code
decides the whistle, stats screen, next period and subsequent match flow.
Callback, scene, period and champion fields are never forced. This is an
accelerated transition fixture, not a naturally elapsed or bot-played full match.
The longer test is opt-in to keep ordinary test runs bounded.

Every scene writes its input script, config, final WRAM, final BMP/PNG, twelve
consecutive BMP/PNG frames at the end of each run and execution log. The penalty
shot-result stage instead captures every frame after the first, plus the final
frame, to compare the kick/camera sequence. Its manifest
records executable/ROM/seed hashes, frame count, actual mode and period, and WRAM
hashes. Original-width state is saved for the following transition. Without
`ISSD_VISUAL_ARTIFACTS`, artifacts stay in pytest's temporary directory.

Each comparison requires a nonblank scene, the expected output dimensions,
byte-identical entire 128 KiB WRAM, and identical native 256-pixel center pixels
for every captured frame. Differences are saved as PNGs before failure.
Readability overlays and radar relocation/scaling are disabled so presentation
comparisons preserve the original center. These checks certify deterministic
simulation and original-center preservation; human review of the PNGs is still
needed to judge whether every newly exposed margin element looks correct.

| Scene | Origin and assertion |
| --- | --- |
| First-half live play | Original Cup final; mode 8, period 0; actual directional input |
| Pause | Actual START press in live play; original RESUME/REPLAY menu |
| Replay | Actual DOWN then A in that menu; original replay mode 0x13 |
| Halftime stats | Accelerated first-half clock; ROM stats mode 0x12 |
| Second half | Actual original stats/menu input; mode 8, period 1 |
| Fulltime stats | Accelerated tied second-half clock; ROM stats mode 0x12 |
| First extra-time half | Original draw continuation; mode 8, period 2 |
| Extra-time halftime stats | Accelerated clock; ROM stats mode 0x12 |
| Second extra-time half | Original continuation; mode 8, period 3 |
| Extra-time fulltime and shootout entry | Original tied fourth-period exit; mode 0x0c, period 3; first-kick goal-facing scene |

After the tied fourth-period exit, the extended run also compares:

| Penalty stage | Original input and assertion |
| --- | --- |
| Ready | Original first-kick camera; possession $1440=0 |
| Shot start | Actual B press at frame 30, release at 50 |
| Shot result | Every frame of the following 140-frame shot/camera sequence |
| Opponent turn | Original next-kick flow; possession $1440=2 |
| Third turn | Original CPU kick and return; possession $1440=0, kick counter $1704=3 |

These five stages run at 256/358/398/504 pixels. Added margins must contain
varied pitch scenery with mostly green grass, rather than blank borders. BG2's
authored crowd/advertising/grass tilemap wraps into the widened area. BG1's single
goal and BG3's HUD remain centered, with native OAM unchanged. Shot mechanics and
camera bounds remain the cartridge's. Full WRAM and every sampled native-center
pixel must still match the original-width run. This checks two actual kicks
and subsequent turns, not a completed shootout. The awarded in-match penalty
layout is traced and unit-tested; this run does not originate an actual foul.

This suite does not certify scoring a real goal, goal celebration/replay,
shootout completion/sudden death, original substitutions, alternate stadiums,
weather, every team/kit, every camera angle, or continuous untouched full-match
play. Selecting replay from pause proves a replay scene, not a goal replay.
The fourth-period continuation enters the original shootout first-kick scene.
The penalty follow-up exercises real kicks and subsequent turns with widened
scenery; it does not complete the shootout. The other cases need additional
actual-input fixtures or manual traces before their coverage can be claimed.
The existing live widescreen regression covers selected offscreen players and
HUD leakage; it complements these scene checks.

Initial verification on the pre-decoder-update native executable: live and all
four period transitions passed together (2 tests, 200.69 seconds); the added
pause/replay test passed separately (1 test, 26.58 seconds). Selected retained
21:9 live/replay and 16:9 extra-time goalmouth PNGs were inspected: the new margins
show terrain and players, while the pre-extension shootout was centered with black side borders.
These results are evidence for that executable; rerun after renderer changes.

Final verification after the decoder update on 2026-10-01: **3 passed in
213.68 seconds** with the extended test enabled. Executable SHA-256:
`c36f2946cf24acc8f9c712aaf265dea29dc8e1fba28e50b9b4f09fbc102d92f2`.
The terminal log is `build/visual-match-acceptance-final/pytest.log`; per-scene
logs, manifests and captures are beside it. All eleven scenes passed at all
three widths, with thirteen captures per scene/presentation, exact full-WRAM
comparison and exact original-center comparison. Final 21:9 live/replay/shootout
and 16:9 extra-time goalmouth PNGs were visually inspected again. That pre-extension shootout
entry was centered with black side borders; completed shootouts remain outside
coverage. The penalty-scenery follow-up below supersedes that presentation.


Penalty-scenery follow-up on 2026-10-01: **3 passed in 279.40 seconds** with the
extended test enabled. Sixteen scenes, 53 scene/viewport runs and 1,197 captures
passed. This includes 16:10/16:9/21:9 ready, real shot, opponent-turn and third-turn
comparisons, with every sampled center pixel and full final WRAM identical to
the original-width run. Selected 16:10 shot-result, 16:9 opponent-turn and 21:9
ready/shot/third-turn PNGs were visually inspected; scenery fills the margins
and the single goal, players and HUD retain their original scale and center.
Log: `build/graphics-penalty-acceptance.log`; captures and manifests:
`build/graphics-penalty-acceptance`. Final Windows executable SHA-256:
`16eaffbe8e5434246ec4033be0bdeca8349cd36475df6509cd5294b064a55511`.
