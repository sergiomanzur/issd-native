"""Real input and real sprite rasterization; no gameplay WRAM edits."""
import os
from pathlib import Path
import shutil
import subprocess

import pytest
from PIL import Image, ImageChops
from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")


def run(folder, frames, *, enabled=False, aspect=0, seed=None, script=None, extra=()):
    folder.mkdir(parents=True, exist_ok=True)
    saves = folder / "saves"
    saves.mkdir(exist_ok=True)
    cfg = folder / "isolated.cfg"
    cfg.write_text(f"aspect_ratio={aspect}\ntrue_widescreen=1\n"
                   f"enhanced_running_animation={int(enabled)}\n", encoding="ascii")
    args = [str(EXE), "--headless", str(frames), "--config", str(cfg),
            "--save-dir", str(saves), "--mods-dir", str(folder / "empty-mods"),
            "--screenshot", "final.bmp", "--dump-state", "state", *extra, str(ROM)]
    if seed:
        shutil.copyfile(seed, saves / "quicksave.sav")
        args += ["--load-state", "0"]
    if script:
        path = folder / "input.txt"
        path.write_text(script, encoding="ascii")
        args += ["--script", str(path)]
    result = subprocess.run(args, cwd=folder, capture_output=True, text=True, timeout=180,
                            env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"))
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "[interp_cap]" not in result.stderr
    return folder


@pytest.fixture(scope="module")
def live_seed(tmp_path_factory):
    folder = run(tmp_path_factory.mktemp("running-live"), 600,
                 extra=("--auto-start", "60", "--save-state", "600"))
    ram = (folder / "state.wram").read_bytes()
    assert int.from_bytes(ram[0x70:0x72], "little") == 8
    return folder / "saves/quicksave.sav"


def test_all_real_running_midpoints(tmp_path, live_seed):
    exe = tmp_path / "atlas.exe"
    compile_c(exe, ROOT, [ROOT / "tests/running_animation_atlas.c",
                         ROOT / "ISSDNative/issd_animation.c"],
              [ROOT / "ISSDNative", ROOT / "deps/snesrecomp/runner/src"])
    boot = live_seed.parent.parent
    output = tmp_path / "running-atlas.ppm"
    result = subprocess.run([str(exe), str(ROM), str(boot / "state.wram"),
                             str(boot / "state.ppu"), str(output)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "64 of 64" in result.stdout
    # Save a reviewable local artifact without redistributing cartridge art.
    artifacts = ROOT / "build/running-animation-acceptance"
    artifacts.mkdir(exist_ok=True)
    image = Image.open(output)
    image.resize((image.width * 2, image.height * 2), Image.Resampling.NEAREST).save(artifacts / "running-atlas.png")


@pytest.mark.parametrize("aspect", [0, 2], ids=["classic", "widescreen"])
def test_running_changes_pixels_without_changing_match(tmp_path, live_seed, aspect):
    # Drive every direction naturally. AI running provides additional overlap/depth
    # coverage; unit tests separately pin all eight strip selectors and palettes.
    directions = ["UP,Y", "UP,RIGHT,Y", "RIGHT,Y", "DOWN,RIGHT,Y", "DOWN,Y",
                  "DOWN,LEFT,Y", "LEFT,Y", "UP,LEFT,Y"]
    script = "0 P1 B\n10 NONE\n" + "".join(f"{20+i * 60} P1 {direction}\n" for i, direction in enumerate(directions))
    original = run(tmp_path / "original", 480, aspect=aspect, seed=live_seed, script=script,
                   extra=("--dump-frames", "30:119", "--save-state", "480"))
    enhanced = run(tmp_path / "enhanced", 480, enabled=True, aspect=aspect, seed=live_seed, script=script,
                   extra=("--dump-frames", "30:119", "--save-state", "480"))
    assert (original / "state.wram").read_bytes() == (enhanced / "state.wram").read_bytes()
    # Snapshot guest payload must stay identical, including VRAM, OAM, CGRAM,
    # CPU/APU clocks and serialized animation history. This is a visual setting.
    a = (original / "saves/quicksave.sav").read_bytes()
    b = (enhanced / "saves/quicksave.sav").read_bytes()
    # Checked envelopes include wall-clock creation times and their hashes.
    # Compare the complete guest/host snapshot, excluding only that envelope.
    assert a[int.from_bytes(a[12:16], "little"):] == b[int.from_bytes(b[12:16], "little"):]
    changed = 0
    for n in range(30, 120):
        a = Image.open(original / f"f_{n:05d}.bmp").convert("RGB")
        b = Image.open(enhanced / f"f_{n:05d}.bmp").convert("RGB")
        difference = ImageChops.difference(a, b)
        if difference.getbbox():
            changed += 1
            # Player OBJ pieces can also overlap HUD panels in the original
            # game. Their priority remains native; assert a sparse sprite-sized
            # change rather than falsely requiring empty rectangular HUD bands.
            pixels = sum(pixel != (0, 0, 0) for pixel in difference.getdata())
            assert pixels < 1000, "running enhancement changed more than sprite pixels"
    assert changed > 3, "enhanced running did not reach the actual rasterized match"


@pytest.mark.parametrize("aspect", [0, 2], ids=["classic", "widescreen"])
def test_enhanced_running_replay_is_identical(tmp_path, live_seed, aspect):
    record = run(tmp_path / "record", 80, enabled=True, aspect=aspect, seed=live_seed,
                 script="0 P1 B\n10 P1 RIGHT,Y\n", extra=("--save-state", "40", "--dump-frames", "40:79"))
    # Replay uses the same held input across the restore boundary.
    replay_folder = tmp_path / "replay"
    replay_folder.mkdir()
    result = run(replay_folder, 40, enabled=True, aspect=aspect,
                 seed=record / "saves/quicksave.sav", script="0 P1 RIGHT,Y\n",
                 extra=("--dump-frames", "0:39"))
    for offset in range(40):
        assert (record / f"f_{40+offset:05d}.bmp").read_bytes() == (
            result / f"f_{offset:05d}.bmp").read_bytes(), f"first divergence {offset}"
    assert (record / "state.wram").read_bytes() == (result / "state.wram").read_bytes()
