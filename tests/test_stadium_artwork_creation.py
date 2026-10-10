from pathlib import Path
import json
import pytest
from tools.mod_studio.stadium_geometry import compile_geometry,read_template


def test_new_artwork_contains_compiled_geometry_and_no_cartridge_pixels(tmp_path):
    from tools.mod_studio import stadium_assets
    assert hasattr(stadium_assets,'create_artwork'),'GUI artwork initializer missing'
    root = Path(__file__).resolve().parents[1]
    rom_path = root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom_path.exists():
        pytest.skip('Requires private cartridge')
    rom = rom_path.read_bytes()
    profile = {'version':1,'base_layout':0,'geometry':{'length_units':1728,'width_units':576}}
    destination = tmp_path/'art'
    result = stadium_assets.create_artwork(profile,rom,destination)
    assert not result.diagnostics
    value = json.loads((destination/'stadium.json').read_text())
    assert value['tiles'] == [] and value['hd'] == []
    expected = compile_geometry(profile,read_template(rom,0))
    assert result.compiled.geometry.world_maps == expected.world_maps
    assert result.compiled.geometry.metatiles == expected.metatiles
    assert len(result.compiled.palette) == 96
    assert all(result.compiled.palette[index:index+2] == bytes(2) for index in (0,32,64))
    assert all(path.suffix in ('.json','.bin') and path.stat().st_size <= 8192 for path in destination.iterdir())
    assert rom_path.read_bytes() == rom


def test_geometry_edits_require_artwork_map_recompilation(tmp_path):
    from tools.mod_studio.stadium_assets import create_artwork
    from tools.stadium_profile import validate_profile
    root = Path(__file__).resolve().parents[1]
    rom_path = root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom_path.exists():
        pytest.skip('Requires private cartridge')
    profile = {'version':1,'base_layout':0,'geometry':{'length_units':1728,'width_units':576}}
    assert not create_artwork(profile,rom_path.read_bytes(),tmp_path/'art').diagnostics
    profile['artwork'] = 'art/stadium.json'
    assert not validate_profile({'stadium_profile':profile},tmp_path)
    profile['geometry']['length_units'] = 1664
    assert any('recompile' in d.message.lower() for d in validate_profile({'stadium_profile':profile},tmp_path))


def test_recompile_retains_imported_pixels_palette_and_source(tmp_path):
    from tools.mod_studio import stadium_assets
    from PIL import Image
    assert hasattr(stadium_assets,'recompile_artwork'),'Artwork geometry refresh missing'
    root = Path(__file__).resolve().parents[1]
    rom_path = root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom_path.exists():
        pytest.skip('Requires private cartridge')
    rom = rom_path.read_bytes()
    profile = {'version':1,'base_layout':0,'geometry':{'length_units':1728,'width_units':576}}
    assert not stadium_assets.create_artwork(profile,rom,tmp_path/'initial').diagnostics
    Image.new('RGB',(8,8),'red').save(tmp_path/'red.png')
    original = stadium_assets.import_image(tmp_path/'red.png',tmp_path/'initial/stadium.json',tmp_path/'art',0,palette_bank=1)
    source = (tmp_path/'art/stadium.json').read_bytes()
    profile['geometry']['length_units'] = 1664
    result = stadium_assets.recompile_artwork(profile,rom,tmp_path/'art/stadium.json',tmp_path/'new')
    assert not result.diagnostics
    assert result.manifest['geometry']['length_units'] == 1664
    assert result.compiled.tile_writes[0].data == original.compiled.tile_writes[0].data
    assert result.compiled.palette == original.compiled.palette
    assert result.compiled.geometry.world_maps != original.compiled.geometry.world_maps
    assert (tmp_path/'art/stadium.json').read_bytes() == source
