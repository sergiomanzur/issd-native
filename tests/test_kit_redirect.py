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


def test_shipped_pack_shared_kits_have_unique_redirects():
    repo = Path(__file__).resolve().parents[1]
    shared = {15, 18, 19, 20, 21, 22, 23, 26}
    for name in ("liga_mx_expansion", "world_cup_2026"):
        teams = json.loads((repo / "mods" / (name + ".json")).read_text(encoding="utf-8"))["teams"]
        overrides = [team["kit_record"] for team in teams if team["team_id"] in shared]
        assert len(overrides) == len(set(overrides))
        assert all(36 <= record < 44 for record in overrides)
