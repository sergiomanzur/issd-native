from pathlib import Path
import subprocess
import pytest
from test_config_persistence import compile_c


def test_original_keeper_movement_is_once_per_actor(tmp_path):
    root = Path(__file__).resolve().parents[1]
    exe = tmp_path / "keeper.exe"
    compile_c(exe, root, [root / "tests/test_bugfix_keeper.c",
                          root / "ISSDNative/issd_bugfix_keeper.c"], [root / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_cartridge_keeper_branch_and_integration_opcodes(tmp_path):
    root = Path(__file__).resolve().parents[1]
    rom = root / "International Superstar Soccer Deluxe (USA).sfc"
    if not rom.exists():
        pytest.skip("requires the user's original ROM")
    runtime = root / "deps/snesrecomp/runner/src/snes"
    exe = tmp_path / "keeper_opcodes.exe"
    # Existing helper keeps strict compiler diagnostics. This tiny include
    # wrapper enables the real interpreter fixture in the same C test file.
    wrapper = tmp_path / "keeper_opcodes.c"
    wrapper.write_text('#define KEEPER_OPCODE_TEST 1\n#include "test_bugfix_keeper.c"\n')
    compile_c(exe, root, [wrapper, root / "ISSDNative/issd_bugfix_keeper.c",
                          runtime / "interp816.c"], [root / "ISSDNative", root / "tests", runtime])
    result = subprocess.run([str(exe), str(rom)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
