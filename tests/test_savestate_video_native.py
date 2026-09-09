import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_savestate_round_trips_video_memory(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_savestate_video_native.exe"
    compile_c(
        exe,
        repo,
        [repo / "tests/test_savestate_video_native.c", repo / "ISSDNative/issd_save.c"],
        [
            repo / "ISSDNative",
            repo / "deps/snesrecomp/runner/src",
            repo / "deps/snesrecomp/runner/src/snes",
        ],
    )
    result = subprocess.run([str(exe)], capture_output=True, text=True, cwd=tmp_path)
    assert result.returncode == 0, result.stdout + result.stderr
