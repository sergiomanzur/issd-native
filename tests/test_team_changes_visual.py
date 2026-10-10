"""Original management input remains deterministic at all wider aspects."""
import subprocess
import pytest
from team_changes_helpers import (bootstrap, REQUEST_MENU, OPEN_FORMATION,
    CHANGE_FORMATION, EXIT_FORMATION, OPEN_SQUAD_AFTER_FORMATION, SUBSTITUTE,
    EXIT_SQUAD, RESUME, ADVANCE_LIVE, EXE, ROM, ROOT, word)
from test_match_visual_acceptance import compare_scene, PENALTY_WIDTHS
from test_config_persistence import compile_c

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
    reason="requires built native executable and owned retail ROM")
FLAGS = {"gameplay_goalkeeper_ai": 1, "gameplay_player_ai": 1}


def test_original_team_changes_visual_widths(tmp_path):
    seed, initial = bootstrap(tmp_path / "bootstrap", flags=3)
    before_lineup = initial[0x3f90:0x3fa4]
    stages = (("request-management", REQUEST_MENU),
        ("formation-menu", OPEN_FORMATION), ("formation-choice", CHANGE_FORMATION),
        ("formation-exit", EXIT_FORMATION), ("squad-menu", OPEN_SQUAD_AFTER_FORMATION),
        ("substitution-pending", SUBSTITUTE), ("squad-exit", EXIT_SQUAD),
        ("native-return", RESUME), ("live-after-throw-in", ADVANCE_LIVE))
    for name, (frames, script) in stages:
        current, seed = compare_scene(tmp_path, name, seed, frames, script,
            widths=PENALTY_WIDTHS, config_overrides=FLAGS)
        assert word(current, 0x70) == (8 if name in ("native-return", "live-after-throw-in") else 0x0c), name
        if name == "formation-choice":
            assert word(current, 0x15f6) == 2
            assert current[0xda6] == initial[0xda6], "preview must not commit early"
        if name == "formation-exit":
            assert current[0xda6] == 2 != initial[0xda6]
        if name in ("substitution-pending", "native-return", "live-after-throw-in"):
            assert (current[0x3f91] & 31) == 12 != (before_lineup[1] & 31)
        if name == "native-return":
            assert current[0x3f9c] & 0x80, "outgoing player substitution not completed"
        if name == "live-after-throw-in":
            assert word(current, 0xbc) == 0, "stoppage must resume through actual throw-in"
    # Ask the shipped name consumer about actual snapshots. The original
    # bootstrap actor is culled and must have no label; after the real swap,
    # the visible actor must follow the incoming roster identity.
    name_exe = tmp_path / "name.exe"
    compile_c(name_exe, ROOT, [ROOT / "tests/team_changes_name_probe.c",
        ROOT / "ISSDNative/issd_readability.c", ROOT / "ISSDNative/issd_camera.c"],
        [ROOT / "ISSDNative"])
    names = []
    for label, ram in (("before", initial), ("after", current)):
        snapshot = tmp_path / (label + ".wram")
        snapshot.write_bytes(ram)
        result = subprocess.run([str(name_exe), str(snapshot), "0x600"],
            capture_output=True, text=True)
        if label == "before":
            assert word(ram, 0x600) == 0, "bootstrap actor should be culled"
            assert result.returncode == 4 and not result.stdout
        else:
            assert result.returncode == 0, result.stderr
            names.append(result.stdout.strip())
    assert names == ["Bucario"], names
