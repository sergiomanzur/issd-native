"""Retail management wallpaper must fill margins without copying its panels."""
from pathlib import Path
import pytest
from PIL import ImageChops
from team_changes_helpers import (bootstrap, native, REQUEST_MENU, OPEN_FORMATION,
                                 EXIT_FORMATION, OPEN_SQUAD_AFTER_FORMATION, SUBSTITUTE,
                                 EXE, ROM)
from test_stats_widescreen_acceptance import run

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                               reason="requires native build and owned retail ROM")


def test_management_wallpaper_uses_only_background(tmp_path):
    seed, _ = bootstrap(tmp_path / "bootstrap")
    stages = (("management", REQUEST_MENU), ("formation", OPEN_FORMATION),
              ("return", EXIT_FORMATION), ("squad", OPEN_SQUAD_AFTER_FORMATION),
              ("substitution", SUBSTITUTE))
    for name, (frames, script) in stages:
        seed, _, _ = native(tmp_path / name / "advance", frames, script, seed=seed)
        baseline_ram = baseline = None
        for aspect, width in ((0, 256), (3, 358), (2, 398), (4, 504)):
            settings = (f"true_widescreen={int(aspect != 0)}\naspect_ratio={aspect}\n"
                        "gameplay_goalkeeper_ai=1\ngameplay_player_ai=1\n")
            folder = tmp_path / name / str(aspect)
            (folder / "saves").mkdir(parents=True)
            (folder / "saves/quicksave.sav").write_bytes(seed)
            ram, picture = run(folder, 2, settings, ("--load-state", "0"))
            margin = (width - 256) // 2
            assert picture.size == (width, 224)
            if baseline is None:
                baseline, baseline_ram = picture, ram
                continue
            assert ram == baseline_ram
            assert ImageChops.difference(picture.crop((margin, 0, margin+256, 224)),
                                        baseline).getbbox() is None
            _, wallpaper = run(folder, 2, settings, ("--load-state", "0"), layer=2)
            for box in ((0, 16, margin, 180), (margin+256, 16, width, 180)):
                edge = picture.crop(box)
                assert len(edge.getcolors(65536)) > 8, f"{name}: missing wallpaper"
                assert ImageChops.difference(edge, wallpaper.crop(box)).getbbox() is None, (
                    f"{name}: foreground leaked into wallpaper")
