from pathlib import Path
import subprocess
from test_config_persistence import compile_c


def test_geometry_extension_preserves_legacy_and_changes_geometry_context(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    assert 'issd_save_set_context_extra' in (repo / 'ISSDNative/issd_save.h').read_text(), 'Geometry context extension missing'
    executable = tmp_path / 'context.exe'
    compile_c(executable, repo, [repo / 'tests/test_stadium_save_context.c',
              repo / 'deps/snesrecomp/runner/src/sha256.c'],
              [repo / 'ISSDNative', repo / 'deps/snesrecomp/runner/src'])
    result = subprocess.run([str(executable), str(tmp_path)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
