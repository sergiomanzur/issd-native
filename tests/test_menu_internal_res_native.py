import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_internal_res_row_is_inert_outside_crt(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_menu_internal_res_native.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_menu_internal_res_native.c",
            repo / "ISSDNative/issd_menu.c",
            repo / "ISSDNative/issd_config.c",
        ],
        [repo / "ISSDNative"],
    )
    result = subprocess.run([str(exe)], capture_output=True, text=True, cwd=tmp_path)
    assert result.returncode == 0, result.stdout + result.stderr
