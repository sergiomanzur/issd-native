from pathlib import Path
import subprocess
import pytest
from test_config_persistence import compile_c


def test_full_name_has_no_caret_upload(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "name.exe"
    compile_c(exe, repo, [repo / "tests/test_bugfix_name.c", repo / "ISSDNative/issd_bugfix_name.c"], [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_original_cartridge_name_caret_upload(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    rom = repo / "International Superstar Soccer Deluxe (USA).sfc"
    if not rom.exists():
        pytest.skip("requires user's supported cartridge ROM")
    wrapper = tmp_path / "name-rom.c"
    wrapper.write_text('#define ISSD_NAME_ROM_TEST 1\n#include "' + (repo / "tests/test_bugfix_name.c").as_posix() + '"\n')
    exe = tmp_path / "name-rom.exe"
    compile_c(exe, repo, [wrapper, repo / "ISSDNative/issd_bugfix_name.c",
                         repo / "deps/snesrecomp/runner/src/snes/interp816.c"],
              [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"])
    result = subprocess.run([str(exe), str(rom)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
