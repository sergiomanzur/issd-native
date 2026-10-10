from pathlib import Path
import subprocess
from test_config_persistence import compile_c


def test_recorded_replay_history_survives_load_and_rejects_stale_ring(tmp_path):
    root=Path(__file__).resolve().parents[1];exe=tmp_path/'history.exe'
    compile_c(exe,root,[root/p for p in (
        'tests/test_replay_history.c','ISSDNative/issd_widescreen.c',
        'ISSDNative/issd_animation.c','ISSDNative/issd_pose_history.c')],
        [root/'ISSDNative',root/'deps/snesrecomp/runner/src'])
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
