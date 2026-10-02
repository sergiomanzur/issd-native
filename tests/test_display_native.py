"""Exercise the real SDL software presentation path without a physical display."""
import json
import os
from pathlib import Path
import subprocess
import struct

import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="requires native build and user's ROM")
@pytest.mark.parametrize("size,aspect,integer,filter_id", [
    ((320, 240), 0, 0, 0), ((1280, 720), 2, 1, 3),
    ((1920, 1080), 4, 0, 3), ((3840, 2160), 0, 0, 3),
])
def test_sdl_display_report(tmp_path, size, aspect, integer, filter_id):
    config = tmp_path / "graphics.json"
    config.write_text(json.dumps({
        "window_width": size[0], "window_height": size[1],
        "aspect_ratio": aspect, "true_widescreen": 1, "integer_scaling": integer,
        "scaling_filter": filter_id, "internal_res": 0, "vsync": 0, "target_fps": 0,
    }, indent=2))
    report = tmp_path / "display.json"
    run = subprocess.run([
        str(EXE), "--rom", str(ROM), "--config", str(config),
        "--save-dir", str(tmp_path / "saves"), "--frames", "60",
        "--graphics-report", str(report),
    ], cwd=tmp_path, env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"),
        capture_output=True, text=True, timeout=90)
    assert run.returncode == 0, run.stdout + run.stderr
    data = json.loads(report.read_text())
    assert data["simulation_frames"] == 60
    assert data["presentations"] > 0
    assert (data["output_width"], data["output_height"]) == size
    assert data["cpu_present_mean_ms"] >= 0
    assert data["present_max_ms"] >= data["present_mean_ms"] > 0
    assert data["intermediate_width"] >= data["native_width"]
    assert data["intermediate_width"] <= 4096
    assert data["filter"] == filter_id


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="requires native build and user's ROM")
def test_live_sharp_readability_software_presentation(tmp_path):
    config = tmp_path / "live.json"
    config.write_text(json.dumps({
        "window_width": 1280, "window_height": 720, "aspect_ratio": 2,
        "true_widescreen": 1, "scaling_filter": 3, "internal_res": 0,
        "vsync": 0, "target_fps": 60, "ball_outline": 1, "ball_shadow": 1,
        "player_markers": 1, "player_names": 1, "hud_scale": 2,
        "radar_scale": 2, "radar_position": 2, "radar_opacity": 50,
    }, indent=2))
    report = tmp_path / "live-report.json"
    result = subprocess.run([
        str(EXE), "--rom", str(ROM), "--config", str(config),
        "--save-dir", str(tmp_path / "saves"), "--frames", "1200", "--auto-start", "60",
        "--graphics-report", str(report), "--dump-state", str(tmp_path / "state"),
    ], cwd=tmp_path, env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"),
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    ram = (tmp_path / "state.wram").read_bytes()
    assert struct.unpack_from("<H", ram, 0x32)[0] == 6
    assert struct.unpack_from("<H", ram, 0x70)[0] == 8
    data = json.loads(report.read_text())
    assert data["simulation_frames"] == 1200 and data["presentations"] >= 1200
    assert (data["output_width"], data["output_height"]) == (1280, 720)
    assert data["native_width"] == 398 and data["filter"] == 3
    assert data["intermediate_width"] >= 398
    assert data["cpu_present_mean_ms"] > 0
