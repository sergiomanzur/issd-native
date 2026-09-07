"""Real-ROM regression: expanded pitch, isolated artifacts, unchanged simulation.

python tests/test_widescreen.py --exe build-fixes/ISSDNative.exe
Use --artifacts PATH to retain the outputs for visual review.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageChops


def run(root, exe, rom, frames):
    baseline_ram = None
    baseline_picture = None
    for name, aspect, enabled, width in [
        ("original", 0, 0, 256), ("16_10", 3, 1, 358),
        ("16_9", 2, 1, 398), ("21_9", 4, 1, 446),
    ]:
        folder = root / name
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "issd_config.json").write_text(json.dumps({
            "engine_mode": 1, "aspect_ratio": aspect,
            "true_widescreen": enabled, "internal_res": 0,
        }, indent=2), encoding="utf-8")
        result = subprocess.run([
            str(exe), "--headless", str(frames), "--auto-start", "60",
            "--screenshot", "frame.bmp", "--dump-state", "state", str(rom),
        ], cwd=folder, capture_output=True, text=True, timeout=180)
        (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == 0, f"{name}: execution failed; see {folder / 'run.log'}"
        picture = Image.open(folder / "frame.bmp").convert("RGB")
        assert picture.size == (width, 224), f"{name}: unexpected size {picture.size}"
        picture.save(folder / "frame.png")
        ram = (folder / "state.wram").read_bytes()
        assert len(ram) == 0x20000
        if baseline_ram is None:
            baseline_ram = ram
            baseline_picture = picture.copy()
        else:
            assert ram == baseline_ram, f"{name}: widescreen changed simulation WRAM"
        if enabled:
            margin = (width - 256) // 2
            center = picture.crop((margin, 0, margin+256, 224))
            assert ImageChops.difference(center, baseline_picture).getbbox() is None, (
                f"{name}: native center pixels changed")
            for side, rectangle in [
                ("left", (0, 40, margin, 210)),
                ("right", (width-margin, 40, width, 210)),
            ]:
                colors = picture.crop(rectangle).getcolors(maxcolors=65536)
                assert colors and len(colors) > 8, f"{name}: {side} pitch not expanded"
            if name == "21_9" and frames == 1200:
                # Recorded regression: player D00 sits at x327 with a valid
                # pose and native offscreen flag. Its blue jersey must appear.
                assert int.from_bytes(ram[0xd08:0xd0a], "little") == 327
                assert ram[0xd1e] == 1
                crop = picture.crop((margin+300, 0, width, 80))
                blue = sum(b > r+40 and b > g+20 for r, g, b in crop.getdata())
                assert blue > 30, "whole player omitted from expanded right view"
                crop.save(folder / "offscreen-player.png")
        print(f"PASS {name}: {width}x224, simulation unchanged, frame {frames}", flush=True)
    print(f"Artifacts: {root}", flush=True)


if __name__ == "__main__":
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=repo / "build-fixes/ISSDNative.exe")
    parser.add_argument("--rom", type=Path, default=repo / "International Superstar Soccer Deluxe (USA).sfc")
    parser.add_argument("--frames", type=int, default=1200)
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    if args.artifacts:
        run(args.artifacts.resolve(), args.exe.resolve(), args.rom.resolve(), args.frames)
    else:
        with tempfile.TemporaryDirectory(prefix="issd-widescreen-") as temp:
            run(Path(temp), args.exe.resolve(), args.rom.resolve(), args.frames)
