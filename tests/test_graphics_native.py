"""Actual cartridge rendering: readability changes pixels, never game WRAM."""
import json
import os
from pathlib import Path
import struct
import subprocess

from PIL import Image, ImageChops
import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"

@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="requires native build and user's ROM")
def test_live_readability_changes_only_presentation(tmp_path):
    states, pictures = [], []
    for enabled in (False, True):
        folder = tmp_path / str(enabled)
        folder.mkdir()
        config = folder / "graphics.json"
        config.write_text(json.dumps({
            "aspect_ratio": 2, "true_widescreen": 1,
            "ball_outline": int(enabled), "ball_shadow": int(enabled),
            "player_markers": int(enabled), "player_names": int(enabled),
            "radar_scale": 3 if enabled else 1,
        }, indent=2))
        result = subprocess.run([
            str(EXE), "--rom", str(ROM), "--headless", "1200", "--auto-start", "60",
            "--config", str(config), "--save-dir", str(folder / "saves"),
            "--dump-state", str(folder / "state"), "--screenshot", str(folder / "frame.bmp"),
        ], cwd=folder, env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"),
            capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
        ram = (folder / "state.wram").read_bytes()
        assert struct.unpack_from("<H", ram, 0x32)[0] == 6
        assert struct.unpack_from("<H", ram, 0x70)[0] == 8
        states.append(ram)
        pictures.append(Image.open(folder / "frame.bmp").convert("RGB"))
    assert states[0] == states[1], "readability settings changed game state"
    assert pictures[0].size == pictures[1].size == (398, 224)
    assert ImageChops.difference(*pictures).getbbox(), "enabled readability did not reach scanout"
    assert ImageChops.difference(pictures[0].crop((0, 0, 398, 24)),
                                pictures[1].crop((0, 0, 398, 24))).getbbox() is None
