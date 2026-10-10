"""Input-driven replacement persistence; private owned snapshots only.

The halftime case accelerates only the clock. It never edits team, period,
management callback, lineup or replacement flags.
"""
import hashlib
import struct

import pytest
from test_game_acceptance import cup_final
from test_password_flow import password_screen
from team_changes_helpers import (EXE, ROM, bootstrap, native, word,
    REQUEST_MENU, SUBSTITUTE, EXIT_SQUAD, RESUME, ADVANCE_LIVE)

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                              reason="requires native build and user's retail ROM")
OPEN_SQUAD = (300, "0 NONE\n20 DOWN\n30 NONE\n60 A\n70 NONE\n")


def next_replacement(bench_steps):
    # Return from the bench to the field list, select the next available
    # outfielder and then select a different, unused bench record.
    script = "0 NONE\n20 RIGHT\n30 NONE\n60 DOWN\n70 NONE\n100 A\n110 NONE\n140 LEFT\n150 NONE\n"
    for step in range(bench_steps):
        frame = 220 + step * 40
        script += f"{frame} DOWN\n{frame + 10} NONE\n"
    frame = 220 + bench_steps * 40
    return 700, script + f"{frame} A\n{frame + 10} NONE\n"


def assert_replay(folder, seed, flags, stage):
    first, a, _ = native(folder / "first", *stage, seed=seed, flags=flags)
    second, b, _ = native(folder / "second", *stage, seed=seed, flags=flags)
    assert a == b
    header = int.from_bytes(first[12:16], "little")
    assert header == int.from_bytes(second[12:16], "little") == 192
    assert first[header:] == second[header:]
    assert (folder / "first/final.bmp").read_bytes() == (folder / "second/final.bmp").read_bytes()
    return first, a


@pytest.mark.parametrize("flags", [0, 3], ids=["original-ai", "both-policies"])
def test_pending_multiple_replacements_restore_and_resume(tmp_path, flags):
    seed, initial = bootstrap(tmp_path / "bootstrap", flags)
    for name, stage in [("request", REQUEST_MENU), ("squad", OPEN_SQUAD),
                        ("first-sub", SUBSTITUTE)]:
        seed, ram, _ = native(tmp_path / name, *stage, seed=seed, flags=flags)
    assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
    seed, ram = assert_replay(tmp_path / "pending-replay", seed, flags, next_replacement(2))
    assert ram[0x3f93] == 14 and ram[0x3f9e] == 0x43
    assert ram[0x3fa4:0x3fb8] == initial[0x3fa4:0x3fb8]
    seed, ram, _ = native(tmp_path / "third-sub", *next_replacement(3), seed=seed, flags=flags)
    assert ram[0x3f95] == 17 and ram[0x3fa1] == 0x45
    exhausted_lineup = ram[0x3f90:0x3fb8]
    # The rejected attempt leaves a different field-selection cursor. Resume
    # from the saved third replacement to exercise the normal commit route.
    _, ram, _ = native(tmp_path / "fourth-rejected", *next_replacement(4), seed=seed, flags=flags)
    assert ram[0x3f90:0x3fb8] == exhausted_lineup, "fourth replacement exceeded original allowance"
    for name, stage in [("exit", EXIT_SQUAD), ("resume", RESUME), ("live", ADVANCE_LIVE)]:
        seed, ram, _ = native(tmp_path / name, *stage, seed=seed, flags=flags)
    assert word(ram, 0x70) == 8 and word(ram, 0xbc) == 0
    assert ram[0x3f91] == 12 and ram[0x3f93] == 14
    assert ram[0x3f9c] == 0x81 and ram[0x3f9e] == 0x83
    assert ram[0x3f95] == 17 and ram[0x3fa1] == 0x85
    assert word(ram, 0x1e6c) == 0xffff
    _, replay = assert_replay(tmp_path / "live-replay", seed, flags,
                              (120, "0 RIGHT\n80 NONE\n"))
    assert replay != ram, "restored replacement match must advance"
    assert replay[0x3f90:0x3fb8] == ram[0x3f90:0x3fb8]


def shorten_clock(seed, ram):
    data = bytearray(seed)
    offset = data.find(ram)
    assert offset >= 192 and data.find(ram, offset + 1) == -1
    for address, value in [(0x16d0, 1), (0x16d2, 0)]:
        struct.pack_into("<H", data, offset + address, value)
    data[160:192] = hashlib.sha256(data[:160] + data[192:]).digest()
    return bytes(data)


def test_original_halftime_replacement_survives_second_half(tmp_path):
    boot = "".join(f"{f} START\n{f + 10} NONE\n" for f in range(60, 361, 60))
    boot += "560 A\n570 NONE\n"
    boot += "".join(f"{f} A\n{f + 10} NONE\n" for f in range(800, 3000, 150))
    boot += "3000 NONE\n"
    seed, initial, _ = native(tmp_path / "original-menu-boot", 4800, boot, flags=0)
    assert word(initial, 0x70) == 8 and word(initial, 0xa8) == 0
    assert word(initial, 0xde07) == 0, "must use original Exhibition setup"
    seed, ram, _ = native(tmp_path / "halftime", 1000,
        seed=shorten_clock(seed, initial), flags=0)
    assert word(ram, 0x70) == 0x12 and word(ram, 0xa8) == 0
    seed, ram, _ = native(tmp_path / "halftime-menu", 550,
        "0 NONE\n200 A\n210 NONE\n", seed=seed, flags=0)
    assert word(ram, 0x70) == 0x0c and word(ram, 0xe8) == 2
    for name, stage in [("squad", OPEN_SQUAD), ("substitute", SUBSTITUTE),
                        ("exit", EXIT_SQUAD), ("resume", RESUME)]:
        seed, ram, _ = native(tmp_path / name, *stage, seed=seed, flags=0)
        if name == "substitute":
            assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
    assert word(ram, 0x70) == 8 and word(ram, 0xa8) == 1
    assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x81
    assert ram[0x3fa4:0x3fb8] == initial[0x3fa4:0x3fb8]
    _, replay = assert_replay(tmp_path / "second-half-replay", seed, 0,
                              (120, "0 RIGHT\n80 NONE\n"))
    assert replay != ram
    assert replay[0x3f90:0x3fb8] == ram[0x3f90:0x3fb8]

def test_extra_time_halftime_replacement_resumes_final_period(cup_final, tmp_path):
    folder, ram, advance = cup_final
    seed = (folder / "owned-saves/quicksave.sav").read_bytes()
    for period in range(2):
        seed, ram, _ = native(tmp_path / f"period-{period}-stats", 1000,
            seed=shorten_clock(seed, ram), flags=0)
        assert word(ram, 0x70) == 0x12
        seed, ram, _ = native(tmp_path / f"period-{period + 1}-resume", 3100,
            advance, seed=seed, flags=0)
        assert word(ram, 0x70) == 8 and word(ram, 0xa8) == period + 1
    original_lineups = ram[0x3f90:0x3fb8]
    seed, ram, _ = native(tmp_path / "extra-time-halftime", 1000,
        seed=shorten_clock(seed, ram), flags=0)
    assert word(ram, 0x70) == 0x12 and word(ram, 0xa8) == 2
    seed, ram, _ = native(tmp_path / "extra-time-menu", 550,
        "0 NONE\n200 A\n210 NONE\n", seed=seed, flags=0)
    assert word(ram, 0x70) == 0x0c
    for name, stage in [("squad", OPEN_SQUAD), ("substitute", SUBSTITUTE),
                        ("exit", EXIT_SQUAD), ("resume", RESUME)]:
        seed, ram, _ = native(tmp_path / name, *stage, seed=seed, flags=0)
        if name == "substitute":
            assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
    assert word(ram, 0x70) == 8 and word(ram, 0xa8) == 3
    assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x81
    assert ram[0x3fa4:0x3fb8] == original_lineups[20:]
    _, replay = assert_replay(tmp_path / "extra-time-replay", seed, 0,
                              (120, "0 RIGHT\n80 NONE\n"))
    assert replay != ram
    assert replay[0x3f90:0x3fb8] == ram[0x3f90:0x3fb8]


def on_controller(stage, player):
    return stage[0], "".join(f"{frame} {player} {buttons}\n"
        for frame, buttons in (line.split(" ", 1) for line in stage[1].splitlines()))


def both_controllers(stage):
    return stage[0], stage[1] + on_controller(stage, "P2")[1]


def test_both_human_sides_replace_and_restore(tmp_path):
    boot = "".join(f"{f} START\n{f + 10} NONE\n" for f in range(60, 361, 60))
    boot += "560 A\n570 NONE\n"
    seed, _, _ = native(tmp_path / "open-game", 750, boot, flags=0)
    for name, frames, script in [
        ("player-menu", 300, "0 NONE\n20 A\n30 NONE\n"),
        ("one-versus-two", 300, "0 NONE\n20 RIGHT\n30 NONE\n60 A\n70 NONE\n"),
        ("p1-team", 400, "0 NONE\n20 A\n30 NONE\n"),
        ("p2-team", 400, "0 NONE\n20 P2 A\n30 P2 NONE\n"),
    ]:
        seed, _, _ = native(tmp_path / name, frames, script, seed=seed, flags=0)
    advance = "0 NONE\n0 P2 NONE\n"
    advance += "".join(f"{f} A\n{f + 10} NONE\n{f} P2 A\n{f + 10} P2 NONE\n"
                       for f in range(100, 2500, 150))
    seed, ram, _ = native(tmp_path / "two-player-live", 3500, advance, seed=seed, flags=0)
    assert word(ram, 0x70) == 8 and word(ram, 0xa8) == 0
    assert word(ram, 0x90) == word(ram, 0x92) == 1
    seed, ram, _ = native(tmp_path / "p2-request", 4000,
        "0 NONE\n0 P2 NONE\n40 P2 SELECT\n50 P2 NONE\n", seed=seed, flags=0)
    assert word(ram, 0x70) == 0x0c and word(ram, 0xe8) == 3
    seed, _, _ = native(tmp_path / "both-choose-squad", *both_controllers(OPEN_SQUAD),
                        seed=seed, flags=0)
    seed, ram, _ = native(tmp_path / "p1-substitute", *SUBSTITUTE, seed=seed, flags=0)
    assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
    seed, _, _ = native(tmp_path / "p1-exit-to-p2", *EXIT_SQUAD, seed=seed, flags=0)
    seed, ram = assert_replay(tmp_path / "p2-substitute-replay", seed, 0,
                              on_controller(SUBSTITUTE, "P2"))
    assert ram[0x3fa5] == 12 and ram[0x3fb0] == 0x41
    assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
    seed, _, _ = native(tmp_path / "p2-exit", *on_controller(EXIT_SQUAD, "P2"),
                        seed=seed, flags=0)
    seed, ram, _ = native(tmp_path / "both-resume", *both_controllers(RESUME),
                          seed=seed, flags=0)
    assert word(ram, 0x70) == 8
    assert ram[0x3f91] == ram[0x3fa5] == 12
    assert ram[0x3f9c] == ram[0x3fb0] == 0x81
    _, replay = assert_replay(tmp_path / "both-live-replay", seed, 0,
        (120, "0 RIGHT\n80 NONE\n0 P2 LEFT\n80 P2 NONE\n"))
    assert replay != ram
    assert replay[0x3f90:0x3fb8] == ram[0x3f90:0x3fb8]
