"""Bounded native stadium resources and independent artwork cloning."""
from dataclasses import dataclass
import copy
from pathlib import Path
import json
import os
import shutil
import struct
import tempfile
import uuid

from PIL import Image
try:
    from ..stadium_profile import Diagnostic, resolve_asset, TEMPLATES
except ImportError:
    from stadium_profile import Diagnostic, resolve_asset, TEMPLATES
from .stadium_geometry import CompiledGeometry


# VRAM word destinations and measured 4bpp tile capacities, in loader order.
SLOTS = (
    ((0x4000,95),(0x45f0,11),(0x46a0,22)),
    ((0x4000,31),(0x4200,32),(0x4400,27),(0x4600,32)),
    ((0x4000,48),(0x4300,27)),
    ((0x41d0,19),(0x4000,45),(0x4300,32),(0x4500,22)),
    ((0x4000,31),(0x4200,27),(0x4400,32),(0x4600,22),(0x4750,23)),
    ((0x4000,68),(0x4750,23),(0x4500,48),(0x48c0,3)),
    ((0x4100,64),(0x4000,46),(0x4500,25),(0x48c0,3),(0x4750,23)),
    ((0x4000,16),(0x4600,22)),
)
PAGES_X = (11,11,12,12,12,11,11,13)


@dataclass(frozen=True)
class TileWrite:
    slot: int
    offset_tiles: int
    data: bytes


@dataclass(frozen=True)
class CompiledAssets:
    geometry: CompiledGeometry
    tile_writes: tuple
    palette: bytes | None
    hd: dict
    dependencies: tuple
    preview: Image.Image
    diagnostics: tuple


@dataclass(frozen=True)
class ImportResult:
    manifest: dict
    compiled: CompiledAssets
    diagnostics: tuple


def _tile_pixels(data):
    for y in range(8):
        for x in range(8):
            bit = 7 - x
            yield sum(((data[y*2 + (plane & 1) + (16 if plane > 1 else 0)]
                        >> bit) & 1) << plane for plane in range(4))


def compile_manifest(manifest, pack_dir):
    errors, dependencies, layers, writes, hd = [], set(), {}, [], {}
    palette = None
    dimensions = (0, 0)

    def error(path, message):
        errors.append(Diagnostic('error', tuple(path), message))

    def read(name, path, size=None):
        try:
            filename = resolve_asset(pack_dir, name)
            if size is not None and filename.stat().st_size != size:
                raise ValueError(f'Resource must contain exactly {size} bytes')
            dependencies.add(filename)
            return filename.read_bytes()
        except (ValueError, OSError) as exc:
            error(path, str(exc))
            return None

    if not isinstance(manifest, dict):
        error((), 'Artwork manifest must be an object')
        manifest = {}
    if type(manifest.get('version')) is not int or manifest.get('version') != 1:
        error(('version',), 'Only artwork version 1 is supported')
    base = manifest.get('base_layout')
    valid_base = type(base) is int and 0 <= base <= 7
    if not valid_base:
        error(('base_layout',), 'Choose an original layout from 0 to 7')
        base = 0
    if manifest.get('allocation_profile') != 'original-scenery-v1':
        error(('allocation_profile',), 'Use the measured original-scenery-v1 allocation')
    if 'geometry' in manifest:
        geometry = manifest['geometry']
        if (not isinstance(geometry,dict) or type(geometry.get('length_units')) is not int or
            type(geometry.get('width_units')) is not int or not valid_base or
            not 1536 <= geometry['length_units'] <= TEMPLATES[base][0] or geometry['length_units']%32 or
            geometry['width_units'] != TEMPLATES[base][1]):
            error(('geometry',),'Artwork map geometry must use the validated template envelope')
    records = manifest.get('layers')
    if not isinstance(records, list):
        error(('layers',), 'Supply both native layer records')
        records = []
    for index, record in enumerate(records):
        path = ('layers', index)
        if not isinstance(record, dict):
            error(path, 'Layer must be an object')
            continue
        layer = record.get('layer')
        if type(layer) is not int or layer not in (0, 1) or layer in layers:
            error(path + ('layer',), 'Each native layer 0 and 1 must appear once')
            continue
        pages_x, pages_y = record.get('pages_x'), record.get('pages_y')
        if (type(pages_x) is not int or pages_x != PAGES_X[base] or
                type(pages_y) is not int or not 1 <= pages_y <= 64 // PAGES_X[base]):
            error(path, 'Page dimensions exceed the original template map envelope')
        else:
            if dimensions != (0, 0) and dimensions != (pages_x, pages_y):
                error(path, 'Both native layers must use the same page dimensions')
            dimensions = pages_x, pages_y
        definitions = read(record.get('metatiles'), path + ('metatiles',), 8192)
        world_map = read(record.get('world_map'), path + ('world_map',), 4096)
        layers[layer] = (definitions, world_map)
    if set(layers) != {0, 1}:
        error(('layers',), 'Both native layer records are required')
    records = manifest.get('tiles', [])
    if not isinstance(records, list):
        error(('tiles',), 'Tile uploads must be an array')
        records = []
    used_words = set()
    for index, record in enumerate(records):
        path = ('tiles', index)
        if not isinstance(record, dict):
            error(path, 'Tile upload must be an object')
            continue
        slot, offset, count = (record.get(key) for key in ('slot', 'offset_tiles', 'tile_count'))
        if (any(type(value) is not int for value in (slot, offset, count)) or
                not 0 <= slot < len(SLOTS[base])):
            error(path, 'Use integer slot, offset and tile count values from the template')
            continue
        destination, capacity = SLOTS[base][slot]
        if 'palette_bank' in record and (type(record['palette_bank']) is not int or
                not 0 <= record['palette_bank'] < (2 if base in (2,6) else 3)):
            error(path+('palette_bank',),'Choose a palette bank owned by this stadium')
            continue
        if offset < 0 or count < 1 or offset + count > capacity:
            error(path, f'This slot holds {capacity} tiles')
            continue
        placement = record.get('placement')
        if placement is not None:
            width,height = record.get('image_width'),record.get('image_height')
            if (not isinstance(placement,dict) or any(type(placement.get(key)) is not int
                    for key in ('layer','x','y')) or placement.get('layer') not in (0,1) or
                    any(type(value) is not int or value <= 0 or value%8 for value in (width,height)) or
                    width*height//64 != count or placement['x'] < 0 or placement['y'] < 0 or
                    placement['x']%8 or placement['y']%8 or
                    placement['x']+width > dimensions[0]*256 or placement['y']+height > dimensions[1]*256 or
                    'palette_bank' not in record):
                error(path+('placement',),'Use an image-sized rectangle on the8px background grid with a palette bank')
                continue
        occupied = set(range(destination + offset*16, destination + (offset+count)*16))
        if occupied & used_words:
            error(path, 'Tile writes overlap another upload, including aliased slots')
            continue
        data = read(record.get('file'), path + ('file',), count * 32)
        if data is not None:
            used_words.update(occupied)
            writes.append(TileWrite(slot, offset, data))
    if 'palette' in manifest:
        palette = read(manifest['palette'], ('palette',), 64 if base in (2, 6) else 96)
        if palette is not None:
            colors = struct.unpack('<' + 'H' * (len(palette)//2), palette)
            if any(color & 0x8000 for color in colors):
                error(('palette',), 'BGR555 colors must not set bit 15')
            if any(colors[index] for index in range(0, len(colors), 16)):
                error(('palette',), 'Each palette bank reserves index 0 for transparency')
    records = manifest.get('hd', [])
    if not isinstance(records, list):
        error(('hd',), 'HD replacements must be an array')
        records = []
    if len(records) > 512:
        error(('hd',), 'A stadium supports at most 512 local HD replacements')
        records = records[:512]
    for index, record in enumerate(records):
        path = ('hd', index)
        if not isinstance(record, dict):
            error(path, 'HD replacement must be an object')
            continue
        key = record.get('key')
        if (not isinstance(key, str) or len(key) != 16 or
                any(character not in '0123456789abcdefABCDEF' for character in key)):
            error(path + ('key',), 'Use the full 16-digit native tile key')
            continue
        try:
            filename = resolve_asset(pack_dir, record.get('file'))
            if filename.suffix.lower() != '.bmp':
                raise ValueError('Runtime HD replacements must be BMP files')
            with filename.open('rb') as source:
                header = source.read(54)
            if (len(header) != 54 or header[:2] != b'BM' or
                    struct.unpack_from('<H', header, 28)[0] != 32 or
                    struct.unpack_from('<I', header, 30)[0] != 0):
                raise ValueError('Runtime HD replacements must be uncompressed 32-bit BMP files')
            with Image.open(filename) as bitmap:
                if bitmap.width != bitmap.height or bitmap.width % 8 or bitmap.width > 512:
                    raise ValueError('HD tile size must be square, a multiple of 8, and at most 512')
                bitmap.load()
            numeric = int(key, 16)
            if numeric in hd:
                raise ValueError('Duplicate HD tile key')
            hd[numeric] = filename
            dependencies.add(filename)
        except (ValueError, OSError) as exc:
            error(path, str(exc))
    # Native tile atlas: shows decoded uploaded pixels, not the original PNG.
    count = sum(len(write.data)//32 for write in writes)
    preview = Image.new('RGBA', (128, max(8, ((count+15)//16)*8)))
    colors = struct.unpack('<' + 'H'*(len(palette)//2), palette) if palette else [0]*16
    tile_banks = {}
    for definitions, _ in layers.values():
        if definitions is not None:
            for word in struct.unpack('<4096H',definitions):
                bank = ((word>>10)&7)-2
                if 0 <= bank < len(colors)//16:
                    tile_banks.setdefault(word&1023,bank)
    tile_index = 0
    for write in writes:
        for position in range(0, len(write.data), 32):
            character = SLOTS[base][write.slot][0]//16-512+write.offset_tiles+position//32
            bank = tile_banks.get(character,0)
            for pixel, value in enumerate(_tile_pixels(write.data[position:position+32])):
                color = colors[bank*16+value]
                rgba = ((color & 31)*255//31, ((color >> 5)&31)*255//31,
                        ((color >> 10)&31)*255//31, 255 if value else 0)
                preview.putpixel(((tile_index % 16)*8 + pixel % 8,
                                  (tile_index // 16)*8 + pixel // 8), rgba)
            tile_index += 1
    geometry = CompiledGeometry(
        {layer: pair[0] for layer, pair in layers.items() if pair[0] is not None},
        {layer: pair[1] for layer, pair in layers.items() if pair[1] is not None},
        dimensions, {}, {}, tuple(errors))
    return CompiledAssets(geometry, tuple(writes), palette, hd,
                          tuple(sorted(dependencies)), preview, tuple(errors))


def clone_artwork(name, pack_dir, stadium_id):
    if pack_dir is None:
        raise ValueError('Save the pack before duplicating its artwork')
    root = Path(pack_dir).resolve()
    source = resolve_asset(root, name)
    manifest = json.loads(source.read_text(encoding='utf-8'))
    compiled = compile_manifest(manifest, source.parent)
    if compiled.diagnostics:
        raise ValueError('Fix artwork validation errors before duplicating it')
    destination = root / f'stadium-{stadium_id}-art'
    suffix = 1
    while destination.exists():
        destination = root / f'stadium-{stadium_id}-art-{suffix}'
        suffix += 1
    stage = Path(tempfile.mkdtemp(prefix='.studio-art-', dir=root))
    try:
        for dependency in compiled.dependencies:
            target = stage / dependency.relative_to(source.parent)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(dependency, target)
        (stage / 'stadium.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        os.replace(stage, destination)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    return (destination / 'stadium.json').relative_to(root).as_posix()


def create_artwork(profile, rom, destination):
    """Initialize editable generated map references and native palette values.

    The generated package contains no cartridge file or extracted tile pixels;
    unchanged characters remain references to the selected runtime template.
    """
    from .stadium_geometry import read_template,compile_geometry,_offset
    manifest,compiled,stage = {},None,None
    try:
        geometry = compile_geometry(profile,read_template(rom,profile['base_layout']))
        if geometry.diagnostics:
            return ImportResult(manifest,None,geometry.diagnostics)
        base = profile['base_layout']
        word = lambda address: struct.unpack_from('<H',rom,_offset(address))[0]
        table = word(0x81ef91+base*2)
        source = 0x890000+word(0x810000+table)
        size = 64 if base in (2,6) else 96
        if word(source)+1 != size:
            raise ValueError('Template palette exceeds its measured allocation')
        palette = bytearray(rom[_offset(source)+2:_offset(source)+2+size])
        for position in range(0,size,32):
            palette[position:position+2] = bytes(2)
        destination = Path(destination).resolve()
        if destination.exists():
            raise ValueError('Artwork destination already exists')
        destination.parent.mkdir(parents=True,exist_ok=True)
        stage = Path(tempfile.mkdtemp(prefix='.studio-art-',dir=destination.parent))
        layers = []
        for layer in (0,1):
            definitions,maps = f'layer-{layer}-metatiles.bin',f'layer-{layer}-map.bin'
            (stage/definitions).write_bytes(geometry.metatiles[layer])
            (stage/maps).write_bytes(geometry.world_maps[layer])
            layers.append({'layer':layer,'pages_x':geometry.page_dimensions[0],
                'pages_y':geometry.page_dimensions[1],'metatiles':definitions,'world_map':maps})
        (stage/'palette.bin').write_bytes(palette)
        manifest = {'version':1,'base_layout':base,'allocation_profile':'original-scenery-v1',
            'layers':layers,'tiles':[],'palette':'palette.bin','hd':[],
            'generated_geometry':True,'geometry':{key:profile['geometry'][key] for key in ('length_units','width_units')}}
        compiled = compile_manifest(manifest,stage)
        if compiled.diagnostics:
            return ImportResult(manifest,compiled,compiled.diagnostics)
        (stage/'stadium.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
        os.replace(stage,destination)
        stage = None
        return ImportResult(manifest,compile_manifest(manifest,destination),())
    except (ValueError,OSError,KeyError,struct.error,IndexError) as exc:
        return ImportResult(manifest,compiled,(Diagnostic('error',('artwork',),str(exc)),))
    finally:
        if stage is not None and stage.exists():
            shutil.rmtree(stage)


def recompile_artwork(profile, rom, manifest_path, destination):
    """Publish new generated pitch maps, preserving immutable authored pixels."""
    manifest,compiled,stage = {},None,None
    try:
        source,destination = Path(manifest_path).resolve(),Path(destination).resolve()
        manifest = json.loads(source.read_text(encoding='utf-8'))
        original = compile_manifest(manifest,source.parent)
        if original.diagnostics:
            return ImportResult(manifest,original,original.diagnostics)
        if manifest.get('generated_geometry') is not True:
            raise ValueError('Only generated pitch maps can be regenerated; retain authored custom maps')
        if manifest['base_layout'] != profile['base_layout']:
            raise ValueError('Artwork must retain its original base layout')
        if destination.exists():
            raise ValueError('Artwork destination already exists')
        destination.parent.mkdir(parents=True,exist_ok=True)
        stage = Path(tempfile.mkdtemp(prefix='.studio-art-',dir=destination.parent))
        for dependency in original.dependencies:
            target = stage/dependency.relative_to(source.parent)
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(dependency,target)
        folder = stage/('pitch-'+uuid.uuid4().hex[:8])
        generated = create_artwork(profile,rom,folder)
        if generated.diagnostics:
            return generated
        manifest = copy.deepcopy(manifest)
        manifest['geometry'] = generated.manifest['geometry']
        manifest['layers'] = generated.manifest['layers']
        base = manifest['base_layout']
        for layer in manifest['layers']:
            definitions = bytearray(generated.compiled.geometry.metatiles[layer['layer']])
            world = generated.compiled.geometry.world_maps[layer['layer']]
            for record in manifest.get('tiles',[]):
                bank = record.get('palette_bank')
                if type(bank) is not int:
                    continue
                first = SLOTS[base][record['slot']][0]//16-512+record.get('offset_tiles',0)
                for position in range(0,len(definitions),2):
                    word = struct.unpack_from('<H',definitions,position)[0]
                    if first <= (word&1023) < first+record['tile_count']:
                        struct.pack_into('<H',definitions,position,(word&~0x1c00)|((bank+2)<<10))
                placement = record.get('placement')
                if placement is not None and placement['layer'] == layer['layer']:
                    definitions,world = _place_native_image(definitions,world,
                        generated.compiled.geometry.page_dimensions,placement,
                        record['image_width'],record['image_height'],first,bank)
                    definitions = bytearray(definitions)
            (folder/layer['metatiles']).write_bytes(definitions)
            (folder/layer['world_map']).write_bytes(world)
            for field in ('metatiles','world_map'):
                layer[field] = (folder/layer[field]).relative_to(stage).as_posix()
        compiled = compile_manifest(manifest,stage)
        if compiled.diagnostics:
            return ImportResult(manifest,compiled,compiled.diagnostics)
        (stage/'stadium.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
        os.replace(stage,destination)
        stage = None
        return ImportResult(manifest,compile_manifest(manifest,destination),())
    except (ValueError,OSError,KeyError,IndexError) as exc:
        return ImportResult(manifest,compiled,(Diagnostic('error',('artwork',),str(exc)),))
    finally:
        if stage is not None and stage.exists():
            shutil.rmtree(stage)


def _place_native_image(definitions, world, pages, placement, width, height, first, bank):
    """Copy-on-write native metatiles; reject overflow before publishing files."""
    if (not isinstance(placement,dict) or any(type(placement.get(key)) is not int
            for key in ('layer','x','y')) or placement['layer'] not in (0,1)):
        raise ValueError('Image placement requires a background layer and integer native-pixel coordinates')
    x,y = placement['x'],placement['y']
    if x < 0 or y < 0 or x%8 or y%8 or x+width > pages[0]*256 or y+height > pages[1]*256:
        raise ValueError('Place the image on the 8px grid inside the native background')
    changes = {}
    for ty in range(height//8):
        for tx in range(width//8):
            px,py = x+tx*8,y+ty*8
            cell = (py>>8)*pages[0]*64+((py&224)>>2)+(px>>8)*64+((px&255)>>5)
            data = changes.setdefault(cell,bytearray(definitions[world[cell]*32:world[cell]*32+32]))
            struct.pack_into('<H',data,((py&31)>>3)*8+((px&31)>>3)*2,
                             first+ty*(width//8)+tx|((bank+2)<<10))
    result,lookup,mapped = bytearray(8192),{},bytearray(world)
    for cell,old in enumerate(world):
        data = bytes(changes[cell]) if cell in changes else bytes(definitions[old*32:old*32+32])
        if data not in lookup:
            if len(lookup) == 256:
                raise ValueError('Image placement exceeds the 256 native metatile budget')
            number = len(lookup)
            lookup[data] = number
            result[number*32:number*32+32] = data
        mapped[cell] = lookup[data]
    return bytes(result),bytes(mapped)


def _native_hd_key(data,palette):
    value = 0xcbf29ce484222325
    for word in (4,*struct.unpack('<16H',data),*struct.unpack('<16H',palette)):
        value = ((value^word)*0x100000001b3)&0xffffffffffffffff
    return value or 1


def import_image(image_path, manifest_path, destination, slot, palette_bank=0, placement=None, hd_scale=1):
    """Import an 8px tile atlas into one measured native upload slot.

    Output is an independent, validated resource directory. Source files and
    other palette banks are retained. Existing output is never overwritten.
    Optional placement authors layer words on the native8px grid. Shared
    metatiles are split without changing neighboring world cells.
    """
    diagnostics = []
    manifest, compiled, stage = {}, None, None
    try:
        source = Path(manifest_path).resolve()
        destination = Path(destination).resolve()
        manifest = json.loads(source.read_text(encoding='utf-8'))
        original = compile_manifest(manifest, source.parent)
        if original.diagnostics:
            return ImportResult(manifest, original, original.diagnostics)
        if original.palette is None:
            raise ValueError('Supply a complete stadium palette before importing so unedited banks are preserved')
        base = manifest['base_layout']
        if type(slot) is not int or not 0 <= slot < len(SLOTS[base]):
            raise ValueError('Choose a measured native upload slot')
        banks = 2 if base in (2, 6) else 3
        if type(palette_bank) is not int or not 0 <= palette_bank < banks:
            raise ValueError('Choose a palette bank owned by this stadium')
        previous = next((record for record in manifest.get('tiles',[]) if record['slot'] == slot),None)
        if any(record['slot'] != slot and record.get('palette_bank') == palette_bank
               for record in manifest.get('tiles',[])):
            raise ValueError('This palette bank belongs to another authored upload; choose a different bank')
        if previous and previous.get('placement') is not None:
            if placement is None:
                placement = copy.deepcopy(previous['placement'])
            elif placement != previous['placement']:
                raise ValueError('This upload already has a placement; edit at the same position or choose another upload slot')
        if destination.exists():
            raise ValueError('Import destination already exists')
        with Image.open(image_path) as opened:
            image = opened.convert('RGBA')
        if type(hd_scale) is not int or hd_scale not in (1,2,4,8):
            raise ValueError('Choose native art or HD scale2,4or8')
        hd_source = image.copy() if hd_scale > 1 else None
        if image.width%(8*hd_scale) or image.height%(8*hd_scale):
            raise ValueError('Image dimensions must be multiples of8times the selected HD scale')
        if hd_source is not None:
            image = image.resize((image.width//hd_scale,image.height//hd_scale),Image.Resampling.BOX)
        if not image.width or not image.height or image.width % 8 or image.height % 8:
            raise ValueError('Tile atlas dimensions must be multiples of 8 pixels')
        count = image.width * image.height // 64
        if previous and previous.get('placement') is not None and (
                image.width != previous['image_width'] or image.height != previous['image_height']):
            raise ValueError('Keep the existing placement dimensions or choose another upload slot')
        if count > SLOTS[base][slot][1]:
            raise ValueError(f'This upload slot holds only {SLOTS[base][slot][1]} tiles')
        if placement is not None:
            if not isinstance(placement,dict) or placement.get('layer') not in (0,1):
                raise ValueError('Choose background layer0or1 for image placement')
            _place_native_image(original.geometry.metatiles[placement['layer']],
                original.geometry.world_maps[placement['layer']],original.geometry.page_dimensions,
                placement,image.width,image.height,SLOTS[base][slot][0]//16-512,palette_bank)
        pixels = list(image.getdata())
        opaque = [pixel[:3] for pixel in pixels if pixel[3] >= 128]
        if any(pixel[3] not in (0, 255) for pixel in pixels):
            diagnostics.append(Diagnostic('warning', ('image',),
                'Native transparency is binary; alpha values are rounded at 128'))
        colors, indices = [], []
        if opaque:
            sample = Image.new('RGB', (len(opaque), 1))
            sample.putdata(opaque)
            quantized = sample.quantize(colors=15, method=Image.Quantize.MEDIANCUT,
                                        dither=Image.Dither.NONE)
            raw = quantized.getpalette()
            used = sorted(set(quantized.getdata()))
            remap = {value: index+1 for index, value in enumerate(used)}
            colors = [tuple(raw[index*3:index*3+3]) for index in used]
            indices = [remap[value] for value in quantized.getdata()]
        palette = bytearray(original.palette)
        native_colors = [(0, 0, 0)]
        start = palette_bank*32
        palette[start:start+32] = bytes(32)
        for index, color in enumerate(colors, 1):
            components = [(value*31+127)//255 for value in color]
            value = components[0] | components[1] << 5 | components[2] << 10
            palette[start+index*2:start+index*2+2] = value.to_bytes(2, 'little')
            native_colors.append(tuple(value*255//31 for value in components))
        cursor, indexed, squared_error = 0, [], 0
        for pixel in pixels:
            value = 0
            if pixel[3] >= 128:
                value = indices[cursor]
                cursor += 1
                squared_error += sum((a-b)**2 for a, b in zip(pixel[:3], native_colors[value]))
            indexed.append(value)
        if squared_error:
            diagnostics.append(Diagnostic('warning', ('image',),
                f'Native quantization mean squared RGB error: {squared_error/max(1,len(opaque)*3):.2f}'))
        encoded = bytearray()
        for tile_y in range(0, image.height, 8):
            for tile_x in range(0, image.width, 8):
                tile = bytearray(32)
                for y in range(8):
                    for x in range(8):
                        value = indexed[(tile_y+y)*image.width+tile_x+x]
                        for plane in range(4):
                            tile[y*2+(plane & 1)+(16 if plane > 1 else 0)] |= (
                                (value >> plane) & 1) << (7-x)
                encoded.extend(tile)
        # Dependencies are copied only after all input/image budgets pass.
        destination.parent.mkdir(parents=True, exist_ok=True)
        stage = Path(tempfile.mkdtemp(prefix='.studio-import-', dir=destination.parent))
        for dependency in original.dependencies:
            target = stage / dependency.relative_to(source.parent)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(dependency, target)
        manifest = copy.deepcopy(manifest)
        # Choose a private namespace that cannot collide with preserved files.
        namespace = stage / 'imported-art'
        suffix = 0
        while namespace.exists():
            suffix += 1
            namespace = stage / f'imported-art-{suffix}'
        namespace.mkdir()
        first_character = SLOTS[base][slot][0]//16-512
        for layer in manifest['layers']:
            definitions = bytearray(original.geometry.metatiles[layer['layer']])
            for position in range(0,len(definitions),2):
                word = struct.unpack_from('<H',definitions,position)[0]
                if first_character <= (word&1023) < first_character+count:
                    word = (word&~0x1c00)|((palette_bank+2)<<10)
                    struct.pack_into('<H',definitions,position,word)
            if placement is not None and layer['layer'] == placement.get('layer'):
                definitions,world = _place_native_image(definitions,
                    original.geometry.world_maps[layer['layer']],original.geometry.page_dimensions,
                    placement,image.width,image.height,first_character,palette_bank)
                mapfile = namespace/f"layer-{layer['layer']}-world-map.bin"
                mapfile.write_bytes(world)
                layer['world_map'] = mapfile.relative_to(stage).as_posix()
            filename = namespace/f"layer-{layer['layer']}-metatiles.bin"
            filename.write_bytes(definitions)
            layer['metatiles'] = filename.relative_to(stage).as_posix()
        (namespace / 'tiles.bin').write_bytes(encoded)
        (namespace / 'palette.bin').write_bytes(palette)
        records = [record for record in manifest.get('tiles', []) if record['slot'] != slot]
        records.append({'slot': slot, 'offset_tiles': 0, 'tile_count': count, 'palette_bank':palette_bank,
                        'file': (namespace / 'tiles.bin').relative_to(stage).as_posix()})
        if placement is not None:
            records[-1].update(placement=copy.deepcopy(placement),image_width=image.width,image_height=image.height)
        manifest['tiles'] = records
        manifest['palette'] = (namespace / 'palette.bin').relative_to(stage).as_posix()
        old_keys = set(previous.get('hd_keys',[])) if previous else set()
        manifest['hd'] = [record for record in manifest.get('hd',[]) if record['key'] not in old_keys]
        if hd_source is not None:
            from .tiles import write_bmp
            generated = {}
            columns = image.width//8
            for index in range(count):
                key = _native_hd_key(encoded[index*32:index*32+32],palette[start:start+32])
                x,y = index%columns*8*hd_scale,index//columns*8*hd_scale
                tile = hd_source.crop((x,y,x+8*hd_scale,y+8*hd_scale))
                if key in generated:
                    if generated[key][0] != tile.tobytes():
                        raise ValueError('Different HD tiles reduce to the same native identity; adjust their native colors or pixels')
                    continue
                filename = namespace/f'hd-{index}.bmp'
                write_bmp(str(filename),tile)
                generated[key] = (tile.tobytes(),filename.relative_to(stage).as_posix())
            manifest['hd'] = [record for record in manifest.get('hd',[]) if int(record['key'],16) not in generated]
            manifest['hd'].extend({'key':f'{key:016x}','file':value[1]} for key,value in generated.items())
            records[-1]['hd_keys'] = [f'{key:016x}' for key in generated]
        compiled = compile_manifest(manifest, stage)
        if compiled.diagnostics:
            return ImportResult(manifest, compiled, compiled.diagnostics)
        (stage / 'stadium.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        os.replace(stage, destination)
        stage = None
        compiled = compile_manifest(manifest, destination)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        diagnostics.append(Diagnostic('error', ('image',), str(exc)))
    finally:
        if stage is not None and stage.exists():
            shutil.rmtree(stage)
    return ImportResult(manifest, compiled, tuple(diagnostics))
