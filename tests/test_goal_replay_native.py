"""Naturally scored goal playback across widths, with no guest-state edits."""
import json
import pytest

from test_password_flow import guest, EXE, ROM
from test_game_acceptance import word
from test_match_visual_acceptance import compare_scene, PENALTY_WIDTHS

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


@pytest.fixture(scope="module")
def original_goal_replay(tmp_path_factory):
    folder = tmp_path_factory.mktemp("natural-goal-replay")
    (folder / "isolated.cfg").write_text(
        f"engine_mode=1\ninternal_res=0\ntrue_widescreen=0\nmods_dir={folder / 'empty-mods'}\n")
    inputs = {f: "START" for f in range(60, 361, 60)}
    inputs.update({f+10: "NONE" for f in range(60, 361, 60)})
    inputs.update({560: "A", 570: "NONE", 3000: "NONE"})
    for f in range(800, 3000, 150):
        inputs.update({f: "A", f+10: "NONE"})
    script = folder / "input.txt"
    script.write_text("".join(f"{f} {b}\n" for f,b in sorted(inputs.items())))
    result = guest(folder, 6420, "--script", str(script), "--save-state", "6420",
                   "--dump-state", str(folder / "goal"))
    (folder / "run.log").write_text(result.stdout+result.stderr)
    assert "[interp_cap]" not in result.stderr
    ram = (folder / "goal.wram").read_bytes()
    assert word(ram, 0x70) == 0x13 and word(ram, 0x72) == 6
    assert (word(ram, 0xda2), word(ram, 0xea2)) == (0, 1)
    absent = [o for o in range(0x500, 0x1b00, 0x100)
              if word(ram, o+0x1e) and word(ram, o+0x14)&0x8000
              and -71 <= int.from_bytes(ram[o+8:o+10], "little", signed=True) < 0]
    assert len(absent) >= 3, "missing real stale offscreen replay actors"
    (folder / "original-evidence.json").write_text(json.dumps(
        dict(naturally_scored_goal=True, guest_state_edits=False,
             absent_edge_players=[hex(o) for o in absent]), indent=2))
    return (folder / "owned-saves/quicksave.sav").read_bytes()


@pytest.mark.parametrize("controls,expected", [
    ("0 NONE\n", {}),
    ("0 NONE\n20 X\n30 NONE\n", {0x18b4: 1, 0x18b2: 1}),
    ("0 NONE\n20 B\n70 NONE\n", {0x18b4: 1, 0x18b2: 1, 0x18ae: 1}),
], ids=["automatic", "paused", "rewind"])
def test_natural_goal_replay_preserves_native_center(original_goal_replay, tmp_path, controls, expected):
    compare_scene(tmp_path, "goal-replay", original_goal_replay, 90, controls,
                  expected={0x70: 0x13, 0x72: 6, **expected}, widths=PENALTY_WIDTHS,
                  capture_start=75, config_overrides={"mods_dir": str(tmp_path / "empty-mods")})
