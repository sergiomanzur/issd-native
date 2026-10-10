from pathlib import Path
import subprocess
from test_config_persistence import compile_c


def test_native_paths_require_existing_resources_inside_physical_pack_root(tmp_path):
    root = Path(__file__).resolve().parents[1]
    source = root / 'ISSDNative/issd_asset_path.c'
    assert source.exists(), 'Native physical asset resolver missing'
    executable = tmp_path / 'path.exe'
    compile_c(executable, root, [root / 'tests/test_stadium_asset_path.c', source],
              [root / 'ISSDNative'])
    result = subprocess.run([str(executable), str(tmp_path)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
