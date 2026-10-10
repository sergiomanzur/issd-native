"""Verify shipped-pack coverage and HD replacement of actual Liga MX pitch tiles."""
import csv
import struct
from PIL import Image, ImageChops
import pytest
from test_stadium_background_acceptance import (
    stadium_seed, edit_snapshot, original_input, run, word, EXE, ROM)

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                               reason="requires native build and owned retail ROM")

# Each restart shows a 300-frame mod notification. Its wrapping intentionally
# depends on display width, so capture after it expires without masking pixels.
CAPTURE_FRAMES = 302


def pitch_pack(dump, destination):
    """Give genuine dumped 4bpp green pitch tiles unmistakable 4x detail."""
    destination.mkdir()
    count = 0
    with (dump / "tiles.csv").open() as manifest:
        for tile in csv.DictReader(manifest):
            if tile["bpp"] != "4":
                continue
            source = Image.open(dump / (tile["tile"] + ".bmp")).convert("RGB")
            if sum(g > r and g > b for r, g, b in source.getdata()) < 32:
                continue
            # Deliberately diagnostic colors: HUD, notice or native scaling
            # cannot accidentally provide these checkerboard subpixels.
            pixels = b"".join(struct.pack("<I", 0xFFFF00FF if (x ^ y) & 1 else 0xFF0000FF)
                              for y in range(32) for x in range(32))
            header = struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
            header += struct.pack("<IiiHHIIiiII", 40, 32, -32, 1, 32, 0,
                                  len(pixels), 0, 0, 0, 0)
            (destination / (tile["tile"] + ".bmp")).write_bytes(header + pixels)
            count += 1
    assert count > 0, "fixture must find actual green pitch tiles"


@pytest.mark.parametrize("stadium", [0, 7])
def test_liga_hd_center_and_actual_pitch_margin_replacements(tmp_path, stadium_seed, stadium):
    selected = edit_snapshot(*stadium_seed, [(0x1fa2, stadium), (0x86, stadium)])
    follow = "0 NONE\n" + "".join(f"{f-1100} {button}\n" for f, button in
        sorted(original_input().items()) if f > 1100)
    ram, _, original = run(tmp_path / "original", 2101, snapshot=selected,
                            script=follow, capture_scale=4, save=True)
    assert word(ram, 0x70) == 8 and original.size == (1024, 896)
    seed = (tmp_path / "original/saves/quicksave.sav").read_bytes()
    baseline_ram, _, baseline = run(tmp_path / "hd-original", CAPTURE_FRAMES,
                                    snapshot=seed, capture_scale=4)
    dump = tmp_path / "pitch-dump"
    _, _, native = run(tmp_path / "native-original", CAPTURE_FRAMES, snapshot=seed, hd=False,
                       extra_args=("--dump-tiles", str(dump), "--dump-tiles-from", "300"))
    nearest = native.resize(baseline.size, Image.Resampling.NEAREST)
    assert ImageChops.difference(baseline, nearest).getbbox(), (
        "shipped HD pack did not replace any pixels; fixture cannot validate HD")
    # The shipped Akron assets match score/clock/player-name artwork in these Liga MX
    # fixtures; their pitch art uses different identities. Do not claim that
    # enabling this pack implies replacements exist for every grass tile.
    assert ImageChops.difference(baseline.crop((0, 128, 1024, 776)),
                                nearest.crop((0, 128, 1024, 776))).getbbox() is None
    replacement_pack = tmp_path / "actual-pitch-hd"
    pitch_pack(dump, replacement_pack)
    _, _, replaced = run(tmp_path / "pitch-original", CAPTURE_FRAMES, snapshot=seed,
                         hd=False, capture_scale=4,
                         extra_args=("--hd-pack", str(replacement_pack)))
    assert ImageChops.difference(replaced.crop((0, 128, 1024, 776)),
                                nearest.crop((0, 128, 1024, 776))).getbbox()
    for aspect, width in ((3,358), (2,398), (4,504)):
        actual, actual_ppu, picture = run(tmp_path / str(aspect), CAPTURE_FRAMES, aspect=aspect,
                                        snapshot=seed, capture_scale=4)
        no_hd, native_ppu, native_wide = run(tmp_path / f"native-{aspect}", CAPTURE_FRAMES,
                                           aspect=aspect, snapshot=seed, hd=False)
        assert actual == baseline_ram == no_hd
        assert actual_ppu[-65536:] == native_ppu[-65536:]
        margin = (width-256)//2
        assert picture.size == (width*4,896)
        center = (margin*4,0,(margin+256)*4,896)
        assert ImageChops.difference(picture.crop(center), baseline).getbbox() is None
        nearest_wide = native_wide.resize(picture.size, Image.Resampling.NEAREST)
        replaced_ram, replaced_ppu, pitch_wide = run(tmp_path / f"pitch-{aspect}", CAPTURE_FRAMES,
            aspect=aspect, snapshot=seed, hd=False, capture_scale=4,
            extra_args=("--hd-pack", str(replacement_pack)))
        assert replaced_ram == baseline_ram
        assert replaced_ppu[-65536:] == native_ppu[-65536:]
        assert ImageChops.difference(pitch_wide.crop(center), replaced).getbbox() is None
        for box in ((0,0,margin*4,896), ((margin+256)*4,0,width*4,896)):
            assert ImageChops.difference(picture.crop(box), nearest_wide.crop(box)).getbbox() is None
            assert ImageChops.difference(pitch_wide.crop(box), nearest_wide.crop(box)).getbbox(), (
                f"stadium {stadium}: actual pitch HD tile is missing from added margin")
