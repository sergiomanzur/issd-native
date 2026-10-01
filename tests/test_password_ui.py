"""Password page input and rendering without SDL or the cartridge codec."""
from pathlib import Path
import subprocess

import pytest
from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def password_ui_exe(tmp_path_factory):
    exe = tmp_path_factory.mktemp("password-ui") / "password-ui.exe"
    compile_c(exe, ROOT,
              [ROOT / "tests/test_password_ui.c", ROOT / "ISSDNative/issd_password_ui.c"],
              [ROOT / "ISSDNative"])
    return exe


@pytest.mark.parametrize("case", ["navigation", "text", "callbacks", "touch", "render"])
def test_password_page(password_ui_exe, case):
    result = subprocess.run([str(password_ui_exe), case], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
