from pathlib import Path
import subprocess
import pytest
from test_config_persistence import compile_c


def test_original_interpreter_profile_field_goal_penalty_and_crossbar_boundaries(tmp_path):
    root=Path(__file__).resolve().parents[1]
    rom=root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom.exists():pytest.skip('Requires private cartridge')
    executable=tmp_path/'predicates.exe'
    compile_c(executable,root,[root/'tests/test_stadium_predicates.c',root/'ISSDNative/issd_stadium_rom.c',
        root/'deps/snesrecomp/runner/src/snes/interp816.c'],
        [root/'ISSDNative',root/'deps/snesrecomp/runner/src/snes'])
    result=subprocess.run([str(executable),str(rom)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
