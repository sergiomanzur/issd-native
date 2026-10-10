from pathlib import Path
import subprocess
import json
from PIL import Image
from test_config_persistence import compile_c
from test_stadium_assets import manifest
from tools.mod_studio.stadium_assets import import_image


def test_automatic_import_key_matches_production_compositor(tmp_path):
    root = Path(__file__).resolve().parents[1]
    value = manifest(tmp_path)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGBA',(16,16),(123,45,67,255)).save(tmp_path/'source.png')
    imported = import_image(tmp_path/'source.png',tmp_path/'stadium.json',tmp_path/'new',0,hd_scale=2)
    assert not [d for d in imported.diagnostics if d.severity=='error']
    fixture = tmp_path/'identity.bin'
    fixture.write_bytes(imported.compiled.tile_writes[0].data+imported.compiled.palette[:32])
    executable = tmp_path/'identity.exe'
    compile_c(executable,root,[root/'tests/test_stadium_hd_import.c'],
        [root/'ISSDNative',root/'deps/snesrecomp/runner/src',root/'deps/snesrecomp/runner/src/snes'])
    key = next(iter(imported.compiled.hd))
    result = subprocess.run([str(executable),str(fixture),f'{key:016x}'],capture_output=True,text=True)
    assert result.returncode == 0,result.stdout+result.stderr
