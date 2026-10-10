"""Complete a retail Exhibition through both periods, then inspect its main menu.

Only test-owned clock and score values are shortened. All transitions, menu
callbacks, graphics and controller input are executed by the retail guest.
"""
import hashlib
import struct

from PIL import ImageChops
import pytest

from test_stats_widescreen_acceptance import EXE, ROM, run

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                               reason="needs native build and owned retail ROM")


def word(ram, address):
    return struct.unpack_from('<H', ram, address)[0]


def shorten(snapshot, ram, score=None):
    data = bytearray(snapshot)
    offset = data.find(ram)
    assert offset >= 192 and data.find(ram, offset + 1) == -1
    edits = [(0x16d0, 1), (0x16d2, 0)]
    if score is not None:
        edits += [(0xda2, score[0]), (0xea2, score[1])]
    for address, value in edits:
        struct.pack_into('<H', data, offset + address, value)
    data[160:192] = hashlib.sha256(data[:160] + data[192:]).digest()
    return bytes(data)


def test_completed_exhibition_returns_clean_widescreen_main_menu(tmp_path):
    def stage(name, frames, script='0 NONE\n', snapshot=None):
        folder = tmp_path / name
        (folder / 'saves').mkdir(parents=True)
        inputs = folder / 'input.txt'
        inputs.write_text(script, encoding='ascii')
        extra = ['--script', str(inputs), '--save-state', str(frames)]
        if snapshot is not None:
            (folder / 'saves/quicksave.sav').write_bytes(snapshot)
            extra += ['--load-state', '0']
        ram, image = run(folder, frames, extra=extra)
        return (folder / 'saves/quicksave.sav').read_bytes(), ram, image

    boot = ''.join(f'{f} START\n{f+10} NONE\n' for f in range(60, 361, 60))
    boot += '560 A\n570 NONE\n'
    boot += ''.join(f'{f} A\n{f+10} NONE\n' for f in range(800, 3000, 150))
    boot += '3000 NONE\n'
    snapshot, ram, _ = stage('live', 4800, boot)
    assert word(ram, 0xde07) == 0 and word(ram, 0x70) == 8
    snapshot, ram, _ = stage('halftime', 1000, snapshot=shorten(snapshot, ram))
    assert word(ram, 0x70) == 0x12 and word(ram, 0xa8) == 0
    confirm = '0 NONE\n200 A\n210 NONE\n'
    snapshot, ram, _ = stage('halftime-menu', 550, confirm, snapshot)
    assert word(ram, 0x70) == 0x0c
    snapshot, ram, _ = stage('second-half', 1000, confirm, snapshot)
    assert word(ram, 0x70) == 8 and word(ram, 0xa8) == 1
    snapshot, ram, _ = stage('fulltime', 1500, snapshot=shorten(snapshot, ram, (2, 0)))
    assert word(ram, 0x70) == 0x12 and word(ram, 0xa8) == 1

    baseline = baseline_ram = None
    for label, settings, width in [
        ('classic', '', 256),
        ('16_10', 'true_widescreen=1\naspect_ratio=3\n', 358),
        ('16_9', 'true_widescreen=1\naspect_ratio=2\n', 398),
        ('21_9', 'true_widescreen=1\naspect_ratio=4\n', 504),
    ]:
        folder = tmp_path / label
        (folder / 'saves').mkdir(parents=True)
        (folder / 'saves/quicksave.sav').write_bytes(snapshot)
        inputs = folder / 'input.txt'
        inputs.write_text(confirm, encoding='ascii')
        extra = ('--load-state', '0', '--script', str(inputs))
        ram, picture = run(folder, 650, settings, extra)
        assert word(ram, 0x70) == 0x0c and word(ram, 0x1648) == 0
        # Original main-menu redraw/input callback, not another blue submenu.
        assert ram[0x1446:0x1449] == bytes.fromhex('479da4')
        assert picture.size == (width, 224)
        picture.save(folder / 'full-menu.png')
        if baseline is None:
            baseline, baseline_ram = picture, ram
            continue
        assert ram == baseline_ram
        margin = (width - 256) // 2
        assert ImageChops.difference(picture.crop((margin, 0, margin+256, 224)), baseline).getbbox() is None
        _, wallpaper = run(folder, 650, settings, extra, layer=2)
        for box in [(0, 0, margin, 224), (margin+256, 0, width, 224)]:
            assert len(picture.crop(box).getcolors(65536)) > 8
            assert ImageChops.difference(picture.crop(box), wallpaper.crop(box)).getbbox() is None, 'menu content duplicated into wallpaper margins'
