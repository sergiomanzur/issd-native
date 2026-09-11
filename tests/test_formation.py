import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_formation_library(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_formation.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_formation.c",
            repo / "ISSDNative/issd_formation.c",
        ],
        [repo / "ISSDNative"],
    )
    result = subprocess.run([str(exe)], cwd=repo, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "passed" in result.stdout, result.stdout
