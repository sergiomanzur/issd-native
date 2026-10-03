from pathlib import Path
import subprocess
from test_config_persistence import compile_c


def test_eight_direction_running_tiles_are_transactional(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "running.exe"
    compile_c(exe, repo, [repo / "tests/test_running_animation.c",
                         repo / "ISSDNative/issd_running.c",
                         repo / "ISSDNative/issd_animation.c"],
              [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
