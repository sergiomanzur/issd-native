"""Fit uses the requested output while retaining the original simulation."""
import os
from pathlib import Path
import subprocess

import pytest
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                               reason="requires native build and user's ROM")


def capture(folder, output, wide):
    folder.mkdir()
    config = folder / "isolated.cfg"
    config.write_text(f"aspect_ratio=7\ntrue_widescreen={int(wide)}\n"
                      "internal_res=0\nscaling_filter=0\n", encoding="ascii")
    result = subprocess.run([
        str(EXE), "--headless", "600", "--auto-start", "60",
        "--config", str(config), "--save-dir", str(folder / "saves"),
        "--mods-dir", str(folder / "empty-mods"), "--fit-output", output,
        "--screenshot", "frame.bmp", "--dump-state", "state", str(ROM),
    ], cwd=folder, capture_output=True, text=True, timeout=90,
       env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"))
    assert result.returncode == 0, result.stdout + result.stderr
    with Image.open(folder / "frame.bmp") as image:
        picture = image.convert("RGB")
    return picture, (folder / "state.wram").read_bytes()


@pytest.mark.parametrize("output,width", [
    ("1920x1080", 480), ("2560x1440", 426), ("1280x800", 426),
    ("800x600", 398), ("0x0", 398),
])
def test_fit_width_and_authentic_center(tmp_path, output, width):
    original, baseline = capture(tmp_path / "original", output, False)
    widened, actual = capture(tmp_path / "fit", output, True)
    assert original.size == (256, 224) and widened.size == (width, 224)
    assert actual == baseline, "display fitting changed the simulation"
    margin = (width - 256) // 2
    center = widened.crop((margin, 0, margin + 256, 224))
    assert ImageChops.difference(original, center).getbbox() is None
