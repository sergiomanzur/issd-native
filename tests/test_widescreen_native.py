import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_native_widescreen_preparation(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_widescreen_native.exe"
    compile_c(
        exe,
        repo,
        [repo / "tests/test_widescreen_native.c", repo / "ISSDNative/issd_widescreen.c"],
        [
            repo / "ISSDNative",
            repo / "deps/snesrecomp/runner/src",
            repo / "deps/snesrecomp/runner/src/snes",
        ],
    )
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
