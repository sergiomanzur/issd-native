import struct
from tools.mod_studio.stadium_geometry import CompiledGeometry


def fixture():
    definitions = bytearray(8192)
    for index in range(16):
        struct.pack_into('<H',definitions,index*2,0x0801)
    geometry = CompiledGeometry({0:bytes(8192),1:bytes(definitions)},
        {0:bytes(4096),1:bytes(4096)},(11,1),{}, {},())
    vram = bytearray(65536)
    for y in range(8):
        vram[0x4020+y*2] = 0x80
    palette = bytearray(512)
    struct.pack_into('<H',palette,0x42,31)
    return geometry,bytes(vram),bytes(palette)


def test_preview_reads_compiled_words_bank_and_binary_transparency():
    from tools.mod_studio.stadium_geometry import render_native_scene
    geometry,vram,palette = fixture()
    image = render_native_scene(geometry,vram,palette,layer=1,region=(0,0,32,32))
    assert image.size == (32,32)
    assert image.getpixel((0,0)) == (255,0,0,255)
    assert image.getpixel((1,0))[3] == 0
    assert image.getpixel((8,0)) == (255,0,0,255)


def test_preview_respects_native_horizontal_flip():
    from tools.mod_studio.stadium_geometry import render_native_scene
    geometry,vram,palette = fixture()
    definitions = bytearray(geometry.metatiles[1])
    struct.pack_into('<H',definitions,0,0x4801)
    geometry.metatiles[1] = bytes(definitions)
    image = render_native_scene(geometry,vram,palette,layer=1,region=(0,0,8,8))
    assert image.getpixel((0,0))[3] == 0
    assert image.getpixel((7,0)) == (255,0,0,255)


def test_artwork_preview_uses_final_maps_and_owned_upload_offsets(monkeypatch):
    from types import SimpleNamespace
    from tools.mod_studio import stadium_geometry as module
    from tools.mod_studio.stadium_assets import TileWrite
    geometry,vram,palette = fixture()
    monkeypatch.setattr(module,'read_preview_resources',lambda rom,base:(vram,palette))
    authored = bytearray(32)
    authored[0] = 0x80
    definitions = bytearray(8192)
    struct.pack_into('<H',definitions,0,0x0a00)  # owned character512, bank2
    geometry.metatiles[1] = bytes(definitions)
    local_palette = bytearray(96)
    struct.pack_into('<H',local_palette,2,0x3e0)
    assets = SimpleNamespace(geometry=geometry,tile_writes=(TileWrite(0,0,bytes(authored)),),
                             palette=bytes(local_palette),diagnostics=())
    image = module.preview_artwork_scene(b'',{'base_layout':0},assets,layer=1,region=(0,0,8,8))
    assert image.getpixel((0,0)) == (0,255,0,255)
    assert image.getpixel((1,0))[3] == 0
