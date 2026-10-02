"""Real input requests, management commits, actor refresh and restored replay.

No guest memory, callbacks, clock, scores or roster data are seeded or edited.
The first team is human controlled; the original CPU opponent remains intact.
"""
import re

import pytest
from team_changes_helpers import (EXE, ROM, word, native, bootstrap, REQUEST_MENU,
    OPEN_FORMATION, CHANGE_FORMATION, EXIT_FORMATION, OPEN_SQUAD_AFTER_FORMATION,
    SUBSTITUTE, EXIT_SQUAD, RESUME, ADVANCE_LIVE)

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                               reason="requires native build and user's retail ROM")


def signed_byte(value):
    return value if value < 128 else value - 256


def test_original_select_request_can_be_cancelled(tmp_path):
    seed, _ = bootstrap(tmp_path / "bootstrap", flags=0)
    seed, _, _ = native(tmp_path / "throw-in", *ADVANCE_LIVE, seed=seed, flags=0)
    _, ram, _ = native(tmp_path / "cancel", 1800,
        "0 NONE\n10 SELECT\n15 NONE\n20 SELECT\n25 NONE\n", seed=seed, flags=0)
    assert word(ram, 0xdd0) & 0x4000 == 0
    assert word(ram, 0x70) != 0x0c, "cancelled request entered original management"
    assert word(ram, 0xe8) != 3


@pytest.mark.parametrize("flags", [0, 3], ids=["original-ai", "both-policies"])
def test_original_management_commits_and_replays(tmp_path, flags):
    seed, initial = bootstrap(tmp_path / "bootstrap", flags)
    original_lineup = initial[0x3f90:0x3fa4]
    original_formation = word(initial, 0xda6)
    for name, stage in (("request", REQUEST_MENU), ("formation", OPEN_FORMATION),
                        ("choose-formation", CHANGE_FORMATION), ("commit-formation", EXIT_FORMATION),
                        ("squad", OPEN_SQUAD_AFTER_FORMATION), ("substitute", SUBSTITUTE),
                        ("commit-squad", EXIT_SQUAD), ("resume", RESUME), ("live", ADVANCE_LIVE)):
        seed, ram, result = native(tmp_path / name, *stage, seed=seed, flags=flags, trace=name == "live")
        if name == "request":
            assert word(ram, 0x70) == 0x0c and word(ram, 0x1408) == 1
            assert word(ram, 0xe8) == 3, "original requested management context missing"
        if name == "choose-formation":
            assert word(ram, 0xda6) == original_formation
            assert word(ram, 0x15f6) == original_formation + 1
        if name == "commit-formation":
            assert word(ram, 0xda6) == original_formation + 1
        if name == "substitute":
            assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x41
            assert word(ram, 0x1e6c) == 0x0c01
        if name == "live":
            assert word(ram, 0x70) == 8 and word(ram, 0xbc) == 0
            assert ram[0x3f91] == 12 and ram[0x3f9c] == 0x81
            assert ram[0x3fa4:0x3fb8] == initial[0x3fa4:0x3fb8]
            assert ram[0x3f90:0x3fa4] != original_lineup
            assert word(ram, 0xda6) == original_formation + 1
            assert (ram[0x662], ram[0x666], ram[0x667]) == (7, 2, 8)
            # The original refresh installs the changed formation on active actors.
            for slot in range(1, 11):
                actor = 0x500 + slot * 0x100
                pos = 0xd000 + word(ram, 0xda6) * 20 + (slot - 1) * 2
                assert word(ram, actor + 0x68) == slot
                assert word(ram, actor + 0x9a) == 0xd00
                assert ram[actor + 0x31] == ram[0xd280 + word(ram, 0xda6) * 10 + slot - 1]
                assert word(ram, actor + 0x8c) == (signed_byte(ram[pos]) * 8 & 0xffff)
                assert word(ram, actor + 0x8e) == (signed_byte(ram[pos + 1]) * 4 & 0xffff)
            stats = re.search(r"\[Gameplay\] goalkeeper=(\d+)/(\d+) player=(\d+)/(\d+)", result.stdout + result.stderr)
            assert stats, result.stdout + result.stderr
            counts = tuple(map(int, stats.groups()))
            if flags:
                assert counts[2] > 0
                samples = re.findall(
                    r"\[GameplayPlayer\] D=([0-9A-F]{4}) .*?target=(\d+)/(\d+) updated=(\d+)/(\d+) "
                    r"applied=1 roster=(\d+) formation=(\d+) condition=(\d+) role=(\d+) "
                    r"speed=(\d+) inverse=(\d+) skill=(\d+)", result.stderr)
                assert samples, "No changed eligible original CPU AI decision after management"
                # This route witnesses eligible CPU opponent decisions. The
                # substituted human team's off-ball policy decision is not
                # asserted; its original actor refresh is checked above.
                for sample in samples:
                    actor = int(sample[0], 16)
                    x, y, updated_x, updated_y, roster, formation, condition, role, speed, inverse, skill = map(int, sample[1:])
                    assert 0x1100 <= actor <= 0x1a00
                    assert formation == word(ram, 0xea6)
                    assert roster == ram[0x3fa4 + (actor - 0x1000) // 0x100] & 31
                    assert (x, y) != (updated_x, updated_y)
                    assert abs(updated_x - x) <= 8 + speed * 2
                    assert abs(updated_y - y) <= 8 + speed * 2
            else:
                assert counts == (0, 0, 0, 0)
    # Identical guest snapshots and controller input must replay moving live play.
    first_seed, first_ram, _ = native(tmp_path / "first", 120, "0 RIGHT\n80 NONE\n", seed=seed, flags=flags)
    second_seed, second_ram, _ = native(tmp_path / "replay", 120, "0 RIGHT\n80 NONE\n", seed=seed, flags=flags)
    assert first_ram == second_ram
    # The envelope includes publication time and its hash; the complete guest
    # payload must match, including CPU/PPU/controller and transient state.
    header = int.from_bytes(first_seed[12:16], "little")
    assert header == int.from_bytes(second_seed[12:16], "little") == 192
    assert first_seed[header:] == second_seed[header:]
    assert first_ram != ram, "restored match did not advance"
    assert (tmp_path / "first/final.bmp").read_bytes() == (tmp_path / "replay/final.bmp").read_bytes()
