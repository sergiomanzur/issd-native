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
EXE = REPO / "build/ISSDNative.exe"
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
        [str(EXE), "--headless", str(frames), "--save-dir", str(folder / "saves"),
         "--config", str(folder / "issd_native.cfg"), "--allow-legacy-save",
         "--screenshot", "frame.bmp", *extra, str(ROM)],
        cwd=folder, capture_output=True, text=True, timeout=300,
    )
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    return folder / "frame.bmp"


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")
def test_loaded_state_resumes_identically(tmp_path):
    # Record: run to frame 240, banking a state at frame 200.
    recorded = emulate(tmp_path / "record", 240,
                       ["--save-state", "200", "--dump-frames", "200:239",
                        "--dump-state", "state"])
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
    replayed = emulate(tmp_path / "replay", 60,
                       ["--load-state", "20", "--dump-frames", "20:59",
                        "--dump-state", "state"],
                       seed_save=tmp_path / "record/saves/quicksave.sav")
    assert replayed.read_bytes() == reference, (
        "replaying a savestate did not reproduce the recorded frame")
    advancing = set()
    for offset in range(40):
        expected = (tmp_path / "record" / f"f_{200 + offset:05d}.bmp").read_bytes()
        actual = (tmp_path / "replay" / f"f_{20 + offset:05d}.bmp").read_bytes()
        assert actual == expected, f"first replay frame divergence at offset {offset}"
        advancing.add(expected)
    assert len(advancing) > 1, "restored state did not produce advancing frames"
    assert (tmp_path / "record/state.wram").read_bytes() == (
        tmp_path / "replay/state.wram").read_bytes(), "replay guest WRAM diverged"


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")
@pytest.mark.parametrize("aspect", [0, 2], ids=["classic", "widescreen"])
def test_live_match_snapshot_advances_identically(tmp_path, aspect):
    """Restore real play with identical scripted input, including the first frame."""
    for name in ("bootstrap", "record", "replay"):
        folder = tmp_path / name
        folder.mkdir()
        (folder / "issd_native.cfg").write_text(
            f"aspect_ratio={aspect}\ntrue_widescreen=1\n", encoding="ascii")
    bootstrap = tmp_path / "bootstrap"
    emulate(bootstrap, 1200, ["--auto-start", "60", "--save-state", "1200",
                              "--dump-state", "state"])
    ram = (bootstrap / "state.wram").read_bytes()
    assert int.from_bytes(ram[0x70:0x72], "little") == 8, "fixture did not reach live play"
    script = tmp_path / "input.txt"
    script.write_text("0 P1 RIGHT\n", encoding="ascii")
    recorded = emulate(tmp_path / "record", 240,
                       ["--load-state", "0", "--save-state", "200", "--script", str(script),
                        "--dump-frames", "200:239", "--dump-state", "state"],
                       seed_save=bootstrap / "saves/quicksave.sav")
    replayed = emulate(tmp_path / "replay", 60,
                       ["--load-state", "20", "--script", str(script),
                        "--dump-frames", "20:59", "--dump-state", "state"],
                       seed_save=tmp_path / "record/saves/quicksave.sav")
    assert recorded.read_bytes() == replayed.read_bytes(), "live match final image diverged"
    advancing = set()
    for offset in range(40):
        expected = (tmp_path / "record" / f"f_{200 + offset:05d}.bmp").read_bytes()
        actual = (tmp_path / "replay" / f"f_{20 + offset:05d}.bmp").read_bytes()
        assert actual == expected, f"first live match frame divergence at offset {offset}"
        advancing.add(expected)
    assert len(advancing) > 1, "live match did not advance"
    assert (tmp_path / "record/state.wram").read_bytes() == (
        tmp_path / "replay/state.wram").read_bytes(), "live match guest WRAM diverged"


@pytest.mark.skipif(not FIXTURE.exists(), reason="no penalty-area savestate fixture")
@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")
def test_penalty_area_fixture_renders_live_pitch(tmp_path):
    frame = emulate(tmp_path, 30, ["--load-state", "5"], seed_save=FIXTURE)
    picture = Image.open(frame).convert("RGB")
    colors = picture.getcolors(maxcolors=65536)
    assert colors and len(colors) > 16, "fixture did not restore a live pitch"
