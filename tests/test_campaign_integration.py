"""Production save context and explicit Continue, using isolated user storage."""
from pathlib import Path
import subprocess
import hashlib
import os
import json

import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's ROM")


def run(folder, *args):
    return subprocess.run([str(EXE), "--rom", str(ROM), "--headless", "80",
                           "--config", str(folder / "isolated.cfg"),
                           "--save-dir", str(folder / "owned-saves"), *args],
                          cwd=folder, capture_output=True, text=True, timeout=120)


def test_continue_requires_a_real_checkpoint(tmp_path):
    result = run(tmp_path, "--continue")
    assert result.returncode != 0, "Continue silently booted without a campaign"
    assert "checkpoint" in (result.stdout + result.stderr).lower()


def test_manual_saves_use_explicit_root_and_checked_envelope(tmp_path):
    result = run(tmp_path, "--save-state", "40")
    assert result.returncode == 0, result.stdout + result.stderr
    path = tmp_path / "owned-saves/quicksave.sav"
    assert path.exists(), "save-directory override was ignored"
    assert path.read_bytes()[:4] != b"SLTR", "new save lacks compatibility envelope"
    assert not (tmp_path / "saves/quicksave.sav").exists()
    restored = run(tmp_path, "--load-state", "20")
    assert restored.returncode == 0, restored.stdout + restored.stderr


@pytest.mark.parametrize("setting", ["debug_unhooked_code=1", "gameplay_goalkeeper_ai=1",
                                      "gameplay_player_ai=1", "gameplay_goalkeeper_ai=1\ngameplay_player_ai=1"])
def test_gameplay_context_change_rejects_manual_load(tmp_path, setting):
    assert run(tmp_path, "--save-state", "40").returncode == 0
    cfg = tmp_path / "changed.cfg"
    cfg.write_text(setting + "\n", encoding="utf-8")
    result = run(tmp_path, "--config", str(cfg), "--load-state", "20")
    assert result.returncode != 0
    assert "compatib" in (result.stdout + result.stderr).lower()


def test_presentation_change_keeps_manual_load_compatible(tmp_path):
    assert run(tmp_path, "--save-state", "40").returncode == 0
    cfg = tmp_path / "presentation.cfg"
    cfg.write_text("target_fps=240\nscaling_filter=0\n", encoding="utf-8")
    result = run(tmp_path, "--config", str(cfg), "--load-state", "20")
    assert result.returncode == 0, result.stdout + result.stderr


def test_same_filename_edited_pack_changes_applied_compatibility(tmp_path):
    mods = tmp_path / "mods"
    mods.mkdir()
    pack = json.loads((ROOT / "tests/fixtures/mods/fixture.json").read_text())
    path = mods / "unchanged-name.json"
    path.write_text(json.dumps(pack), encoding="utf-8")
    cfg = tmp_path / "modded.cfg"
    cfg.write_text("active_mod_packs=Fixture Pack\n", encoding="ascii")
    args = ("--config", str(cfg), "--mods-dir", str(mods))
    saved = run(tmp_path, *args, "--save-state", "40")
    assert saved.returncode == 0, saved.stdout + saved.stderr
    # Reformatting the pack preserves effective cartridge data.
    path.write_text(json.dumps(pack, indent=2), encoding="utf-8")
    same = run(tmp_path, *args, "--load-state", "20")
    assert same.returncode == 0, same.stdout + same.stderr
    # A meaningful in-place edit retains its path/name/version yet changes ROM.
    pack["teams"][0]["players"][0]["name"] = "Changed"
    path.write_text(json.dumps(pack), encoding="utf-8")
    changed = run(tmp_path, *args, "--load-state", "20")
    assert changed.returncode != 0
    assert "compatib" in (changed.stdout + changed.stderr).lower()


@pytest.mark.parametrize("world", [False, True], ids=["cup", "world-series"])
def test_campaign_setup_autosaves_and_continues(tmp_path, world):
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n"
    if world:
        script += "530 DOWN\n540 NONE\n"
    script += "560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1300, 100))
    input_path = tmp_path / "input.txt"
    input_path.write_text(script, encoding="ascii")
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy")
    result = subprocess.run(
        [str(EXE), "--rom", str(ROM), "--headless", "1302", "--script", str(input_path),
         "--config", str(tmp_path / "isolated.cfg"), "--save-dir", str(tmp_path / "owned-saves")],
        cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    saves = list((tmp_path / "owned-saves/campaigns").glob("*/campaign.sav"))
    assert len(saves) == 1, "verified campaign setup did not create its autosave"
    original = saves[0].read_bytes()
    assert original[:4] == b"ISCE"
    assert int.from_bytes(original[24:32], "little") == 1, "setup saved repeatedly"
    assert original[160:192] == hashlib.sha256(original[:160] + original[192:]).digest()
    expected = b"World Series setup: Brazil" if world else b"Cup setup: Brazil"
    assert original[80:144].split(b"\0")[0] == expected
    continued = run(tmp_path, "--continue", "--dump-state", str(tmp_path / "continued"))
    assert continued.returncode == 0, continued.stdout + continued.stderr
    ram = (tmp_path / "continued.wram").read_bytes()
    assert int.from_bytes(ram[0x32:0x34], "little") == 6
    assert int.from_bytes(ram[0x70:0x72], "little") == 12
    assert saves[0].read_bytes() == original, "Continue rewrote the restored checkpoint"
