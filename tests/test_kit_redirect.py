import json
import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_native_kit_redirect_isolates_home_and_away(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "kit_redirect.exe"
    compile_c(exe, repo, [repo / "tests/test_kit_redirect.c",
                         repo / "ISSDNative/issd_mod_rom.c",
                         repo / "ISSDNative/issd_mod.c",
                         repo / "ISSDNative/issd_formation.c"],
              [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], cwd=repo, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_shipped_packs_give_the_formerly_shared_teams_distinct_records():
    """The retail table ships no shared strips; these packs must not create
    any. The set is the old pre-pointer-table assumption of eight teams on two
    strips, kept as the likely places a pack would induce sharing. The measured
    truth is the assertion: each names a distinct spare record in 36-43."""
    repo = Path(__file__).resolve().parents[1]
    former_shared = {15, 18, 19, 20, 21, 22, 23, 26}
    for name in ("liga_mx_expansion", "world_cup_2026"):
        teams = json.loads((repo / "mods" / (name + ".json")).read_text(encoding="utf-8"))["teams"]
        overrides = [team["kit_record"] for team in teams
                     if team["team_id"] in former_shared]
        assert len(overrides) == len(set(overrides))
        assert all(36 <= record < 44 for record in overrides)
