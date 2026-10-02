import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_widescreen_descriptor_continuation(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_widescreen_reliability.exe"
    compile_c(exe, repo, [
        repo / "tests/test_widescreen_reliability.c",
        repo / "ISSDNative/issd_widescreen.c",
        repo / "ISSDNative/issd_pose_history.c",
        repo / "ISSDNative/issd_animation.c",
    ], [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
