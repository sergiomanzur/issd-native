import subprocess
from pathlib import Path
from test_config_persistence import compile_c


def test_authored_animation_interpreter(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_animation.exe"
    compile_c(exe, repo, [repo / "tests/test_animation.c",
                         repo / "ISSDNative/issd_animation.c"],
              [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
