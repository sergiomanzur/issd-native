from pathlib import Path
import subprocess
from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]

def test_original_penalty_and_match_options(tmp_path):
    exe = tmp_path / 'options.exe'
    runtime = ROOT / 'deps/snesrecomp/runner/src/snes'
    compile_c(exe, ROOT, [ROOT / 'tests/test_match_options.c',
        ROOT / 'ISSDNative/issd_match_options.c', runtime / 'interp816.c'],
        [ROOT / 'ISSDNative', runtime])
    result = subprocess.run([str(exe), str(ROOT / 'International Superstar Soccer Deluxe (USA).sfc')],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
