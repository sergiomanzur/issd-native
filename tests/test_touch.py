import subprocess
from pathlib import Path
import pytest

from test_config_persistence import compile_c


def test_touch_controls(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_touch.exe"
    compile_c(
        exe,
        repo,
        [repo / "tests/test_touch.c", repo / "ISSDNative/issd_touch.c"],
        [repo / "ISSDNative"],
    )
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.fixture(scope="module")
def touch_layout(tmp_path_factory):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path_factory.mktemp("touch-layout") / "test_touch_layout.exe"
    compile_c(exe,repo,[repo / "tests/test_touch_layout.c",repo / "ISSDNative/issd_touch.c"],
              [repo / "ISSDNative"])
    return exe


@pytest.mark.parametrize("case", ["default","moved","scaled","rotation","bounds","invalid",
                                 "reset","layout-reset","viewport-reset","same-config","extreme-dpad",
                                 "center","unknown-center"])
def test_touch_layout(touch_layout,case):
    result = subprocess.run([str(touch_layout),case],capture_output=True,text=True)
    assert result.returncode == 0, result.stdout + result.stderr
