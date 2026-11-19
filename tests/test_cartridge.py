import subprocess
from pathlib import Path

import pytest

from test_config_persistence import compile_c


def test_cartridge_module(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    rom = repo / "International Superstar Soccer Deluxe (USA).sfc"
    if not rom.exists():
        pytest.skip("requires user's supported cartridge ROM")

    exe = tmp_path / "test_cartridge.exe"
    compile_c(
        exe,
        repo,
        [
            repo / "tests/test_cartridge.c",
            repo / "ISSDNative/issd_cartridge.c",
            repo / "ISSDNative/issd_mod.c",
            repo / "deps/snesrecomp/runner/src/sha256.c",
        ],
        [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"],
    )
    result = subprocess.run([str(exe), str(tmp_path)], cwd=repo,
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "cartridge tests passed" in result.stdout, result.stdout
