import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_hd_tile_replacement(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_hd.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_hd.c",
            repo / "ISSDNative/issd_hd.c",
        ],
        [
            repo / "ISSDNative",
            repo / "deps/snesrecomp/runner/src",
            repo / "deps/snesrecomp/runner/src/snes",
        ],
    )
    result = subprocess.run([str(exe), str(tmp_path)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "passed" in result.stdout, result.stdout
