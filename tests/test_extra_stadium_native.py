"""Ordinary retail stadium input, preserved logical ID and native save/reload."""
import json
import os

import pytest

from test_stadium_background_acceptance import EXE, ROM, EXPANDED_PACKS, run, word

pytestmark = pytest.mark.skipif(
    not EXE.exists() or not ROM.exists(),
    reason="requires native executable and user's retail ROM",
)


def script(entries):
    return ''.join(f'{frame} {button}\n' for frame, button in sorted(entries.items()))


def exhibition_picker_input():
    entries = {frame: 'START' for frame in range(60, 361, 60)}
    entries.update({frame + 10: 'NONE' for frame in range(60, 361, 60)})
    entries.update({560: 'A', 570: 'NONE'})
    for frame in range(800, 1251, 150):
        entries.update({frame: 'A', frame + 10: 'NONE'})
    return entries


@pytest.fixture(scope='module')
def native_picker(tmp_path_factory):
    folder = tmp_path_factory.mktemp('native-stadium-picker')
    ram, _, _ = run(folder, 1400, script=script(exhibition_picker_input()),
                    packs=EXPANDED_PACKS, save=True, hd=False)
    assert word(ram, 0x70) == 12 and word(ram, 0x1fa2) == 1
    assert [word(ram, address) for address in (0x18, 0x1a, 0x1c, 0x1e)] == [52, 44, 48, 40]
    return (folder / 'saves/quicksave.sav').read_bytes()


def select_input(stadium):
    entries = {0: 'NONE'}
    for step in range(stadium - 1):
        entries.update({60 + step * 60: 'RIGHT', 70 + step * 60: 'NONE'})
    confirm = 60 + (stadium - 1) * 60 + 100
    entries.update({confirm: 'A', confirm + 10: 'NONE'})
    for step in range(12):
        entries.update({confirm + 200 + step * 150: 'A',
                        confirm + 210 + step * 150: 'NONE'})
    return script(entries)


@pytest.mark.parametrize('stadium', range(8, 12))
def test_original_menu_added_stadium_to_live_and_reload(tmp_path, native_picker, stadium):
    folder = tmp_path / 'live'
    ram, _, _ = run(folder, 4300, snapshot=native_picker, script=select_input(stadium),
                    packs=EXPANDED_PACKS, save=True, hd=False)
    assert word(ram, 0x70) == 8
    assert word(ram, 0x1fa2) == stadium
    assert word(ram, 0x86) == (stadium & 7)
    snapshot = (folder / 'saves/quicksave.sav').read_bytes()
    restored, _, _ = run(tmp_path / 'reload', 180, snapshot=snapshot,
                         packs=EXPANDED_PACKS, hd=False)
    assert word(restored, 0x1fa2) == stadium
    assert word(restored, 0x86) == (stadium & 7)
    assert word(restored, 0x70) == 8 and restored != ram
    assert (tmp_path / 'reload/saves/quicksave.sav').read_bytes() == snapshot
    (tmp_path / 'evidence.json').write_text(json.dumps({
        'stadium': stadium, 'match_layout': stadium & 7,
        'guest_state_edits': False, 'native_save_reload': True,
    }, indent=2))


def test_original_stadium_picker_wraps_all_twelve_slots(tmp_path, native_picker):
    # Default USA (1), LEFT to Brazil (0), LEFT wraps to added Guadalajara (11).
    left = '0 NONE\n60 LEFT\n70 NONE\n120 LEFT\n130 NONE\n240 A\n250 NONE\n'
    ram, _, _ = run(tmp_path / 'left', 400, snapshot=native_picker,
                    script=left, packs=EXPANDED_PACKS, hd=False)
    assert word(ram, 0x1fa2) == 11
    right = ('0 NONE\n60 LEFT\n70 NONE\n120 LEFT\n130 NONE\n'
             '180 RIGHT\n190 NONE\n300 A\n310 NONE\n')
    ram, _, _ = run(tmp_path / 'right', 460, snapshot=native_picker,
                    script=right, packs=EXPANDED_PACKS, hd=False)
    assert word(ram, 0x1fa2) == 0


@pytest.fixture(scope='module')
def naturally_scored_added_stadium_goal(tmp_path_factory, native_picker):
    folder = tmp_path_factory.mktemp('natural-added-stadium-goal')
    initial = folder / 'initial'
    run(initial, 12000, snapshot=native_picker, script=select_input(9),
        packs=EXPANDED_PACKS, save=True, hd=False, timeout=300)
    entries = {0: 'NONE'}
    for step in range(120):
        entries.update({60 + step * 180: 'B', 70 + step * 180: 'NONE'})
    goal = folder / 'goal'
    ram, _, _ = run(goal, 24000,
                    snapshot=(initial / 'saves/quicksave.sav').read_bytes(),
                    script=script(entries), packs=EXPANDED_PACKS, save=True,
                    hd=False, timeout=400)
    assert (word(ram, 0x70), word(ram, 0x72)) == (0x13, 6)
    assert (word(ram, 0xda2), word(ram, 0xea2)) == (0, 1)
    assert word(ram, 0x1fa2) == 9 and word(ram, 0x86) == 1
    return (goal / 'saves/quicksave.sav').read_bytes(), word(ram, 0x18aa)


@pytest.mark.skipif(os.environ.get('ISSD_NATURAL_STADIUMS') != '1',
                    reason='set ISSD_NATURAL_STADIUMS=1 for the sustained natural goal run')
def test_naturally_scored_added_stadium_replay_widths(tmp_path, naturally_scored_added_stadium_goal):
    from PIL import Image, ImageChops

    seed, initial_cursor = naturally_scored_added_stadium_goal
    baseline = None
    baseline_captures = None
    for aspect, width in ((0, 256), (3, 358), (2, 398), (4, 504)):
        ram, ppu, picture = run(tmp_path / f'width-{width}', 450,
                                snapshot=seed,
                                script='0 NONE\n310 A\n320 NONE\n330 X\n340 NONE\n',
                                packs=EXPANDED_PACKS, aspect=aspect, hd=False,
                                extra_args=('--dump-frames', '375:449'))
        assert (word(ram, 0x70), word(ram, 0x72)) == (0x13, 6)
        assert word(ram, 0x1fa2) == 9 and word(ram, 0x86) == 1
        assert word(ram, 0x18ae) == 0 and word(ram, 0x18b2) == 0
        assert word(ram, 0x18aa) != initial_cursor
        captures = sorted((tmp_path / f'width-{width}').glob('f_*.bmp'))
        assert len(captures) == 75
        assert len({capture.read_bytes() for capture in captures}) > 1
        assert picture.size == (width, 224)
        if baseline is None:
            baseline = ram, ppu, picture
            baseline_captures = [Image.open(capture).convert('RGB') for capture in captures]
        else:
            assert ram == baseline[0]
            assert ppu[-65536:] == baseline[1][-65536:]
            margin = (width - 256) // 2
            assert ImageChops.difference(
                picture.crop((margin, 0, margin + 256, 224)), baseline[2]
            ).getbbox() is None
            for capture, reference in zip(captures, baseline_captures):
                current = Image.open(capture).convert('RGB')
                assert ImageChops.difference(
                    current.crop((margin, 0, margin + 256, 224)), reference
                ).getbbox() is None
