"""Retail match scene comparisons; extended periods are explicitly opt-in.

Only clock/score fixtures are edited. Scene transitions run original ROM code.
Keep captures with ISSD_VISUAL_ARTIFACTS; enable periods with ISSD_VISUAL_EXTENDED=1.
"""
import hashlib
import json
import os
from pathlib import Path

from PIL import Image, ImageChops
import pytest

from test_game_acceptance import cup_final, shorten_clock, word
from test_password_flow import password_screen, guest, EXE, ROM

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")
WIDTHS = (("original", 0, 0, 256), ("16_9", 2, 1, 398), ("21_9", 4, 1, 504))
PENALTY_WIDTHS = (WIDTHS[0], ("16_10", 3, 1, 358), *WIDTHS[1:])


def compare_scene(folder, name, seed, frames, script="0 NONE\n", expected=None,
                  *, widths=WIDTHS, capture_start=None, scenery=False, config_overrides=None):
    """Replay identical seed/input in each presentation; never mutate a mode."""
    baseline_ram, baseline_pictures = None, None
    results = []
    retained = os.environ.get("ISSD_VISUAL_ARTIFACTS")
    root = Path(retained).resolve() / folder.name / name if retained else folder / name
    for label, aspect, wide, width in widths:
        run = root / label
        run.mkdir(parents=True, exist_ok=True)
        saves = run / "owned-saves"
        saves.mkdir(exist_ok=True)
        (saves / "quicksave.sav").write_bytes(seed)
        config = {
            "engine_mode": 1, "aspect_ratio": aspect, "true_widescreen": wide,
            "internal_res": 0, "ball_outline": 0, "ball_shadow": 0,
            "player_markers": 0, "player_names": 0, "radar_scale": 1,
            "hud_scale": 1, "radar_position": 0, "radar_opacity": 100,
        }
        if config_overrides:
            config.update(config_overrides)
        (run / "isolated.cfg").write_text("".join(f"{k}={v}\n" for k, v in config.items()), encoding="ascii")
        inputs = run / "input.txt"
        inputs.write_text(script, encoding="ascii")
        start = max(1, frames - 12) if capture_start is None else max(1, capture_start)
        result = guest(run, frames, "--load-state", "1", "--script", str(inputs),
                       "--dump-frames", f"{start}:{frames-1}", "--dump-state", str(run / "state"),
                       "--screenshot", str(run / "final.bmp"), "--save-state", str(frames))
        (run / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert "[interp_cap]" not in result.stderr
        ram = (run / "state.wram").read_bytes()
        assert len(ram) == 0x20000
        if expected is not None:
            for address, value in expected.items():
                assert word(ram, address) == value, (name, label, hex(address), word(ram, address))
        pictures = []
        for capture in sorted(run.glob("f_*.bmp")) + [run / "final.bmp"]:
            with Image.open(capture) as source:
                picture = source.convert("RGB")
            assert picture.size == (width, 224), (name, label, picture.size)
            picture.save(capture.with_suffix(".png"))
            pictures.append(picture)
        assert len(pictures) == frames - start + 1, "missing consecutive scene captures"
        assert len(pictures[-1].getcolors(65536) or []) > 16, "blank/flat scene"
        if scenery and wide:
            margin = (width - 256) // 2
            for side in ((0, 100, margin, 175), (width-margin, 100, width, 175)):
                pixels = pictures[-1].crop(side)
                assert len(pixels.getcolors(65536) or []) > 8, "missing penalty pitch scenery"
                green = sum(g > r and g > b for r, g, b in pixels.getdata())
                assert green > pixels.width * pixels.height // 2, "penalty margins are not pitch"

        if baseline_ram is None:
            baseline_ram, baseline_pictures = ram, pictures
        else:
            assert ram == baseline_ram, f"{name}/{label}: full WRAM changed with width"
            margin = (width - 256) // 2
            for index, (reference, picture) in enumerate(zip(baseline_pictures, pictures)):
                center = picture.crop((margin, 0, margin + 256, 224))
                difference = ImageChops.difference(reference, center)
                if difference.getbbox():
                    difference.save(run / f"center-difference-{index}.png")
                assert difference.getbbox() is None, f"{name}/{label}: native center changed at capture {index}"
        results.append({"presentation": label, "width": width, "wram_sha256": hashlib.sha256(ram).hexdigest(),
                        "mode": word(ram, 0x70), "period": word(ram, 0xa8), "captures": len(pictures)})
    (root / "manifest.json").write_text(json.dumps({"scene": name, "frames": frames,
        "seed_sha256": hashlib.sha256(seed).hexdigest(), "clock_and_score_fixture": name.startswith("period"),
        "exe_sha256": hashlib.sha256(EXE.read_bytes()).hexdigest(),
        "rom_sha256": hashlib.sha256(ROM.read_bytes()).hexdigest(), "runs": results}, indent=2))
    return baseline_ram, (root / "original/owned-saves/quicksave.sav").read_bytes()


def test_cup_final_live_visual_widths(cup_final):
    folder, _, _ = cup_final
    compare_scene(folder, "live-first-half", (folder / "owned-saves/quicksave.sav").read_bytes(),
                  90, "0 RIGHT\n60 NONE\n", {0x70: 8, 0xa8: 0})


def test_original_pause_and_replay_visual_widths(cup_final):
    folder, _, _ = cup_final
    _, paused = compare_scene(folder, "original-pause",
        (folder / "owned-saves/quicksave.sav").read_bytes(), 120,
        "0 NONE\n40 START\n50 NONE\n", {0x70: 8})
    # START opens the cartridge's RESUME/REPLAY menu. Selecting the second
    # item must enter original replay mode, rather than merely matching pixels.
    compare_scene(folder, "original-replay", paused, 180,
        "0 NONE\n20 DOWN\n30 NONE\n60 A\n70 NONE\n", {0x70: 0x13})


@pytest.mark.skipif(os.environ.get("ISSD_VISUAL_EXTENDED") != "1",
                    reason="set ISSD_VISUAL_EXTENDED=1 for accelerated retail period transitions")
def test_retail_period_transitions_visual_widths(cup_final):
    folder, current, advance = cup_final
    for period in range(4):
        shorten_clock(folder, current, score=(0, 0))
        current, seed = compare_scene(folder, f"period-{period}-stats",
            (folder / "owned-saves/quicksave.sav").read_bytes(), 1000, expected={0x70: 0x12})
        current, seed = compare_scene(folder, f"period-{period+1}-resume", seed, 3100, advance,
            expected={0x70: 8, 0xa8: period+1} if period < 3 else {0x70: 0x0c, 0xa8: 3})
        (folder / "owned-saves/quicksave.sav").write_bytes(seed)
    # The ROM reached the goal-facing camera; now exercise real shot input.
    # $1440 is original team possession: 0=Brazil, 2=Uruguay in this fixture.
    current, seed = compare_scene(folder, "penalty-ready", seed, 90,
        expected={0x70: 0x0c, 0x1440: 0}, widths=PENALTY_WIDTHS, scenery=True)
    current, seed = compare_scene(folder, "penalty-shot-start", seed, 60,
        "0 NONE\n30 B\n50 NONE\n", expected={0x70: 0x0c},
        widths=PENALTY_WIDTHS, scenery=True)
    # Capture every following frame across the shot/camera sequence, not only
    # the final pose. Guest state and native center must remain identical.
    current, seed = compare_scene(folder, "penalty-shot-result", seed, 140,
        expected={0x70: 0x0c, 0x1440: 0}, widths=PENALTY_WIDTHS,
        capture_start=1, scenery=True)
    current, seed = compare_scene(folder, "penalty-opponent-turn", seed, 350,
        expected={0x70: 0x0c, 0x1440: 2}, widths=PENALTY_WIDTHS, scenery=True)
    compare_scene(folder, "penalty-third-turn", seed, 350,
        expected={0x70: 0x0c, 0x1440: 0, 0x1704: 3},
        widths=PENALTY_WIDTHS, scenery=True)
