"""Verify AI policies affect real cartridge decisions and reproduce on restore."""
from pathlib import Path
import os
import hashlib
import struct
import re
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs native build and user's ROM")


def run(folder, flags, frames, extra=(), environment=None):
    folder.mkdir(exist_ok=True, parents=True)
    config = folder / "config.cfg"
    config.write_text(f"gameplay_goalkeeper_ai={int(bool(flags & 1))}\ngameplay_player_ai={int(bool(flags & 2))}\n", encoding="ascii")
    result = subprocess.run([str(EXE), "--rom", str(ROM), "--config", str(config),
                             "--save-dir", str(folder / "saves"), "--headless", str(frames),
                             "--dump-state", str(folder / "state"), *extra],
                            cwd=folder, env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy", **(environment or {})),
                            capture_output=True, text=True, timeout=180)
    log = result.stdout + result.stderr
    (folder / "run.log").write_text(log, encoding="utf-8")
    assert result.returncode == 0, log
    assert "S=01AF" in log and "NMI-busy=0000" in log, log[-5000:]
    stats = re.search(r"\[Gameplay\] goalkeeper=(\d+)/(\d+) player=(\d+)/(\d+) native=(\d+) interpreted=(\d+)", log)
    assert stats, log
    return (folder / "state.wram").read_bytes(), tuple(map(int, stats.groups()))


@pytest.mark.parametrize("flags", [0, 1, 2, 3], ids=["off", "keeper", "player", "both"])
def test_live_gameplay_routes(tmp_path, flags):
    ram, stats = run(tmp_path, flags, 1800, ["--auto-start", "60"])
    assert int.from_bytes(ram[0x32:0x34], "little") == 6
    assert int.from_bytes(ram[0x70:0x72], "little") in (8, 9)
    if flags & 1:
        assert stats[1] > 0, "goalkeeper policy never reached a live decision"
    else:
        assert stats[0:2] == (0, 0)
    if flags & 2:
        assert stats[2] > 0, "formation policy was invoked but changed no movement targets"
    else:
        assert stats[2:4] == (0, 0)
    if flags:
        assert stats[4] + stats[5] > 0


def test_enabled_tweaks_replay_live_play(tmp_path):
    # Saves and all transient scopes must reproduce a real, advancing match.
    bootstrap = tmp_path / "bootstrap"
    run(bootstrap, 3, 540, ["--auto-start", "60", "--save-state", "540"])
    script = tmp_path / "input.txt"
    script.write_text("".join(f"{f} P1 RIGHT,{'B' if f % 40 == 0 else 'A'}\n{f+10} P1 RIGHT\n"
                              for f in range(0, 160, 20)), encoding="ascii")
    record = tmp_path / "record"
    (record / "saves").mkdir(parents=True)
    (record / "saves/quicksave.sav").write_bytes((bootstrap / "saves/quicksave.sav").read_bytes())
    expected, stats = run(record, 3, 240, ["--load-state", "0", "--script", str(script), "--save-state", "200",
                                          "--dump-frames", "200:239"])
    replay = tmp_path / "replay"
    (replay / "saves").mkdir(parents=True)
    (replay / "saves/quicksave.sav").write_bytes((record / "saves/quicksave.sav").read_bytes())
    replay_script = tmp_path / "replay-input.txt"
    replay_script.write_text("0 P1 RIGHT\n", encoding="ascii")
    actual, _ = run(replay, 3, 40, ["--load-state", "0", "--script", str(replay_script), "--dump-frames", "0:39"])
    assert actual == expected, "enabled tweaks changed WRAM after identical restored inputs"
    frames = set()
    for offset in range(40):
        image = (record / f"f_{200+offset:05d}.bmp").read_bytes()
        assert image == (replay / f"f_{offset:05d}.bmp").read_bytes()
        frames.add(image)
    assert len(frames) > 1, "replay was a frozen scene"
    assert stats[2] > 0
    interpreted = tmp_path / "interpreted"
    (interpreted / "saves").mkdir(parents=True)
    (interpreted / "saves/quicksave.sav").write_bytes((record / "saves/quicksave.sav").read_bytes())
    interpreted_ram, interpreted_stats = run(interpreted, 3, 40,
        ["--load-state", "0", "--script", str(replay_script)], {"SNESRECOMP_LLE_BOUNCE": "0"})
    assert interpreted_stats[2] > 0 and interpreted_stats[5] > 0, "player tweak never ran in interpreter"
    for actor in range(0x500, 0x1b00, 0x100):
        assert interpreted_ram[actor+0x50:actor+0x54] == expected[actor+0x50:actor+0x54], "execution tiers disagree on player targets"


def test_incoming_angled_shot_changes_live_keeper_target(tmp_path):
    bootstrap = tmp_path / "bootstrap"
    ram, _ = run(bootstrap, 1, 600, ["--auto-start", "60", "--save-state", "600"])
    original = (bootstrap / "saves/quicksave.sav").read_bytes()
    offset = original.find(ram)
    assert offset >= 192 and original.find(ram, offset + 1) == -1
    shot = bytearray(ram)
    def put(a, v):
        struct.pack_into("<H", shot, a, v & 0xffff)
    def word(a):
        return struct.unpack_from("<H", shot, a)[0]
    # A reproducible angled shot toward the left goal, using the cartridge's
    # isometric position/velocity fields. Keep original animation/game data.
    put(0xbc, 0); put(0x70, 8); put(0x11fa, 0)
    put(0xa4, 0); put(0xa6, 0); put(0xc8, 0)
    put(0x408, 128 + 288 - word(0x98)); put(0x40c, 288 - word(0x9a))
    put(0x406, 0); put(0x40a, 0); put(0x42a, 128); put(0x42c, 288)
    put(0x422, 0); put(0x424, -8); put(0x426, 0); put(0x428, 2)
    put(0x54c, 0); put(0x56e, 0); put(0x59e, 0)
    envelope = bytearray(original)
    envelope[offset:offset+len(shot)] = shot
    envelope[160:192] = hashlib.sha256(envelope[:160] + envelope[192:]).digest()
    replay = tmp_path / "shot"
    (replay / "saves").mkdir(parents=True)
    (replay / "saves/quicksave.sav").write_bytes(envelope)
    native_ram, stats = run(replay, 1, 20, ["--load-state", "0"])
    assert stats[0] > 0, "incoming angled shot changed no real keeper positioning targets"
    interpreted = tmp_path / "interpreted-shot"
    (interpreted / "saves").mkdir(parents=True)
    (interpreted / "saves/quicksave.sav").write_bytes(envelope)
    interpreted_ram, interpreted_stats = run(interpreted, 1, 20, ["--load-state", "0"],
                                             {"SNESRECOMP_LLE_BOUNCE": "0"})
    assert interpreted_stats[0] > 0 and interpreted_stats[5] > 0
    assert interpreted_ram[0x550:0x554] == native_ram[0x550:0x554], "execution tiers disagree on keeper aim"
