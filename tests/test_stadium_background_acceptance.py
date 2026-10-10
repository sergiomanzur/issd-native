"""Liga MX stadium constructors and genuine native renderer coverage.

The selected stadium is changed in a test-owned pre-constructor snapshot.
After selecting each slot, original initialization and recorded input construct
the match. The capture camera is produced by the game itself.
Added logical slots retain their identity while using one of the eight
original match layouts. Ordinary menu serialization is tested separately.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

from PIL import Image, ImageChops
import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("ISSD_TEST_EXE", ROOT / "build/ISSDNative.exe"))
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
LIGA_PACK = "Liga MX y Expansion MX"
EXPANDED_PACKS = "All-Star Legends|Chivas de Guadalajara|Formation Showcase|Liga MX y Expansion MX|FIFA World Cup 2026|World Cup 2026 - Mexico"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


def word(data, address):
    return struct.unpack_from("<H", data, address)[0]


def original_input():
    entries = {f: "START" for f in range(60, 361, 60)}
    entries.update({f + 10: "NONE" for f in range(60, 361, 60)})
    entries.update({500: "DOWN", 510: "NONE", 560: "A", 570: "NONE"})
    for f in list(range(800, 1600, 100)) + list(range(1800, 2900, 200)):
        entries.update({f: "A", f + 10: "NONE"})
    return entries


def run(folder, frames, aspect=0, snapshot=None, script="0 NONE\n", save=False,
        packs=LIGA_PACK, timeout=120, capture_scale=1, hd=True, extra_args=()):
    folder.mkdir(parents=True, exist_ok=True)
    config = folder / "isolated.cfg"
    config.write_text(f"engine_mode=0\ninternal_res=0\ntrue_widescreen={int(aspect != 0)}\n"
                      f"aspect_ratio={aspect}\nactive_mod_packs={packs}\n"
                      f"hd_texture_packs={'estadio_akron_hd' if hd else ''}\n", encoding="ascii")
    (folder / "input.txt").write_text(script, encoding="ascii")
    args = []
    if snapshot is not None:
        (folder / "saves").mkdir(exist_ok=True)
        (folder / "saves/quicksave.sav").write_bytes(snapshot)
        args += ["--load-state", "0"]
    if save:
        args += ["--save-state", str(frames)]
    result = subprocess.run([str(EXE), "--rom", str(ROM), "--headless", str(frames),
        "--config", str(config), "--save-dir", str(folder / "saves"),
        "--mods-dir", str(ROOT / "mods"), "--script", str(folder / "input.txt"),
        "--dump-state", str(folder / "state"), "--screenshot", str(folder / "frame.bmp"),
        "--capture-scale", str(capture_scale),
        *args, *extra_args], cwd=folder, capture_output=True, text=True, timeout=timeout,
        env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"))
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    picture = Image.open(folder / "frame.bmp").convert("RGB")
    picture.save(folder / "frame.png")
    return (folder / "state.wram").read_bytes(), (folder / "state.ppu").read_bytes(), picture


def edit_snapshot(snapshot, ram, changes):
    owned = bytearray(snapshot)
    offset = owned.find(ram)
    assert offset >= 192 and owned.find(ram, offset + 1) == -1
    for address, value in changes:
        struct.pack_into("<H", owned, offset + address, value)
    owned[160:192] = hashlib.sha256(owned[:160] + owned[192:]).digest()
    return owned


def bounds(ram, layer):
    stride = word(ram, 0x1ffcc)
    occupied = [(((i % stride) // 64) * 256 + (i & 7) * 32,
                 (i // stride) * 256 + ((i & 63) >> 3) * 32)
                for i, value in enumerate(ram[0x1d000 + layer * 4096:0x1e000 + layer * 4096])
                if value]
    assert occupied
    return min(x for x, y in occupied), max(x for x, y in occupied), min(y for x, y in occupied), max(y for x, y in occupied)


@pytest.fixture(scope="module")
def stadium_seed(tmp_path_factory):
    folder = tmp_path_factory.mktemp("stadium-selection-seed")
    return make_seed(folder, LIGA_PACK)


@pytest.fixture(scope="module")
def expanded_seed(tmp_path_factory):
    folder = tmp_path_factory.mktemp("expanded-stadium-selection-seed")
    return make_seed(folder, EXPANDED_PACKS)


def make_seed(folder, packs):
    script = "".join(f"{f} {button}\n" for f, button in sorted(original_input().items()))
    ram, _, _ = run(folder, 1100, script=script, save=True, packs=packs)
    assert word(ram, 0x70) == 0x0c and word(ram, 0x1ffcc) == 0
    return (folder / "saves/quicksave.sav").read_bytes(), ram


@pytest.mark.parametrize("stadium", range(8))
def test_shipped_stadium_backgrounds(tmp_path, stadium_seed, stadium):
    check_stadium(tmp_path, stadium_seed, stadium, LIGA_PACK)


@pytest.mark.parametrize("stadium", range(8, 12))
def test_added_stadium_backgrounds(tmp_path, expanded_seed, stadium):
    check_stadium(tmp_path, expanded_seed, stadium, EXPANDED_PACKS)


def check_stadium(tmp_path, stadium_seed, stadium, packs, timeout=120):
    # Preserve the logical identity independently of the stock layout index.
    # $86 >= 8 selects special cutscenes or reads past eight-entry match tables.
    selected = edit_snapshot(*stadium_seed, [(0x1fa2, stadium), (0x86, stadium & 7)])
    follow = "0 NONE\n" + "".join(f"{f - 1100} {button}\n" for f, button in sorted(original_input().items()) if f > 1100)

    def capture(aspect):
        folder = tmp_path / f"aspect-{aspect}"
        return run(folder, 2101, aspect=aspect, snapshot=selected, script=follow,
                   packs=packs, timeout=timeout)

    with ThreadPoolExecutor(max_workers=2) as pool:
        baseline_result, wide_result = list(pool.map(capture, (0, 2)))
    baseline_ram, baseline_ppu, baseline = baseline_result
    actual_ram, actual_ppu, picture = wide_result
    for ram in (baseline_ram, actual_ram):
        assert word(ram, 0x70) == 8 and word(ram, 0x1fa2) == stadium
        assert word(ram, 0x86) == (stadium & 7)
        assert 0x80 <= word(ram, 0x1ffcc) <= 0x340
        assert word(ram, 0x50) == 1
    assert actual_ram == baseline_ram, "presentation changed guest WRAM"
    assert actual_ppu[-65536:] == baseline_ppu[-65536:], "presentation changed guest VRAM"
    assert baseline.size == (256, 224) and picture.size == (398, 224)
    assert ImageChops.difference(picture.crop((71, 0, 327, 224)), baseline).getbbox() is None
    for side, box in (("left", (0, 40, 71, 180)), ("right", (327, 40, 398, 180))):
        assert len(picture.crop(box).getcolors(65536)) > 8, f"stadium {stadium} {side} not expanded"
    evidence = {"stadium": stadium, "stock_layout": stadium & 7,
                "fixture_selection": True, "new_geometry": False,
                "camera": [word(actual_ram, 0x13a0), word(actual_ram, 0x13b0)],
                "stride": word(actual_ram, 0x1ffcc), "bounds": [bounds(actual_ram, layer) for layer in range(2)],
                "active_packs": packs, "aspect": 2, "width": 398,
                "map_sha256": hashlib.sha256(actual_ram[0x18000:0x1f000]).hexdigest()}
    (tmp_path / "coverage.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
