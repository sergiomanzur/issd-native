import json
from pathlib import Path
import struct
from PIL import Image
from test_stadium_assets import manifest


def importer():
    from tools.mod_studio import stadium_assets
    assert hasattr(stadium_assets, 'import_image'), 'Native image importer missing'
    return stadium_assets.import_image


def test_image_placement_does_not_change_shared_metatile_neighbors(tmp_path):
    value = manifest(tmp_path)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGB',(16,8),'red').save(tmp_path/'image.png')
    result = importer()(tmp_path/'image.png',tmp_path/'stadium.json',tmp_path/'new',0,
                        placement={'layer':1,'x':24,'y':0})
    assert not [d for d in result.diagnostics if d.severity == 'error']
    geometry = result.compiled.geometry
    world = geometry.world_maps[1]
    definitions = geometry.metatiles[1]
    read = lambda cell,offset: struct.unpack_from('<H',definitions,world[cell]*32+offset)[0]
    assert read(0,6) == 0x0a00 and read(1,0) == 0x0a01
    assert read(0,0) == 0 and read(1,2) == 0
    assert read(2,0) == 0 and geometry.world_maps[0] == bytes(4096)
    assert (tmp_path/'defs.bin').read_bytes() == bytes(8192)


def test_hd_image_import_authors_native_fallback_and_automatic_local_keys(tmp_path):
    value = manifest(tmp_path)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGBA',(16,16),'red').save(tmp_path/'image.png')
    result = importer()(tmp_path/'image.png',tmp_path/'stadium.json',tmp_path/'new',0,hd_scale=2)
    assert not [d for d in result.diagnostics if d.severity == 'error']
    assert len(result.compiled.tile_writes[0].data) == 32
    assert len(result.compiled.hd) == 1
    path = next(iter(result.compiled.hd.values()))
    with Image.open(path) as bitmap:
        assert bitmap.size == (16,16)
        assert bitmap.convert('RGB').getpixel((3,3)) == (255,0,0)


def test_import_cannot_recolor_another_authored_upload_bank(tmp_path):
    value = manifest(tmp_path)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGBA',(16,16),'red').save(tmp_path/'red.png')
    first = importer()(tmp_path/'red.png',tmp_path/'stadium.json',tmp_path/'first',0,hd_scale=2)
    assert not first.diagnostics
    Image.new('RGB',(8,8),'blue').save(tmp_path/'blue.png')
    second = importer()(tmp_path/'blue.png',tmp_path/'first/stadium.json',tmp_path/'second',1)
    assert any(d.severity == 'error' and 'bank' in d.message.lower() for d in second.diagnostics)
    assert not (tmp_path/'second').exists()


def test_replacement_cannot_leave_untracked_old_placement(tmp_path):
    value = manifest(tmp_path)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGB',(8,8),'red').save(tmp_path/'image.png')
    first = importer()(tmp_path/'image.png',tmp_path/'stadium.json',tmp_path/'first',0,
                       placement={'layer':1,'x':0,'y':0})
    assert not first.diagnostics
    result = importer()(tmp_path/'image.png',tmp_path/'first/stadium.json',tmp_path/'moved',0,
                        placement={'layer':1,'x':32,'y':0})
    assert any(d.severity == 'error' and 'placement' in d.message.lower() for d in result.diagnostics)
    assert not (tmp_path/'moved').exists()


def test_import_produces_planar_native_art_and_retains_transparency(tmp_path):
    source = tmp_path / 'source'
    source.mkdir()
    value = manifest(source)
    (source / 'stadium.json').write_text(json.dumps(value))
    image = Image.new('RGBA', (8, 8), (255, 0, 0, 255))
    image.putpixel((0, 0), (0, 255, 0, 0))
    image.save(tmp_path / 'image.png')
    destination = tmp_path / 'imported'
    result = importer()(tmp_path / 'image.png', source / 'stadium.json', destination, 0)
    assert not [d for d in result.diagnostics if d.severity == 'error']
    assert len(result.compiled.tile_writes[0].data) == 32
    assert result.compiled.preview.getpixel((0, 0))[3] == 0
    assert result.compiled.preview.getpixel((1, 0)) == (255, 0, 0, 255)
    assert result.compiled.palette[0:4] == b'\x00\x00\x1f\x00'
    assert (source / 'tiles.bin').read_bytes() == bytes(32)
    exported = json.loads((destination / 'stadium.json').read_text())
    assert exported == result.manifest
    assert (destination / exported['tiles'][0]['file']).read_bytes() != bytes(32)


def test_import_overflow_and_bad_dimensions_leave_no_output(tmp_path):
    value = manifest(tmp_path)
    (tmp_path / 'stadium.json').write_text(json.dumps(value))
    for size in ((9, 8), (8*96, 8)):
        Image.new('RGB', size).save(tmp_path / 'image.png')
        destination = tmp_path / 'bad'
        result = importer()(tmp_path / 'image.png', tmp_path / 'stadium.json', destination, 0)
        assert any(d.severity == 'error' for d in result.diagnostics)
        assert not destination.exists()


def test_import_reports_color_loss_and_preserves_other_palette_banks(tmp_path):
    value = manifest(tmp_path)
    # Each bank's index zero stays transparent.
    palette = bytes(32) + bytes(2) + b'\x01\x00' * 15 + bytes(32)
    (tmp_path / 'palette.bin').write_bytes(palette)
    (tmp_path / 'stadium.json').write_text(json.dumps(value))
    image = Image.new('RGBA', (8, 8))
    for i in range(64):
        image.putpixel((i % 8, i // 8), (i*4, 128, 50, 255))
    image.save(tmp_path / 'image.png')
    result = importer()(tmp_path / 'image.png', tmp_path / 'stadium.json', tmp_path / 'new', 0)
    assert any(d.severity == 'warning' and 'quantization' in d.message.lower()
               for d in result.diagnostics)
    assert result.compiled.palette[32:] == palette[32:]


def test_import_requires_palette_to_preserve_unedited_banks(tmp_path):
    value = manifest(tmp_path)
    value.pop('palette')
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGB',(8,8),'red').save(tmp_path/'image.png')
    destination = tmp_path/'new'
    result = importer()(tmp_path/'image.png',tmp_path/'stadium.json',destination,0)
    assert any(d.severity == 'error' and 'palette' in d.message.lower() for d in result.diagnostics)
    assert not destination.exists()


def test_import_rebinds_referenced_native_tiles_to_selected_palette_bank(tmp_path):
    value = manifest(tmp_path)
    definitions = bytearray(8192)
    struct.pack_into('<H',definitions,0,0x0a00) # character512, native palette2
    (tmp_path/'defs.bin').write_bytes(definitions)
    (tmp_path/'stadium.json').write_text(json.dumps(value))
    Image.new('RGB',(8,8),'red').save(tmp_path/'image.png')
    result = importer()(tmp_path/'image.png',tmp_path/'stadium.json',tmp_path/'new',0,palette_bank=1)
    assert not [d for d in result.diagnostics if d.severity == 'error']
    assert struct.unpack_from('<H',result.compiled.geometry.metatiles[0],0)[0] == 0x0e00
    assert result.compiled.preview.getpixel((0,0)) == (255,0,0,255)
    assert (tmp_path/'defs.bin').read_bytes() == definitions
