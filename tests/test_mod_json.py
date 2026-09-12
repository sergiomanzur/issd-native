import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_mod_pack_is_real_json(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_mod_json.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_mod_json.c",
            repo / "ISSDNative/issd_mod.c",
        ],
        [repo / "ISSDNative"],
    )
    result = subprocess.run([str(exe), str(tmp_path)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "passed" in result.stdout, result.stdout
