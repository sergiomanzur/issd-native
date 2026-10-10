"""Live extra-time requests and a replacement cap across completed resumption.

Only prior-period clocks in owned snapshots are accelerated. Requests, stopping,
menus, replacement records, limits and resumption all run original guest code.
"""
import pytest

from team_changes_helpers import (EXE, ROM, bootstrap, native, word,
    REQUEST_MENU, SUBSTITUTE, EXIT_SQUAD, RESUME, ADVANCE_LIVE)
from test_game_acceptance import cup_final
from test_password_flow import password_screen
from test_substitution_flows import (OPEN_SQUAD, assert_replay,
                                    next_replacement, shorten_clock)

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                              reason="requires native build and user's retail ROM")


def test_live_extra_time_request_replaces_and_resumes_same_period(cup_final, tmp_path):
    folder, ram, advance = cup_final
    seed = (folder / "owned-saves/quicksave.sav").read_bytes()
    for period in range(2):
        seed, ram, _ = native(tmp_path / f"period-{period}-stats", 1000,
            seed=shorten_clock(seed, ram), flags=0)
        assert word(ram, 0x70) == 0x12
        seed, ram, _ = native(tmp_path / f"period-{period + 1}-resume", 3100,
            advance, seed=seed, flags=0)
        assert word(ram, 0x70) == 8 and word(ram, 0xa8) == period + 1
    original_opponent = ram[0x3fa4:0x3fb8]
    # No clock acceleration in the requested extra-time period itself.
    seed, ram, _ = native(tmp_path / "extra-time-select-request", *REQUEST_MENU,
                          seed=seed, flags=0)
    assert word(ram, 0x70) == 0x0c and word(ram, 0xa8) == 2
    assert word(ram, 0x16d0) > 0, "must enter at a live stoppage, not period end"
    assert word(ram, 0xe8) == 1 and word(ram, 0xdd0) & 0x4000
    for name, stage in [("squad", OPEN_SQUAD), ("substitute", SUBSTITUTE),
                        ("exit", EXIT_SQUAD), ("resume", RESUME), ("live", ADVANCE_LIVE)]:
        seed, ram, _ = native(tmp_path / name, *stage, seed=seed, flags=0)
        if name == "substitute":
            assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
    assert word(ram, 0x70) == 8 and word(ram, 0xa8) == 2
    assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x81
    assert ram[0x3fa4:0x3fb8] == original_opponent
    _, replay = assert_replay(tmp_path / "extra-time-live-replay", seed, 0,
                              (120, "0 RIGHT\n80 NONE\n"))
    assert replay != ram
    assert word(replay, 0xa8) == 2
    assert replay[0x3f90:0x3fb8] == ram[0x3f90:0x3fb8]


def test_completed_replacements_exhaust_next_stoppage_allowance(tmp_path):
    seed, _ = bootstrap(tmp_path / "bootstrap", flags=0)
    for name, stage in [("request", REQUEST_MENU), ("squad", OPEN_SQUAD),
                        ("first", SUBSTITUTE), ("second", next_replacement(2)),
                        ("third", next_replacement(3)), ("exit", EXIT_SQUAD),
                        ("resume", RESUME), ("live", ADVANCE_LIVE)]:
        seed, ram, _ = native(tmp_path / name, *stage, seed=seed, flags=0)
    assert word(ram, 0x70) == 8 and word(ram, 0xbc) == 0
    assert [ram[0x3f9c], ram[0x3f9e], ram[0x3fa1]] == [0x81, 0x83, 0x85]
    completed = ram[0x3f90:0x3fb8]
    seed, ram, _ = native(tmp_path / "next-request", *REQUEST_MENU, seed=seed, flags=0)
    assert word(ram, 0x70) == 0x0c and word(ram, 0xe8) == 1
    seed, ram, _ = native(tmp_path / "next-squad", *OPEN_SQUAD, seed=seed, flags=0)
    # Bench12 is departed (81); select the next, untouched outfielder at13.
    assert ram[0x3f9d] == 13
    attempt = (500, "0 NONE\n20 DOWN\n30 NONE\n60 A\n70 NONE\n100 LEFT\n110 NONE\n"
                    "180 DOWN\n190 NONE\n220 DOWN\n230 NONE\n260 A\n270 NONE\n")
    _, ram = assert_replay(tmp_path / "unused-fourth-rejected", seed, 0, attempt)
    # CODE_86A4EC validates selected pair $48/$4A. CODE_86A5C1 counts
    # previous outfield replacements ($A4-$A6) and rejects at three.
    # The original Squad task uses DP=$1500; prove candidate13 is unused,
    # rather than accidentally testing the departed player81 rejection.
    assert word(ram, 0x1528) == 0x3f90
    assert word(ram, 0x1548) == 13 and word(ram, 0x154a) == 1
    assert word(ram, 0x15c8) == 13 and word(ram, 0x15ca) == 12
    assert word(ram, 0x15a4) == 3 and word(ram, 0x15a6) == 0
    assert ram[0x3f9d] == 13
    assert ram[0x3f90:0x3fb8] == completed
    assert word(ram, 0x1e6c) == 0xffff
