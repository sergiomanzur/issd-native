import subprocess
import json
from pathlib import Path
from test_config_persistence import compile_c


def test_profile_registry_is_atomic_and_ordered(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    assert (repo / 'ISSDNative/issd_stadium.c').exists(), 'Native registry missing'
    executable = tmp_path / 'stadium_profile.exe'
    from test_stadium_assets import manifest
    art = tmp_path / 'art'
    art.mkdir()
    (art / 'stadium.json').write_text(json.dumps(manifest(art)))
    compile_c(executable, repo, [repo / 'tests/test_stadium_profile.c',
              repo / 'ISSDNative/issd_mod.c', repo / 'ISSDNative/issd_stadium.c',
              repo / 'ISSDNative/issd_asset_path.c',
              repo / 'ISSDNative/issd_stadium_assets.c',
              repo / 'deps/snesrecomp/runner/src/sha256.c'],
              [repo / 'ISSDNative', repo / 'deps/snesrecomp/runner/src'])
    result = subprocess.run([str(executable), str(tmp_path)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'tests passed' in result.stdout
