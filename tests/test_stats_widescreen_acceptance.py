"""Original halftime transition; only the test-owned clock is shortened."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess

from PIL import Image, ImageChops
import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/ISSDNative.exe'
ROM = ROOT / 'International Superstar Soccer Deluxe (USA).sfc'
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason='needs native build and retail ROM')


def run(folder, frames, settings='', extra=(), layer=None):
    folder.mkdir(parents=True, exist_ok=True)
    cfg = folder / 'isolated.cfg'
    cfg.write_text('engine_mode=0\ninternal_res=0\n' + settings, encoding='ascii')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    if layer is not None:
        env['SNESRECOMP_LAYER_MASK'] = str(layer)
    result = subprocess.run([str(EXE), '--rom', str(ROM), '--headless', str(frames),
        '--config', str(cfg), '--save-dir', str(folder / 'saves'),
        '--mods-dir', str(folder / 'empty-mods'), '--dump-state', str(folder / 'state'),
        '--screenshot', str(folder / 'frame.bmp'), *extra], cwd=folder,
        env=env, capture_output=True, text=True, timeout=120)
    (folder / 'run.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    image = Image.open(folder / 'frame.bmp').convert('RGB')
    image.save(folder / 'frame.png')
    return (folder / 'state.wram').read_bytes(), image


def test_halftime_stats_widescreen(tmp_path):
    seed = tmp_path / 'seed'
    seed.mkdir()
    script = seed / 'input.txt'
    script.write_text(''.join(f'{f} START\n{f+10} NONE\n' for f in range(60,361,60))
        + '500 DOWN\n510 NONE\n560 A\n570 NONE\n'
        + ''.join(f'{f} A\n{f+10} NONE\n' for f in range(800,1600,100))
        + ''.join(f'{f} A\n{f+10} NONE\n' for f in range(1800,2900,200)), encoding='ascii')
    ram, _ = run(seed, 3101, extra=('--script', str(script), '--save-state', '3101'))
    assert struct.unpack_from('<H', ram, 0x70)[0] == 8
    snapshot = bytearray((seed / 'saves/quicksave.sav').read_bytes())
    offset = snapshot.find(ram)
    assert offset >= 192 and snapshot.find(ram, offset + 1) == -1
    for address, value in [(0x16d0, 1), (0x16d2, 0)]:
        struct.pack_into('<H', snapshot, offset + address, value)
    snapshot[160:192] = hashlib.sha256(snapshot[:160] + snapshot[192:]).digest()
    baseline = baseline_ram = None
    for label, settings, width in [('classic', '', 256),
            ('16_10', 'true_widescreen=1\naspect_ratio=3\n', 358),
            ('16_9', 'true_widescreen=1\naspect_ratio=2\n', 398),
            ('21_9', 'true_widescreen=1\naspect_ratio=4\n', 504)]:
        folder = tmp_path / label
        (folder / 'saves').mkdir(parents=True)
        (folder / 'saves/quicksave.sav').write_bytes(snapshot)
        ram, picture = run(folder, 1000, settings, ('--load-state', '0'))
        assert struct.unpack_from('<H', ram, 0x70)[0] == 0x12
        assert picture.size == (width, 224)
        if baseline is None:
            baseline, baseline_ram = picture, ram
            continue
        assert ram == baseline_ram
        margin = (width - 256) // 2
        assert ImageChops.difference(picture.crop((margin, 0, margin+256, 224)), baseline).getbbox() is None
        for box in [(0, 0, margin, 224), (margin+256, 0, width, 224)]:
            assert len(picture.crop(box).getcolors(65536)) > 8, 'stats still pillarboxed'
