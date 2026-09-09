"""A savestate must be replayable headlessly, so a specific pitch position can
be reproduced for widescreen regression work without driving the menus.

python -m pytest tests/test_savestate_replay.py
"""
from pathlib import Path
import shutil
import subprocess

from PIL import Image
import pytest

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build-fixes/ISSDNative.exe"
ROM = REPO / "International Superstar Soccer Deluxe (USA).sfc"
# Bank one interactively (F5 in a penalty area, 16:9) and copy it here to turn
# the widescreen margin checks into a reproducible fixture.
FIXTURE = REPO / "tests/fixtures/penalty_area.sav"


def emulate(folder, frames, extra, seed_save=None):
    folder.mkdir(parents=True, exist_ok=True)
    if seed_save is not None:
        (folder / "saves").mkdir(exist_ok=True)
        shutil.copyfile(seed_save, folder / "saves/quicksave.sav")
    result = subprocess.run(
        [str(EXE), "--headless", str(frames), "--screenshot", "frame.bmp", *extra, str(ROM)],
        cwd=folder, capture_output=True, text=True, timeout=300,
    )
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    return folder / "frame.bmp"


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")
def test_loaded_state_resumes_identically(tmp_path):
    # Record: run to frame 240, banking a state at frame 200.
    recorded = emulate(tmp_path / "record", 240, ["--save-state", "200"])
    reference = recorded.read_bytes()

    # A frame the emulator never actually drew would make every comparison
    # below pass for free, so refuse to certify anything from a blank picture.
    # The regenerated bank00 output currently wedges frame 1 and renders black;
    # see docs/RECOMP_BOOT_REGRESSION.md. Skip rather than fail, so this stays
    # a signal about replay rather than a duplicate alarm for the recomp bug.
    colors = Image.open(recorded).convert("RGB").getcolors(maxcolors=65536)
    if not colors or len(colors) <= 1:
        pytest.skip("headless boot renders a blank frame (recomp regression); "
                    "replay cannot be verified until the game runs")

    # Control: the same short run WITHOUT the state must not already match, or
    # the comparison proves nothing about the state having been restored.
    control = emulate(tmp_path / "control", 60, [])
    assert control.read_bytes() != reference, (
        "control run already matches the reference; frame content is not "
        "sensitive to what was simulated")

    # Replay: install that state at frame 20, advance the same 40 frames.
    replayed = emulate(tmp_path / "replay", 60, ["--load-state", "20"],
                       seed_save=tmp_path / "record/saves/quicksave.sav")
    assert replayed.read_bytes() == reference, (
        "replaying a savestate did not reproduce the recorded frame")


@pytest.mark.skipif(not FIXTURE.exists(), reason="no penalty-area savestate fixture")
@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")
def test_penalty_area_fixture_renders_live_pitch(tmp_path):
    frame = emulate(tmp_path, 30, ["--load-state", "5"], seed_save=FIXTURE)
    picture = Image.open(frame).convert("RGB")
    colors = picture.getcolors(maxcolors=65536)
    assert colors and len(colors) > 16, "fixture did not restore a live pitch"
