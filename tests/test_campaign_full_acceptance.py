"""Complete fresh-start retail tournament with clocks/scores accelerated only.

This never imports a password or seeds rounds, stages, entrants, opponent,
period, callback or champion. Each native result is followed by a new-process
Continue before the next original match. A full played-clock/human campaign
remains a separate acceptance check. Native guest launches are bounded to120s.
"""
import hashlib
import json
import os
import struct
import subprocess

import pytest
from test_game_acceptance import ROOT, ROM, word
from pathlib import Path

EXE = Path(os.environ.get("ISSD_ACCEPTANCE_EXE", str(ROOT / "build/ISSDNative.exe")))

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


def guest(folder, frames, *args):
    result = subprocess.run([str(EXE), "--rom", str(ROM), "--headless", str(frames),
                             "--config", str(folder / "isolated.cfg"),
                             "--save-dir", str(folder / "owned-saves"), *args],
                            cwd=folder, capture_output=True, text=True, timeout=120,
                            env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"))
    assert result.returncode == 0, result.stdout + result.stderr
    assert "[interp_cap]" not in result.stderr
    return result


def run(folder, name, frames, script="0 NONE\n", *, load=True):
    path = folder / "input.txt"
    path.write_text(script, encoding="ascii")
    args = ["--script", str(path), "--dump-state", str(folder / name),
            "--save-state", str(frames)]
    if load:
        args += ["--load-state", "1"]
    guest(folder, frames, *args)
    return (folder / (name + ".wram")).read_bytes()


def shorten_clock(folder, ram, *, score=None):
    path = folder / "owned-saves/quicksave.sav"
    data = bytearray(path.read_bytes())
    offset = data.find(ram)
    assert data[:4] == b"ISCE" and offset >= 192 and data.find(ram, offset + 1) == -1
    # Zero both clocks. Leaving the real clock at1 with display0 can wait for
    # a display-clock tick that never arrives in a paused/dead-ball phase.
    edits = [(0x16d0, 0), (0x16d2, 0)]
    if score is not None:
        edits += [(0xda2, score[0]), (0xea2, score[1])]
    for address, value in edits:
        struct.pack_into("<H", data, offset + address, value)
    data[160:192] = hashlib.sha256(data[:160] + data[192:]).digest()
    path.write_bytes(data)


def campaign_file(folder):
    paths = list((folder / "owned-saves/campaigns").glob("*/campaign.sav"))
    assert len(paths) == 1
    return paths[0]


def generation(data):
    return int.from_bytes(data[24:32], "little")


def callback(ram):
    return ram[0x1446:0x1449].hex()


def assert_continue(folder, ram, prefix):
    path = campaign_file(folder)
    saved = path.read_bytes()
    guest(folder, 80, "--continue", "--dump-state", str(folder / prefix),
          "--save-state", "80")
    restored = (folder / (prefix + ".wram")).read_bytes()
    assert restored[0x1640:0x1698] == ram[0x1640:0x1698]
    assert restored[0xdc00:0xdeff] == ram[0xdc00:0xdeff]
    assert path.read_bytes() == saved, "Continue republished its own checkpoint"
    return restored


def enter_match(folder, prefix, advance, *, period=0):
    current = run(folder, prefix, 3100, advance)
    # New-match intros can outlast the first menu pulses; use actual button
    # input and bounded waits rather than changing the original task state.
    for attempt in range(3):
        if word(current, 0x70) == 8 and word(current, 0xa8) == period:
            return current
        if word(current, 0x70) == 0x13:
            # Original ShowReplay ignores A; START returns to the pitch.
            current = run(folder, f"{prefix}-replay-{attempt}", 400,
                          "0 NONE\n50 START\n60 NONE\n")
        else:
            current = run(folder, f"{prefix}-intro-{attempt}", 1200,
                          "0 NONE\n200 A\n210 NONE\n500 A\n510 NONE\n800 A\n810 NONE\n")
    assert word(current, 0x70) == 8 and word(current, 0xa8) == period, (word(current, 0x70), callback(current))
    return current


def play_campaign(folder, *, world=False, probe_matches=None):
    trace = []
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n"
    if world:
        script += "530 DOWN\n540 NONE\n"
    script += "560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1600, 100))
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(1800, 2900, 200))
    current = run(folder, "fresh-first-half", 3101, script, load=False)
    advance = "0 NONE\n" + "".join(f"{f} A\n{f+10} NONE\n" for f in range(200, 1600, 150))
    advance += "1800 B\n1810 NONE\n2200 B\n2210 NONE\n"
    path = campaign_file(folder)
    assert generation(path.read_bytes()) == 1
    limit = 35 if world else 20
    for index in range(1, limit + 1):
        assert word(current, 0x70) == 8 and word(current, 0xa8) == 0
        stage = word(current, 0xddff)
        round_number = word(current, 0xde11)
        opponent = word(current, 0xea0)
        previous = generation(path.read_bytes())
        shorten_clock(folder, current)
        halftime = run(folder, f"{index:02}-halftime", 1000)
        assert word(halftime, 0x70) == 0x12
        assert generation(path.read_bytes()) == previous
        second = enter_match(folder, f"{index:02}-second", advance, period=1)
        assert word(second, 0x70) == 8 and word(second, 0xa8) == 1
        shorten_clock(folder, second, score=(2, 0))
        fulltime = run(folder, f"{index:02}-fulltime", 1000)
        assert word(fulltime, 0x70) == 0x12
        assert generation(path.read_bytes()) == previous
        result = run(folder, f"{index:02}-result", 550, "0 NONE\n200 A\n210 NONE\n")
        entry = dict(match=index, prior_stage=stage, prior_round=round_number,
                     opponent=opponent, stage=word(result, 0x1640),
                     round=word(result, 0x1652), callback=callback(result),
                     generation=generation(path.read_bytes()),
                     label=path.read_bytes()[80:144].split(b"\0")[0].decode("ascii"))
        trace.append(entry)
        (folder / "progression.json").write_text(json.dumps(trace, indent=2), encoding="utf-8")
        print(entry, flush=True)
        complete = not world and word(result, 0x1640) == 9
        if complete:
            assert result[0xddce] == result[0xda0]
            terminal = run(folder, "ceremony-finished", 3000)
            assert callback(terminal) == "c0c88b"
            result = run(folder, "completed", 550, "0 NONE\n200 A\n210 NONE\n")
        elif world and index == 35:
            assert word(result, 0x1652) == 35
            result = run(folder, "completed", 550, "0 NONE\n200 A\n210 NONE\n")
            assert callback(result) == "ee948b"
            complete = True
        saved = path.read_bytes()
        assert generation(saved) > previous, f"match {index} committed without checkpoint: {entry}"
        result = assert_continue(folder, result, f"{index:02}-continued")
        if complete:
            expected = b"World Series complete: Brazil" if world else b"Cup complete: Brazil"
            assert saved[80:144].split(b"\0")[0] == expected
            return trace
        if probe_matches is not None and index == probe_matches:
            return trace
        current = enter_match(folder, f"{index:02}-next-match", advance)
    pytest.fail(f"Tournament did not complete within {limit} original matches: {trace}")


def test_complete_original_cup_with_result_continue(tmp_path):
    trace = play_campaign(tmp_path)
    assert len(trace) == 9, "Cup skipped expected qualifying/group/knockout progression"
    assert [entry["prior_stage"] for entry in trace] == list(range(9))
    assert [entry["stage"] for entry in trace] == list(range(1, 10))


@pytest.mark.skipif(os.environ.get("ISSD_RUN_FULL_WORLD") != "1",
                    reason="35-match acceptance is opt-in: ISSD_RUN_FULL_WORLD=1")
def test_complete_original_world_series_with_result_continue(tmp_path):
    trace = play_campaign(tmp_path, world=True)
    assert len(trace) == 35
    assert [entry["round"] for entry in trace] == list(range(1, 36))
    assert len({entry["opponent"] for entry in trace}) == 35
