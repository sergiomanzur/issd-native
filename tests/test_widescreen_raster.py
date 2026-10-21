"""Real 2bpp weather pixels and color-window edges, without a ROM."""
from pathlib import Path
import subprocess

from test_config_persistence import c_compiler

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


def test_weather_and_framed_announcements(tmp_path):
    exe = tmp_path / "raster.exe"
    # The runner renderer uses GNU C extensions.
    result = subprocess.run([
        c_compiler(), "-std=c11", "-DSNESRECOMP_REVERSE_DEBUG=0",
        "-I" + str(RUNTIME), "-I" + str(RUNTIME / "snes"),
        str(ROOT / "tests/test_widescreen_raster.c"),
        str(RUNTIME / "snes/ppu_legacy.c"), "-o", str(exe),
    ], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
