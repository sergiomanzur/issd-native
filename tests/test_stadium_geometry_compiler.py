from pathlib import Path
import struct
from tools.mod_studio import stadium_geometry as geometry


def template():
    definitions, maps, lookup = bytearray(), bytearray(), {}
    for index in range(4096):
        x = ((index % 704) // 64) * 256 + (index & 7) * 32
        y = (index // 704) * 256 + ((index & 63) >> 3) * 32
        words = []
        for dy in range(0, 32, 8):
            for dx in range(0, 32, 8):
                u, v = x + dx - y - dy + 64, y + dy - 224
                word = 1
                if 0 <= v <= 576 and u in (0, 1792):
                    word = 2
                elif 0 <= v <= 576 and u == 896:
                    word = 4
                elif 232 <= v <= 344 and 1848 <= u <= 1864:
                    word = 3
                words.append(word)
        block = struct.pack('<16H', *words)
        if block not in lookup:
            lookup[block] = len(lookup)
            definitions.extend(block)
        maps.append(lookup[block])
    assert len(definitions) <= 8192
    definitions.extend(bytes(8192-len(definitions)))
    return {'base_layout': 0, 'length_units': 1792, 'width_units': 576,
            'stride': 704, 'page_dimensions': (11, 5),
            'metatiles': {layer: bytes(definitions) for layer in range(2)},
            'world_maps': {layer: bytes(maps) for layer in range(2)}}


def tile(compiled, u, v=288, layer=1):
    x, y = u + v + 160, v + 224
    index = (y >> 8)*704 + ((y & 224) >> 2) + (x >> 8)*64 + ((x & 255) >> 5)
    identity = compiled.world_maps[layer][index]
    offset = identity*32 + ((y & 31) >> 3)*8 + ((x & 31) >> 3)*2
    return struct.unpack_from('<H', compiled.metatiles[layer], offset)[0]


def test_pitch_end_center_and_goal_move_without_old_marks():
    assert hasattr(geometry, 'compile_geometry'), 'Geometry compiler missing'
    for length in (1728, 1664):
        compiled = geometry.compile_geometry({'version': 1, 'base_layout': 0,
            'geometry': {'length_units': length, 'width_units': 576}}, template())
        assert not compiled.diagnostics
        assert tile(compiled, length) == 2
        assert tile(compiled, length//2) == 4
        assert tile(compiled, length+64) == 3
        assert tile(compiled, 896) == 1
        assert tile(compiled, 1856) == 1
        assert compiled.derived_fields[0x12a2] == length
        assert compiled.derived_fields[0x1c8a] == length + 72
        assert len(compiled.metatiles[1]) == 8192
        assert len(compiled.world_maps[1]) == 4096


def test_original_geometry_is_an_identity_operation():
    assert hasattr(geometry, 'compile_geometry'), 'Geometry compiler missing'
    original = template()
    compiled = geometry.compile_geometry({'version': 1, 'base_layout': 0,
        'geometry': {'length_units': 1792, 'width_units': 576}}, original)
    assert compiled.metatiles == original['metatiles']
    assert compiled.world_maps == original['world_maps']
