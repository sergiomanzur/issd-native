import shutil
import subprocess
from pathlib import Path


def test_windows_launcher_cfg_round_trip(tmp_path):
    if not shutil.which("gcc"):
        raise AssertionError("gcc is required for the Windows launcher self-test")
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "ISSDNativeLauncherTest.exe"
    cfg = tmp_path / "issd_native.cfg"
    result = subprocess.run(
        [
            "gcc",
            "-O2",
            str(repo / "tools/windows_launcher/ISSDNativeLauncher.c"),
            "-o",
            str(exe),
            "-lcomdlg32",
        ],
        capture_output=True,
        text=True,
        cwd=repo,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe), "--launcher-self-test", str(cfg)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
