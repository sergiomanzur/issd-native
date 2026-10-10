from pathlib import Path
import json
import struct
import subprocess
from PIL import Image
from test_config_persistence import compile_c
from test_stadium_assets import manifest


def test_native_assets_validate_and_compile_the_same_resource_contract(tmp_path):
    root = Path(__file__).resolve().parents[1]
    source = root / 'ISSDNative/issd_stadium_assets.c'
    assert source.exists(), 'Native manifest compiler missing'
    executable = tmp_path / 'assets.exe'
    compile_c(executable, root, [root / 'tests/test_stadium_assets_native.c', source,
              root / 'ISSDNative/issd_asset_path.c',
              root / 'deps/snesrecomp/runner/src/sha256.c'],
              [root / 'ISSDNative', root / 'deps/snesrecomp/runner/src'])
    value = manifest(tmp_path)
    path = tmp_path / 'stadium.json'
    for valid, change in ((True, {}), (False, {'version': 2}),
                           (False, {'base_layout': True}),
                           (False, {'palette': '../outside.bin'})):
        data = dict(value, **change)
        path.write_text(json.dumps(data))
        result = subprocess.run([str(executable), str(path), str(int(valid))],
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
    for field, bad in (('slot', 7), ('offset_tiles', 95), ('tile_count', True)):
        data = manifest(tmp_path)
        data['tiles'][0][field] = bad
        path.write_text(json.dumps(data))
        result = subprocess.run([str(executable), str(path), '0'], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
    from tools.mod_studio.stadium_assets import compile_manifest
    for mode, valid in (('RGB', False), ('RGBA', True)):
        Image.new(mode, (8, 8)).save(tmp_path / 'local.bmp')
        data = manifest(tmp_path)
        data['hd'] = [{'key': '0000000000000001', 'file': 'local.bmp'}]
        path.write_text(json.dumps(data))
        assert (not compile_manifest(data, tmp_path).diagnostics) == valid
        result = subprocess.run([str(executable), str(path), str(int(valid))],
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
