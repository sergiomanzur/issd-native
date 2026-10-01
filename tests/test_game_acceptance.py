"""Bounded retail-ROM acceptance, not certification of a whole played campaign.

A captured, cartridge-generated semifinal password selects the final through
the original Password screen. Only the test-owned match clock and score are
shortened: period, progression, callbacks and champion are written by the ROM.
No ROM or full game snapshot is checked into this repository.
"""
import hashlib
import json
import struct

import pytest
from test_password_flow import password_screen, guest, ROOT, ROM, EXE

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


def word(ram, address):
    return struct.unpack_from("<H", ram, address)[0]


def run(folder, name, frames, script="0 NONE\n", *, load=True, save=True):
    path = folder / "input.txt"
    path.write_text(script, encoding="ascii")
    args = ["--script", str(path), "--dump-state", str(folder / name)]
    if load:
        args += ["--load-state", "1"]
    if save:
        args += ["--save-state", str(frames)]
    result = guest(folder, frames, *args)
    assert "[interp_cap]" not in result.stderr
    return (folder / (name + ".wram")).read_bytes()


def shorten_clock(folder, ram, *, score=None):
    path = folder / "owned-saves/quicksave.sav"
    data = bytearray(path.read_bytes())
    offset = data.find(ram)
    assert data[:4] == b"ISCE" and offset >= 192
    assert data.find(ram, offset + 1) == -1
    edits = [(0x16d0, 1), (0x16d2, 0)]
    if score is not None:
        edits += [(0xda2, score[0]), (0xea2, score[1])]
    for address, value in edits:
        struct.pack_into("<H", data, offset + address, value)
    data[160:192] = hashlib.sha256(data[:160] + data[192:]).digest()
    path.write_bytes(data)


@pytest.fixture
def cup_final(password_screen, tmp_path):
    envelope, _ = password_screen
    saves = tmp_path / "owned-saves"
    saves.mkdir()
    (saves / "quicksave.sav").write_bytes(envelope)
    fixture = json.loads((ROOT / "tests/fixtures/password/campaign_states.json").read_text())
    symbols = tmp_path / "semifinal-password.bin"
    symbols.write_bytes(bytes.fromhex(fixture["cupsemicommitted"]["symbols"]))
    guest(tmp_path, 600, "--load-state", "1", "--import-password-at", "2",
          "--import-password-symbols", str(symbols), "--save-state", "600",
          "--dump-state", str(tmp_path / "imported"))
    imported = (tmp_path / "imported.wram").read_bytes()
    assert word(imported, 0x1640) == 8
    assert imported[0x1446:0x1449] == bytes.fromhex("2ed385")
    advance = "0 NONE\n" + "".join(f"{f} A\n{f+10} NONE\n" for f in range(200, 1600, 150))
    advance += "1800 B\n1810 NONE\n2200 B\n2210 NONE\n"
    first = run(tmp_path, "final-first-half", 3100, advance)
    assert word(first, 0x70) == 8 and word(first, 0xa8) == 0
    assert word(first, 0xddff) == 8, "original import did not select the Cup final"
    return tmp_path, first, advance


def test_password_to_original_cup_final_and_continue(cup_final):
    folder, first, advance = cup_final
    shorten_clock(folder, first)
    halftime = run(folder, "halftime", 1000)
    assert word(halftime, 0x70) == 0x12
    second = run(folder, "second-half", 3100, advance)
    assert word(second, 0x70) == 8 and word(second, 0xa8) == 1
    # A deterministic winning score is a fixture, not a claim that a bot won.
    shorten_clock(folder, second, score=(2, 0))
    fulltime = run(folder, "fulltime", 1000)
    assert word(fulltime, 0x70) == 0x12, [(hex(a), word(fulltime, a)) for a in (0xa8, 0x16d0, 0x16d2, 0x14d6, 0x17bc)]
    ceremony = run(folder, "ceremony", 550, "0 NONE\n200 A\n210 NONE\n")
    assert word(ceremony, 0x1640) == 9
    assert ceremony[0xddce] == ceremony[0xda0], "ROM did not write the winning champion"
    terminal = run(folder, "terminal", 3000)
    assert terminal[0x1446:0x1449] == bytes.fromhex("c0c88b")
    campaign = next((folder / "owned-saves/campaigns").glob("*/campaign.sav"))
    before = int.from_bytes(campaign.read_bytes()[24:32], "little")
    completed = run(folder, "completed", 550, "0 NONE\n200 A\n210 NONE\n", save=False)
    saved = campaign.read_bytes()
    assert int.from_bytes(saved[24:32], "little") == before + 1
    assert saved[80:144].split(b"\0")[0] == b"Cup complete: Brazil"
    guest(folder, 80, "--continue", "--dump-state", str(folder / "continued"))
    continued = (folder / "continued.wram").read_bytes()
    assert continued[0x1640:0x1698] == completed[0x1640:0x1698]
    assert continued[0xdc00:0xdeff] == completed[0xdc00:0xdeff]
    assert campaign.read_bytes() == saved


def test_drawn_final_enters_original_extra_time(cup_final):
    folder, current, advance = cup_final
    # Each original period transition must run; only clocks/scores are fixtures.
    for expected_period in (1, 2, 3):
        shorten_clock(folder, current, score=(0, 0))
        stats = run(folder, f"period-{expected_period}-stats", 1000)
        assert word(stats, 0x70) == 0x12
        current = run(folder, f"period-{expected_period}-live", 3100, advance)
        assert word(current, 0x70) == 8
        assert word(current, 0xa8) == expected_period
    # Actual third and fourth periods establish extra-time flow. Shootout
    # completion, normal elapsed clocks, and physical input remain manual checks.
    campaign = next((folder / "owned-saves/campaigns").glob("*/campaign.sav"))
    assert int.from_bytes(campaign.read_bytes()[24:32], "little") == 1, "uncommitted drawn period autosaved"
