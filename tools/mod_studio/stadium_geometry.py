"""Original stadium resources and the engine's coordinate projection.

Cartridge resources stay private. Reading templates is distinct from exporting
new artwork and does not grant permission to package cartridge image data.
"""
from dataclasses import dataclass
import struct
from pathlib import Path
try:
    from ..stadium_profile import validate_profile, TEMPLATES, CAMERA_FIELDS, Diagnostic
except ImportError:
    from stadium_profile import validate_profile, TEMPLATES, CAMERA_FIELDS, Diagnostic


@dataclass(frozen=True)
class CompiledGeometry:
    metatiles: dict
    world_maps: dict
    page_dimensions: tuple
    derived_fields: dict
    camera: dict
    diagnostics: tuple


def world_to_screen(u, v, camera_x, camera_y):
    return u + v - camera_x, v - camera_y


def preview_artwork_scene(rom, profile, assets=None, layer=None, region=None):
    """Render final compiled resources without making cartridge pixels exportable."""
    from .stadium_assets import SLOTS
    base = profile['base_layout']
    vram,palette = map(bytearray,read_preview_resources(rom,base))
    if assets is None:
        geometry = compile_geometry(profile,read_template(rom,base))
    else:
        if assets.diagnostics:
            raise ValueError('Correct artwork errors before rendering the scene')
        geometry = assets.geometry
        for upload in assets.tile_writes:
            start = (SLOTS[base][upload.slot][0]+upload.offset_tiles*16)*2
            vram[start:start+len(upload.data)] = upload.data
        if assets.palette is not None:
            palette[0x40:0x40+len(assets.palette)] = assets.palette
    return render_native_scene(geometry,bytes(vram),bytes(palette),layer,region)


def render_native_scene(geometry, vram, palette, layer=None, region=None):
    """Decode compiled background words and 4bpp pixels before sprites/fades.

    VRAM and palette are explicit private preview inputs, never export
    dependencies. BG1's measured 16px offset aligns it with BG2.
    """
    from PIL import Image
    if geometry.diagnostics or len(vram) != 65536 or len(palette) != 512:
        raise ValueError('Supply valid compiled maps, 64KiB VRAM and 256 native colors')
    if layer not in (None, 0, 1):
        raise ValueError('Choose background layer 0 or 1')
    pages_x,pages_y = geometry.page_dimensions
    left,top,width,height = region or (0,0,pages_x*256,pages_y*256)
    if any(type(value) is not int for value in (left,top,width,height)) or width <= 0 or height <= 0:
        raise ValueError('Preview region must have positive integer dimensions')
    colors = struct.unpack('<256H',palette)
    cache = {}
    image = Image.new('RGBA',(width,height))
    for current in ((1,0) if layer is None else (layer,)):
        definitions,world = geometry.metatiles[current],geometry.world_maps[current]
        if len(definitions) != 8192 or len(world) != 4096:
            raise ValueError('Invalid compiled native layer')
        shift = 16 if current == 0 and layer is None else 0
        for y in range((top+shift)//8*8,top+height+shift,8):
            for x in range((left+shift)//8*8,left+width+shift,8):
                if not 0 <= x < pages_x*256 or not 0 <= y < pages_y*256:
                    continue
                index = (y>>8)*(pages_x*64)+((y&224)>>2)+(x>>8)*64+((x&255)>>5)
                offset = world[index]*32+((y&31)>>3)*8+((x&31)>>3)*2
                word = struct.unpack_from('<H',definitions,offset)[0]
                if word not in cache:
                    number,bank = word&1023,(word>>10)&7
                    data = vram[0x4000+number*32:0x4020+number*32]
                    tile = Image.new('RGBA',(8,8))
                    for ty in range(8):
                        for tx in range(8):
                            value = sum(((data[ty*2+(plane&1)+(16 if plane>1 else 0)]>>(7-tx))&1)<<plane
                                        for plane in range(4))
                            color = colors[bank*16+value]
                            tile.putpixel((tx,ty),((color&31)*255//31,((color>>5)&31)*255//31,
                                ((color>>10)&31)*255//31,255 if value else 0))
                    if word&0x4000:
                        tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
                    if word&0x8000:
                        tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                    cache[word] = tile
                tile = cache[word]
                image.alpha_composite(tile,(x-shift-left,y-shift-top))
    return image


def derived_fields(length, width):
    half = width // 2
    return {
        0x12a2: length, 0x12a4: width, 0x12f2: length//2,
        0x12d6: half, 0x12d8: half, 0x12d0: half-128,
        0x12c6: half-128, 0x12c8: half-192, 0x12ca: half+128,
        0x12cc: half+192, 0x12e4: half-48, 0x12e6: half+48,
        0x12e0: width+24, 0x12e8: (-56)&65535, 0x12ea: length+56,
        0x12e2: length+128, 0x12c2: length-64, 0x12b4: length-384,
        0x12b6: length-768, 0x12c0: length-768, 0x12be: length-736,
        0x12c4: length//2+160, 0x12b8: length//2-128,
        0x12ba: length//2-256, 0x12bc: length//2+384, 0x12ce: width-32,
        0x1c80: half-56, 0x1c82: half-48, 0x1c84: half+48,
        0x1c86: half+56, 0x1c88: half, 0x1c8c: length+56, 0x1c8a: length+72,
    }


def compile_geometry(profile, template):
    """Reassemble native tile words, retaining original scenery and glyphs.

    Version 1 changes length with fixed transverse bounds. Left areas stay in
    place; right areas/goals translate by the length delta; the center translates
    by half that delta. Grass between those regions comes from the original
    grass strip between the left area and center circle. All offsets are tile
    aligned. No cartridge pixel resources are added to an exported pack.
    """
    diagnostics = validate_profile({'stadium_profile': profile}, Path('.'))
    # Resource validation belongs to the artwork compiler, not this transform.
    if isinstance(profile, dict) and 'artwork' in profile:
        clean = dict(profile)
        clean.pop('artwork')
        diagnostics = validate_profile({'stadium_profile': clean}, Path('.'))
    definitions, maps = template.get('metatiles', {}), template.get('world_maps', {})
    pages = template.get('page_dimensions', (0, 0))
    stride = template.get('stride')
    if (type(stride) is not int or not 0x80 <= stride <= 0x340 or stride % 64 or
            any(not isinstance(definitions.get(layer), bytes) or
                len(definitions[layer]) != 8192 or
                not isinstance(maps.get(layer), bytes) or len(maps[layer]) != 4096
                for layer in range(2))):
        diagnostics.append(Diagnostic('error', ('template',), 'Invalid native template resources'))
    if diagnostics:
        return CompiledGeometry({}, {}, pages, {}, {}, tuple(diagnostics))
    base = profile['base_layout']
    geometry = profile['geometry']
    length, width = geometry['length_units'], geometry['width_units']
    if (template.get('base_layout') != base or
            template.get('length_units') != TEMPLATES[base][0] or
            template.get('width_units') != width):
        return CompiledGeometry({}, {}, pages, {}, {}, (
            Diagnostic('error', ('template',), 'Template does not match the selected base layout'),))
    camera = dict(zip(CAMERA_FIELDS, TEMPLATES[base][2:]))
    camera.update(geometry.get('camera', {}))
    camera.update(initial_x=(length+width)//2+32, initial_y=384)
    derived = derived_fields(length, width)
    delta = template['length_units'] - length
    if delta == 0:
        return CompiledGeometry(dict(definitions), dict(maps), pages, derived, camera, ())
    result_definitions, result_maps = {}, {}
    for layer in range(2):
        source_definitions, source_map = definitions[layer], maps[layer]

        def sample(x, y):
            if x < 0 or y < 0 or x >= stride*4:
                raise ValueError('Geometry sample exceeds the original map')
            index = (y>>8)*stride + ((y&224)>>2) + (x>>8)*64 + ((x&255)>>5)
            if index >= 4096:
                raise ValueError('Geometry sample exceeds the original map')
            offset = source_map[index]*32 + ((y&31)>>3)*8 + ((x&31)>>3)*2
            return struct.unpack_from('<H', source_definitions, offset)[0]

        unique, encoded_definitions, encoded_map = {}, bytearray(), bytearray()
        for index in range(4096):
            x0 = ((index % stride)//64)*256 + (index&7)*32
            y0 = (index//stride)*256 + ((index&63)>>3)*32
            words = []
            for dy in range(0, 32, 8):
                for dx in range(0, 32, 8):
                    x, y = x0+dx, y0+dy
                    u = x-y+64
                    # BG1 scrolls 16 pixels beyond BG2 in both axes.
                    v = y+4-(240 if layer == 0 else 224)
                    old_pitch = -8 <= u <= template['length_units']+8 and -8 <= v <= width+8
                    net_band = abs(v-width//2) <= 80
                    old_net = net_band and (-80 <= u <= 0 or
                                            template['length_units'] <= u <= template['length_units']+80)
                    if old_pitch or old_net:
                        new_pitch = -8 <= u <= length+8 and -8 <= v <= width+8
                        new_net = net_band and (-80 <= u <= 0 or length <= u <= length+80)
                        if (new_pitch or new_net) and u <= 384:
                            source_x = x
                        elif (new_pitch or new_net) and u >= length-384:
                            source_x = x+delta
                        elif new_pitch and abs(u-length//2) <= 192:
                            source_x = x+delta//2
                        else:
                            grass_u = 512 + ((u-512) % 128)
                            source_x = grass_u+y-64
                        words.append(sample(source_x, y))
                    else:
                        offset = source_map[index]*32 + dy + dx//4
                        words.append(struct.unpack_from('<H', source_definitions, offset)[0])
            block = struct.pack('<16H', *words)
            if block not in unique:
                if len(unique) == 256:
                    return CompiledGeometry({}, {}, pages, derived, camera, (
                        Diagnostic('error', ('geometry',), 'Geometry exceeds 256 native metatiles'),))
                unique[block] = len(unique)
                encoded_definitions.extend(block)
            encoded_map.append(unique[block])
        encoded_definitions.extend(bytes(8192-len(encoded_definitions)))
        result_definitions[layer] = bytes(encoded_definitions)
        result_maps[layer] = bytes(encoded_map)
    return CompiledGeometry(result_definitions, result_maps, pages, derived, camera, ())


def _offset(address):
    return ((address >> 16) & 127) * 0x8000 + (address & 0x7fff)


def _decompress(rom, address, maximum=0x10000):
    start = _offset(address)
    if start + 2 > len(rom):
        raise ValueError('Compressed resource lies outside the cartridge')
    header = struct.unpack_from('<H', rom, start)[0]
    end = start + (header & 0x7fff)
    if end > len(rom) or end <= start + 2:
        raise ValueError('Invalid compressed resource extent')
    position = start + 2
    window = bytearray(1024)
    cursor = 0
    output = bytearray()

    def byte():
        nonlocal position
        if position >= end:
            raise ValueError('Truncated compressed command')
        value = rom[position]
        position += 1
        return value

    def emit(value):
        nonlocal cursor
        if len(output) >= maximum:
            raise ValueError('Decompressed resource exceeds its allocation')
        output.append(value)
        window[cursor] = value
        cursor = (cursor + 1) & 1023

    while position < end:
        command = byte()
        if command < 0x80:
            reference = ((((command & 3) << 8) | byte()) - 0x3df) & 1023
            for _ in range((command >> 2) + 2):
                emit(window[reference])
                reference = (reference + 1) & 1023
        elif command < 0xa0:
            for _ in range(command & 31):
                emit(byte())
        elif command < 0xc0:
            for _ in range((command & 31) + 2):
                emit(0)
                emit(byte())
        elif command < 0xe0:
            value = byte()
            for _ in range((command & 31) + 2):
                emit(value)
        else:
            count = byte() + 2 if command == 0xff else (command & 31) + 2
            for _ in range(count):
                emit(0)
    return bytes(output), bool(header & 0x8000)


def read_preview_resources(rom, base_layout):
    """Decode private day/fine template characters for background preview.

    These pixel buffers stay in memory and are never package dependencies.
    """
    if type(base_layout) is not int or not 0 <= base_layout < 8 or len(rom) < 0x200000:
        raise ValueError('Supply the original cartridge and a layout from0to7')
    word = lambda address: struct.unpack_from('<H',rom,_offset(address))[0]
    vram,palette = bytearray(65536),bytearray(512)
    descriptors = [0x828578]+[0x820000|word(table+base_layout*2)
                             for table in (0x81ef41,0x81ef51,0x81ac2f)]
    for descriptor in descriptors:
        position = _offset(descriptor)
        descriptor_type = struct.unpack_from('<H',rom,position)[0]
        if descriptor_type == 1:
            # Original animated scenery caches are WRAM resources. This static
            # background preview does not pretend to simulate their playback.
            continue
        if descriptor_type != 0:
            raise ValueError('Unexpected native graphics descriptor type')
        position += 2
        for _ in range(64):
            if position >= len(rom):
                raise ValueError('Truncated native graphics descriptor')
            if rom[position] == 255:
                break
            if position+5 > len(rom):
                raise ValueError('Truncated native graphics upload')
            destination = struct.unpack_from('<H',rom,position)[0]
            source = int.from_bytes(rom[position+2:position+5],'little')
            position += 5
            data,interleaved = _decompress(rom,source)
            if interleaved:
                if len(data)%16:
                    raise ValueError('Invalid interleaved native graphics')
                data = b''.join(bytes(value for pair in zip(data[i:i+8],data[i+8:i+16]) for value in pair)
                                for i in range(0,len(data),16))
            start = destination*2
            if start+len(data) > len(vram):
                raise ValueError('Native preview graphics exceed VRAM')
            vram[start:start+len(data)] = data
        else:
            raise ValueError('Unterminated native graphics descriptor')
    table = word(0x81ef91+base_layout*2)
    records = [(0,0x899bba),(0x1e,0x899596),
               (0x0a,0x890000|word(0x81f021+base_layout*2)),
               (4,0x890000|word(0x810000|table))]
    for selector,source in records:
        start = word(0x819176+selector)-0x2c00
        size = word(source)+1
        if start < 0 or start+size > 512:
            raise ValueError('Native preview palette exceeds CGRAM')
        position = _offset(source)+2
        palette[start:start+size] = rom[position:position+size]
    return bytes(vram),bytes(palette)


def read_template(rom, base_layout):
    if type(base_layout) is not int or not 0 <= base_layout <= 7:
        raise ValueError('Original layout must be an integer from 0 to 7')
    pointer = _offset(0x81ef61) + base_layout * 2
    if len(rom) < pointer + 2:
        raise ValueError('Cartridge does not contain the stadium descriptor table')
    descriptor = 0x820000 | struct.unpack_from('<H', rom, pointer)[0]
    position = _offset(descriptor)
    if struct.unpack_from('<H', rom, position)[0] != 1:
        raise ValueError('Expected original stadium WRAM descriptors')
    position += 2
    ram = bytearray(0x20000)
    written = bytearray(0x20000)
    while True:
        if position + 2 > len(rom):
            raise ValueError('Unterminated original stadium descriptors')
        if rom[position:position+2] == b'\xff\xff':
            break
        if position + 6 > len(rom):
            raise ValueError('Truncated original stadium descriptor')
        destination = int.from_bytes(rom[position:position+3], 'little')
        source = int.from_bytes(rom[position+3:position+6], 'little')
        position += 6
        if destination >> 16 not in (0x7e, 0x7f):
            raise ValueError('Unexpected stadium destination bank')
        address = (destination & 0xffff) + (0x10000 if destination >> 16 == 0x7f else 0)
        data, interleaved = _decompress(rom, source & 0xbfffff)
        if source & 0x400000:
            if address + len(data) * 2 > len(ram):
                raise ValueError('Stadium map exceeds WRAM')
            for index, value in enumerate(data):
                ram[address + index * 2] = value
                written[address + index * 2] = 1
        else:
            if interleaved:
                if len(data) % 16:
                    raise ValueError('Invalid interleaved tile resource size')
                data = b''.join(bytes(value for pair in zip(data[i:i+8], data[i+8:i+16])
                                      for value in pair)
                                for i in range(0, len(data), 16))
            if address + len(data) > len(ram):
                raise ValueError('Stadium resource exceeds WRAM')
            ram[address:address+len(data)] = data
            written[address:address+len(data)] = b'\x01' * len(data)
    dimensions = struct.unpack_from('<HH', rom, _offset(0x81ec47) + base_layout * 4)
    stride = struct.unpack_from('<H', rom, _offset(0x81ee71) + base_layout * 2)[0]
    return {
        'base_layout': base_layout, 'length_units': dimensions[0],
        'width_units': dimensions[1], 'stride': stride,
        'page_dimensions': (stride // 64, 4096 // stride),
        'metatiles': {layer: bytes(ram[0x18000+layer*0x2000:0x1a000+layer*0x2000])
                      for layer in range(2)},
        'world_maps': {layer: bytes(ram[0x1d000+layer*0x1000:0x1e000+layer*0x1000])
                       for layer in range(2)},
        'written_metatile_bytes': {
            layer: bytes(written[0x18000+layer*0x2000:0x1a000+layer*0x2000])
            for layer in range(2)},
    }
