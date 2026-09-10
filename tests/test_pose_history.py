import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_pose_history_continuation(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_pose_history.exe"
    compile_c(
        exe,
        repo,
        [repo / "tests/test_pose_history.c", repo / "ISSDNative/issd_pose_history.c"],
        [repo / "ISSDNative"],
    )
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
