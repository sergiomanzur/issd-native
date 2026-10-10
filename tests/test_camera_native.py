from pathlib import Path
import pytest
from PIL import ImageChops
from test_stats_widescreen_acceptance import run, EXE, ROM


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason='needs native build and owned ROM')
@pytest.mark.parametrize('frames', [4000, 6420], ids=['live', 'natural-goal-replay'])
def test_tactical_cameras_reveal_field_and_preserve_simulation(tmp_path, frames):
    script = ''.join(f'{f} START\n{f+10} NONE\n' for f in range(60,361,60))
    script += '560 A\n570 NONE\n'
    script += ''.join(f'{f} A\n{f+10} NONE\n' for f in range(800,3000,150))
    results = []
    for mode in (0, 1, 2):
        folder = tmp_path / str(mode)
        folder.mkdir()
        inputs = folder / 'input.txt'
        inputs.write_text(script)
        ram, image = run(folder, frames, f'engine_mode=1\naspect_ratio=2\ntrue_widescreen=1\ncamera_mode={mode}\n',
                         ('--script', str(inputs)))
        assert int.from_bytes(ram[0x70:0x72], 'little') == (8 if frames == 4000 else 19)
        results.append((ram, image))
    assert results[0][0] == results[1][0] == results[2][0]
    assert ImageChops.difference(results[0][1], results[1][1]).getbbox()
    assert ImageChops.difference(results[1][1], results[2][1]).getbbox()
