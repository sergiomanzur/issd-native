import importlib.util
from pathlib import Path
import sys

PATH = Path(__file__).resolve().parents[1] / 'tools/mod_studio/stadium_assets.py'


def compiler():
    assert PATH.exists(), 'Native stadium asset compiler missing'
    from tools.mod_studio.stadium_assets import compile_manifest
    return compile_manifest


def manifest(directory):
    (directory / 'defs.bin').write_bytes(bytes(8192))
    (directory / 'map.bin').write_bytes(bytes(4096))
    (directory / 'tiles.bin').write_bytes(bytes(32))
    (directory / 'palette.bin').write_bytes(bytes(96))
    return {'version': 1, 'base_layout': 0,
            'allocation_profile': 'original-scenery-v1',
            'layers': [{'layer': layer, 'pages_x': 11, 'pages_y': 5,
                        'metatiles': 'defs.bin', 'world_map': 'map.bin'} for layer in range(2)],
            'tiles': [{'slot': 0, 'offset_tiles': 0, 'tile_count': 1, 'file': 'tiles.bin'}],
            'palette': 'palette.bin'}


def test_exact_resources_compile_without_using_arbitrary_vram(tmp_path):
    value = manifest(tmp_path)
    compiled = compiler()(value, tmp_path)
    assert not compiled.diagnostics
    assert compiled.tile_writes[0].slot == 0
    assert compiled.tile_writes[0].offset_tiles == 0
    assert compiled.tile_writes[0].data == bytes(32)
    assert len(compiled.dependencies) == 4


def test_slot_overflow_and_overlapping_writes_are_rejected(tmp_path):
    value = manifest(tmp_path)
    value['tiles'][0]['offset_tiles'] = 95
    assert compiler()(value, tmp_path).diagnostics
    value['tiles'][0]['offset_tiles'] = 0
    value['tiles'].append(dict(value['tiles'][0]))
    assert compiler()(value, tmp_path).diagnostics


def test_palette_cannot_touch_unowned_banks_or_set_bgr_bit15(tmp_path):
    value = manifest(tmp_path)
    (tmp_path / 'palette.bin').write_bytes(bytes(512))
    assert compiler()(value, tmp_path).diagnostics
    (tmp_path / 'palette.bin').write_bytes(b'\x00\x80' + bytes(94))
    assert compiler()(value, tmp_path).diagnostics


def test_boolean_counts_bad_layers_and_escaped_files_are_rejected(tmp_path):
    for field, bad in (('tile_count', True), ('slot', 8), ('file', '../outside.bin')):
        value = manifest(tmp_path)
        value['tiles'][0][field] = bad
        assert compiler()(value, tmp_path).diagnostics
    value = manifest(tmp_path)
    value['layers'][1]['layer'] = 0
    assert compiler()(value, tmp_path).diagnostics


def test_artwork_clone_has_independent_manifest_and_native_files(tmp_path):
    compiler()
    import json
    from tools.mod_studio.stadium_assets import clone_artwork
    value = manifest(tmp_path)
    (tmp_path / 'stadium.json').write_text(json.dumps(value))
    copied = clone_artwork('stadium.json', tmp_path, 9)
    assert copied != 'stadium.json'
    clone = tmp_path / copied
    assert json.loads(clone.read_text()) == value
    (clone.parent / 'tiles.bin').write_bytes(b'\xff' * 32)
    assert (tmp_path / 'tiles.bin').read_bytes() == bytes(32)
