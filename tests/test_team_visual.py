import subprocess
from pathlib import Path
from test_config_persistence import compile_c

def test_native_team_visual_identity(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / 'team_visual.exe'
    compile_c(exe, repo, [repo / 'tests/test_team_visual.c', repo / 'ISSDNative/issd_team_visual.c'], [repo / 'ISSDNative', repo / 'deps/snesrecomp/runner/src'])
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
