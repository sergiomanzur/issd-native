from pathlib import Path
import pytest
from tools.mod_studio.stadium_assets import SLOTS


def test_preview_static_character_uploads_match_original_native_captures():
    from tools.mod_studio import stadium_geometry
    assert hasattr(stadium_geometry,'read_preview_resources'),'Private native preview resources missing'
    root = Path(__file__).resolve().parents[1]
    rom_path = root/'International Superstar Soccer Deluxe (USA).sfc'
    captured = root/'build/independent-stadium-validation/original-contract-queues'
    if not rom_path.exists() or not captured.exists():
        pytest.skip('Requires private original captures')
    rom = rom_path.read_bytes()
    for base in range(8):
        vram,palette = stadium_geometry.read_preview_resources(rom,base)
        actual = (captured/f'original-{base}/state.ppu').read_bytes()[-65536:]
        assert len(vram) == 65536 and len(palette) == 512
        for destination,count in SLOTS[base]:
            # Layout6's native ordered overlap is resolved in both outputs.
            start,end = destination*2,(destination+count*16)*2
            assert vram[start:end] == actual[start:end],f'Original {base} owned characters {destination:04x}'
