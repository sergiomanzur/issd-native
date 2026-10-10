"""Read-only decompression must reproduce the actual original native maps."""
from pathlib import Path
import importlib.util
import pytest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'tools/mod_studio/stadium_geometry.py'


def test_rom_templates_match_all_eight_original_constructors():
    assert MODULE.exists(), 'Original template reader is missing'
    from tools.mod_studio import stadium_geometry as module
    rom = ROOT / 'International Superstar Soccer Deluxe (USA).sfc'
    evidence = ROOT / 'build/independent-stadium-validation/original-contract'
    if not rom.exists() or not evidence.exists():
        pytest.skip('Requires private cartridge and fresh original construction captures')
    for layout in range(8):
        ram = (evidence / f'original-{layout}/state.wram').read_bytes()
        template = module.read_template(rom.read_bytes(), layout)
        for layer in range(2):
            captured = ram[0x18000 + layer * 0x2000:0x1a000 + layer * 0x2000]
            # The original descriptors leave unused definition slots intact;
            # the intro's stale bytes are not part of the selected template.
            for index, written in enumerate(template['written_metatile_bytes'][layer]):
                if written:
                    assert template['metatiles'][layer][index] == captured[index]
            for identity in set(template['world_maps'][layer]):
                start = identity * 32
                assert template['metatiles'][layer][start:start+32] == captured[start:start+32]
            assert template['world_maps'][layer] == ram[
                0x1d000 + layer * 0x1000:0x1e000 + layer * 0x1000]
