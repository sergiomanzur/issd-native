import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_video_geometry(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_video.exe"
    compile_c(exe, repo,
              [repo / "tests/test_video.c", repo / "ISSDNative/issd_video.c"],
              [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
