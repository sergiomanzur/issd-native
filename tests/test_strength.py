from pathlib import Path
import subprocess
from test_config_persistence import compile_c

def test_applied_rosters_and_formations_drive_strength(tmp_path):
    root=Path(__file__).resolve().parents[1]
    exe=tmp_path/'strength.exe'
    compile_c(exe,root,[root/'tests/test_strength.c',root/'ISSDNative/issd_strength.c'],[root/'ISSDNative'])
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
