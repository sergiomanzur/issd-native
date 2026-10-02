"""Real ROM bug-fix hooks; constructed ownership fixture in a healthy exhibition.

The fixture deliberately modifies a checksummed snapshot to assign several
human controllers to one moving keeper. It is not a naturally captured match.
The CPU, original movement opcodes, input reader and production hooks run intact.
"""
from pathlib import Path
import hashlib
import os
import re
import struct
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="requires native build and user's ROM")


def word(ram, address):
    return struct.unpack_from("<H", ram, address)[0]


def run(folder, enabled, frames, *, snapshot=None, save=None, bootstrap=False, ai=False, interpreted=False):
    folder.mkdir(parents=True, exist_ok=True)
    cfg = folder / "config.cfg"
    cfg.write_text(f"engine_mode=0\ninternal_res=0\ntrue_widescreen=0\ngameplay_bug_fixes={int(enabled)}\n"
                   f"gameplay_goalkeeper_ai={int(ai)}\ngameplay_player_ai={int(ai)}\n", encoding="ascii")
    saves = folder / "saves"
    saves.mkdir(exist_ok=True)
    if snapshot is not None:
        (saves / "quicksave.sav").write_bytes(snapshot)
    command = [str(EXE), "--rom", str(ROM), "--config", str(cfg), "--save-dir", str(saves),
               "--mods-dir", str(folder / "empty-mods"), "--headless", str(frames),
               "--dump-state", str(folder / "state")]
    entries = []
    if bootstrap:
        for frame in range(60, 361, 60):
            entries.extend([(frame, "START"), (frame + 10, "NONE")])
        entries.extend([(560, "A"), (570, "NONE")])
        for frame in range(800, 3000, 150):
            entries.extend([(frame, "A"), (frame + 10, "NONE")])
        entries.append((3000, "NONE"))
    else:
        # L remains valid held input for every controller while the fixture's
        # movement velocity isolates repeated position integration.
        entries.extend([(0, f"P{i} L") for i in range(1, 5)])
    script = folder / "input.txt"
    script.write_text("".join(f"{frame} {button}\n" for frame, button in sorted(entries)), encoding="ascii")
    command += ["--script", str(script)]
    if snapshot is not None:
        command += ["--load-state", "0"]
    if save is not None:
        command += ["--save-state", str(save)]
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy")
    if interpreted:
        env["SNESRECOMP_LLE_BOUNCE"] = "0"
    result = subprocess.run(command, cwd=folder, env=env, capture_output=True, text=True, timeout=180)
    log = result.stdout + result.stderr
    (folder / "run.log").write_text(log, encoding="utf-8")
    ram = (folder / "state.wram").read_bytes() if (folder / "state.wram").exists() else None
    return result, ram, log


def healthy(result, ram, log):
    assert result.returncode == 0, log
    assert ram is not None and "S=01AF" in log and "NMI-busy=0000" in log, log[-3000:]
    assert "[interp_cap]" not in log


@pytest.fixture(scope="module")
def exhibitions(tmp_path_factory):
    cases = {}
    for enabled in (False, True):
        folder = tmp_path_factory.mktemp(f"bugfix-exhibition-{enabled}")
        result, ram, log = run(folder, enabled, 4000, bootstrap=True, save=4000)
        healthy(result, ram, log)
        assert word(ram, 0x32) == 6 and word(ram, 0x70) == 8
        assert word(ram, 0xde07) == 0, "original exhibition constructor was not used"
        assert "[Match] setup" in log and "[Match] kickoff" in log
        cases[enabled] = (ram, (folder / "saves/quicksave.sav").read_bytes())
    return cases


def keeper_fixture(base, count):
    ram, envelope = base
    offset = envelope.find(ram)
    assert offset >= 192 and envelope.find(ram, offset + 1) < 0
    modified = bytearray(ram)
    def put(a, v):
        struct.pack_into("<H", modified, a, v & 0xffff)
    for address in (0xbc, 0xa4, 0xa6, 0x12f6):
        put(address, 0)
    put(0xda8, 1); put(0x1bac, 1)
    put(0x560, 0xa381); put(0x56e, 0x80)
    for address, value in ((0x506, 0), (0x508, 80), (0x50a, 0), (0x50c, 120),
                           (0x522, 0), (0x524, 2), (0x526, 0), (0x528, 0)):
        put(address, value)
    for i in range(5):
        put(0x1aa0 + i * 0x30 + 0x2c, 0x500 if i < count else 0)
        put(0x1aa0 + i * 0x30 + 0x2e, 0)
    output = bytearray(envelope)
    output[offset:offset + len(modified)] = modified
    output[160:192] = hashlib.sha256(output[:160] + output[192:]).digest()
    return bytes(output)


def test_live_keeper_speed_stacking_and_original_single_owner(tmp_path, exhibitions):
    positions = {}
    for enabled in (False, True):
        for count in range(1, 5):
            result, ram, log = run(tmp_path / f"{enabled}-{count}", enabled, 1,
                                   snapshot=keeper_fixture(exhibitions[enabled], count))
            healthy(result, ram, log)
            positions[enabled, count] = ram[0x506:0x50e]
            assert all(word(ram, 0x1aa0 + i * 0x30 + 0x2c) == 0x500 for i in range(count))
            assert all(word(ram, 0x1aa0 + i * 0x30 + 0x12) & 0x20 for i in range(count)), "held input was lost"
            changes = re.search(r"\[BugFixes\] keeper=(\d+)", log)
            assert changes and (int(changes[1]) > 0) == (enabled and count > 1), log
    assert positions[True, 1] == positions[False, 1], "normal one-owner movement changed"
    for count in range(2, 5):
        assert positions[True, count] == positions[True, 1], "enabled movement still stacks"
        assert positions[False, count] != positions[False, 1], "original bug did not reproduce"


def test_keeper_bugfix_interpreted_route_and_snapshot_replay(tmp_path, exhibitions):
    source = keeper_fixture(exhibitions[True], 4)
    result, native, log = run(tmp_path / "native", True, 20, snapshot=source, save=10)
    healthy(result, native, log)
    saved = (tmp_path / "native/saves/quicksave.sav").read_bytes()
    result, restored, log = run(tmp_path / "restored", True, 10, snapshot=saved)
    healthy(result, restored, log)
    assert restored == native, "enabled save/load changed full guest WRAM"
    result, interpreted, log = run(tmp_path / "interpreted", True, 1, snapshot=source, interpreted=True)
    healthy(result, interpreted, log)
    result, one_frame, log = run(tmp_path / "one-frame", True, 1, snapshot=source)
    healthy(result, one_frame, log)
    assert interpreted[0x506:0x50e] == one_frame[0x506:0x50e]


def test_bugfix_context_change_rejects_load_and_preserves_saved_file(tmp_path, exhibitions):
    source = exhibitions[False][1]
    result, _, log = run(tmp_path, True, 1, snapshot=source)
    assert result.returncode != 0 and "compatib" in log.lower(), log
    assert (tmp_path / "saves/quicksave.sav").read_bytes() == source


def test_all_gameplay_policies_smoke(tmp_path):
    result, ram, log = run(tmp_path, True, 4000, bootstrap=True, ai=True)
    healthy(result, ram, log)
    assert word(ram, 0x32) == 6 and word(ram, 0x70) == 8
    assert "[Gameplay]" in log and "[BugFixes]" in log
