"""Original-input coin introduction, isolated configs/saves and retail ROM.

The timed Exhibition script exercises moving banners and the actual coin
minigame. Auto-start skips that presentation and cannot verify this feature.
"""
import os
from pathlib import Path
import subprocess

from PIL import Image, ImageChops
import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("ISSD_TEST_EXE", ROOT / "build/ISSDNative.exe"))
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


def original_exhibition_input():
    entries = {frame: "START" for frame in range(60, 361, 60)}
    entries.update({frame + 10: "NONE" for frame in range(60, 361, 60)})
    entries.update({560: "A", 570: "NONE"})
    for frame in range(800, 3000, 150):
        entries.update({frame: "A", frame + 10: "NONE"})
    return "".join(f"{frame} {button}\n" for frame, button in sorted(entries.items()))


def crowd_source_x(logical_x, map_row, artwork_loaded=True):
    """Independent pixel oracle for authored strips, including native seams."""
    left = logical_x < 0
    if artwork_loaded and 5 <= map_row <= 23:
        if left:
            return logical_x % (8 if 12 <= map_row <= 15 else 16)
        return 240 + (logical_x-256) % 16
    if artwork_loaded and 24 <= map_row <= 28:
        return 112 + (logical_x+128) % 96 if left else 160 + (logical_x-256) % 96
    return logical_x % 128 if left else 128 + (logical_x-256) % 128


@pytest.mark.parametrize("frames,stage", [(1940, 9), (2450, 0x13)],
                         ids=["moving-team-banners", "actual-coin-minigame"])
def test_original_coin_widescreen(tmp_path, frames, stage):
    baseline_picture = baseline_ram = None
    for label, aspect, enabled, width in [
        ("original", 0, 0, 256), ("16_10", 3, 1, 358),
        ("16_9", 2, 1, 398), ("21_9", 4, 1, 504),
    ]:
        folder = tmp_path / label
        folder.mkdir()
        config = folder / "isolated.cfg"
        config.write_text(f"engine_mode=0\ninternal_res=0\ntrue_widescreen={enabled}\n"
                          f"aspect_ratio={aspect}\n", encoding="ascii")
        script = folder / "input.txt"
        script.write_text(original_exhibition_input(), encoding="ascii")
        result = subprocess.run([
            str(EXE), "--rom", str(ROM), "--headless", str(frames),
            "--script", str(script), "--config", str(config),
            "--save-dir", str(folder / "owned-saves"),
            "--mods-dir", str(folder / "empty-mods"),
            "--dump-state", str(folder / "state"),
            "--screenshot", str(folder / "frame.bmp"),
        ], cwd=folder, capture_output=True, text=True, timeout=120,
            env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"))
        (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == 0, result.stdout + result.stderr
        ram = (folder / "state.wram").read_bytes()
        assert int.from_bytes(ram[0x70:0x72], "little") == 0x0f
        assert int.from_bytes(ram[0x72:0x74], "little") == stage
        assert int.from_bytes(ram[0x86:0x88], "little") == 8
        picture = Image.open(folder / "frame.bmp").convert("RGB")
        picture.save(folder / "frame.png")
        assert picture.size == (width, 224)
        if baseline_picture is None:
            baseline_picture, baseline_ram = picture.copy(), ram
            continue
        assert ram == baseline_ram, f"{label}: coin presentation changed WRAM"
        margin = (width - 256) // 2
        center = picture.crop((margin, 0, margin + 256, 224))
        assert ImageChops.difference(center, baseline_picture).getbbox() is None, (
            f"{label}: original center pixels changed")
        for side, box in [("left", (0, 0, margin, 224)),
                          ("right", (margin + 256, 0, width, 224))]:
            crop = picture.crop(box)
            assert len(crop.getcolors(65536)) > 8, f"{label}: {side} still pillarboxed"
            if frames == 2450:
                # BG1 contains both TV and crowd. Added columns must be
                # varied authored scenery strips, never copies of the TV.
                for yy in range(224):
                    for xx in range(margin):
                        gx = xx - margin if side == "left" else 256 + xx
                        source_x = crowd_source_x(gx, ((544+yy)//8) % 64)
                        assert crop.getpixel((xx, yy)) == baseline_picture.getpixel((source_x, yy)), (
                            f"{label}: {side} cloned TV or pitch at ({xx},{yy})")
                fans = crop.crop((0, 165, margin, 185))
                assert fans.crop((0, 0, margin-8, 20)).tobytes() != fans.crop((8, 0, margin, 20)).tobytes(), (
                    f"{label}: {side} front crowd still repeats every 8 pixels")
            # Rendering banners outside guest edges used to reveal stock names
            # in added columns. Their yellow/red glyphs must stay in the center.
            if frames == 1940:
                band = crop.crop((0, 60, margin, 85))
                assert not any(r > 180 and g > 140 and b < 80
                               for r, g, b in band.getdata()), (
                                   f"{label}: banner text leaked into {side} margin")


def test_coin_vertical_scroll_16_9(tmp_path):
    """Follow the whole sky-to-TV scroll and coin fade, rather than one frame.

    Isolating BG1 provides a scenery reference without native edge sprites
    (waving flags), whose clipping must not be mistaken for tile duplication.
    """
    pictures = {}
    states = {}
    for label, aspect, enabled, layer_mask in [
        ("original", 0, 0, "255"), ("bg1", 0, 0, "1"),
        ("16_9", 2, 1, "255"),
    ]:
        folder = tmp_path / label
        folder.mkdir()
        config = folder / "isolated.cfg"
        config.write_text(f"engine_mode=0\ninternal_res=0\ntrue_widescreen={enabled}\n"
                          f"aspect_ratio={aspect}\n", encoding="ascii")
        script = folder / "input.txt"
        script.write_text(original_exhibition_input(), encoding="ascii")
        result = subprocess.run([
            str(EXE), "--rom", str(ROM), "--headless", "3100",
            "--script", str(script), "--config", str(config),
            "--save-dir", str(folder / "owned-saves"),
            "--mods-dir", str(folder / "empty-mods"),
            "--dump-state", str(folder / "state"),
            "--dump-frames", "1900:2960",
        ], cwd=folder, capture_output=True, text=True, timeout=120,
            env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy",
                     SNESRECOMP_LAYER_MASK=layer_mask))
        (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == 0, result.stdout + result.stderr
        states[label] = (folder / "state.wram").read_bytes()
        pictures[label] = folder
    assert states["original"] == states["16_9"] == states["bg1"]
    assert int.from_bytes(states["original"][0x70:0x72], "little") == 8

    for frame in range(1900, 2961):
        with Image.open(pictures["original"] / f"f_{frame:05d}.bmp") as src:
            original = src.convert("RGB")
        with Image.open(pictures["16_9"] / f"f_{frame:05d}.bmp") as src:
            wide = src.convert("RGB")
        assert wide.size == (398, 224)
        assert wide.crop((71, 0, 327, 224)).tobytes() == original.tobytes(), (
            f"scroll frame {frame}: native center changed")

    # Sky/floodlights, TV entering vertically, settled hand/coin, direction
    # selection and final fade all retain one TV and aligned crowd rows.
    for frame in (1950, 2050, 2150, 2200, 2250, 2300, 2450, 2600, 2750, 2900, 2950):
        with Image.open(pictures["bg1"] / f"f_{frame:05d}.bmp") as src:
            scenery = src.convert("RGB")
        with Image.open(pictures["16_9"] / f"f_{frame:05d}.bmp") as src:
            wide = src.convert("RGB")
        expected = Image.new("RGB", (398, 224))
        # Verified cartridge scroll: stageB advances BG1 by2px each frame,
        # starting at dump1969, then settles at mapY544 for the minigame.
        first_y = min(544, max(0, 2*(frame-1969)))
        for yy in range(224):
            map_row = ((first_y+yy)//8) % 64
            for x in range(71):
                for destination, logical in [(x, x-71), (x+327, 256+x)]:
                    source = crowd_source_x(logical, map_row, first_y+yy >= 512)
                    expected.putpixel((destination,yy), scenery.getpixel((source,yy)))
        # Every added pixel must be scenery. This includes the nearest two
        # columns where a native crowd flag used to leak during the scroll.
        for box in [(0, 0, 71, 224), (327, 0, 398, 224)]:
            assert wide.crop(box).tobytes() == expected.crop(box).tobytes(), (
                f"scroll frame {frame}: scenery/TV duplication in margin")
    # The front fan band must contain horizontally varied complete people;
    # this specifically rejects the original single8px-column duplication.
    with Image.open(pictures["16_9"] / "f_02450.bmp") as src:
        fans = src.convert("RGB").crop((0, 165, 71, 185))
    assert fans.crop((0,0,63,20)).tobytes() != fans.crop((8,0,71,20)).tobytes(), (
        "front crowd still repeats every8px")
    def frame_bytes(frame):
        with Image.open(pictures["original"] / f"f_{frame:05d}.bmp") as src:
            return src.convert("RGB").tobytes()
    assert frame_bytes(2050) != frame_bytes(2150) != frame_bytes(2250), (
        "fixture no longer exercises vertical scrolling")
