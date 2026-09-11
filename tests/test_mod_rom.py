import subprocess
from pathlib import Path

from test_config_persistence import compile_c


def test_mod_rom_patching(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "test_mod_rom.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_mod_rom.c",
            repo / "ISSDNative/issd_mod_rom.c",
            repo / "ISSDNative/issd_mod.c",
        ],
        [repo / "ISSDNative"],
    )
    result = subprocess.run([str(exe)], cwd=repo, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "skipped" not in result.stdout, result.stdout
