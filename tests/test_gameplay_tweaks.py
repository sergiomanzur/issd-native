from pathlib import Path
import subprocess
from test_config_persistence import compile_c


def test_gameplay_decisions(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "gameplay.exe"
    compile_c(exe, repo, [repo / "tests/test_gameplay_tweaks.c",
                         repo / "ISSDNative/issd_gameplay.c"], [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
