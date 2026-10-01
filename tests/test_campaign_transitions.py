import subprocess
import os
import pytest
from pathlib import Path
from test_config_persistence import compile_c


def test_campaign_transitions(tmp_path):
    root = Path(__file__).resolve().parents[1]
    executable = tmp_path / "campaign.exe"
    compile_c(executable, root,
              [root / "tests/test_campaign_transitions.c",
               root / "ISSDNative/issd_campaign.c"], [root / "ISSDNative"])
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
GAME = ROOT / "build/ISSDNative.exe"


@pytest.mark.skipif(not ROM.exists() or not GAME.exists(),
                    reason="requires user's cartridge ROM and native build")
@pytest.mark.parametrize("kind,frames", [("Cup", (941, 1120, 1121)),
                                        ("World Series", (1121, 1300, 1301))])
def test_cartridge_setup_transition(tmp_path, kind, frames):
    # Captures come from unmodified guest execution, not manufactured WRAM.
    # Assets and the ROM are not distributed with the tests.
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n"
    if kind == "World Series":
        script += "530 DOWN\n540 NONE\n"
    script += "560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1600, 100))
    (tmp_path / "input.txt").write_text(script)
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy")
    captures = []
    for frame in frames:
        prefix = tmp_path / f"frame{frame}"
        result = subprocess.run(
            [str(GAME), "--headless", str(frame), "--script", str(tmp_path / "input.txt"),
             "--dump-state", str(prefix), "--config", str(tmp_path / "config.ini"),
             "--save-dir", str(tmp_path / "saves"), "--rom", str(ROM)],
            cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "[interp_cap]" not in result.stderr
        captures.append(Path(str(prefix) + ".wram"))
    executable = tmp_path / "observer.exe"
    compile_c(executable, ROOT, [ROOT / "tests/test_campaign_transitions.c",
                                ROOT / "ISSDNative/issd_campaign.c"], [ROOT / "ISSDNative"])
    label = "International Cup setup" if kind == "Cup" else "World Series setup"
    result = subprocess.run([str(executable), label, *(str(p) for p in captures)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def _run_guest(folder, script, frames, prefix, *, load=False, save=False, continued=False):
    input_path = folder / "fixture-input.txt"
    input_path.write_text(script, encoding="ascii")
    args = [str(GAME), "--headless", str(frames), "--script", str(input_path),
            "--dump-state", str(folder / prefix), "--config", str(folder / "fixture.cfg"),
            "--save-dir", str(folder / "saves"), "--rom", str(ROM)]
    if load:
        args += ["--load-state", "20"]
    if save:
        args += ["--save-state", str(frames - 1)]
    if continued:
        args += ["--continue"]
    result = subprocess.run(args, cwd=folder,
                            env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"),
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "[interp_cap]" not in result.stderr
    return (folder / (prefix + ".wram")).read_bytes()


def _accelerate_private_match(folder, ram, edits):
    # This only edits a test-owned quicksave. Original match-end and standings
    # code must still execute. The completion fixture additionally seeds the
    # verified backed-up round counter; it never injects a committed outcome.
    import hashlib
    path = folder / "saves/quicksave.sav"
    data = bytearray(path.read_bytes())
    assert data[:4] == b"ISCE"
    needle = ram[0xdc00:0xdd00]
    offset = data.find(needle) - 0xdc00
    assert offset > 192 and data.find(needle, offset + 0xdc00 + 1) == -1
    for address, value in edits:
        data[offset + address:offset + address + 2] = value.to_bytes(2, "little")
    data[160:192] = hashlib.sha256(data[:160] + data[192:]).digest()
    path.write_bytes(data)


def _seed_cup_stage(folder, base, fulltime, campaign, stage, round_number, *, semifinal=False, final=False):
    import hashlib
    data = bytearray(base)
    offset = data.find(fulltime[0xdc00:0xdd00]) - 0xdc00
    assert offset > 192
    data[offset + 0xdc00:offset + 0xdf00] = campaign[0xdc00:0xdf00]
    if final:
        data[offset + 0xddff:offset + 0xde5f] = campaign[0x1640:0x16a0]
        opponent = int.from_bytes(campaign[0x0ea0:0x0ea2], "little")
    else:
        opponent = campaign[0xddb1] if semifinal else campaign[0xdc03]
    if semifinal:
        # Original $8B:F583 points round 1 to DDC8, round 0 to DDCC.
        # Seed the semifinal entrants from the cartridge's initialized bracket;
        # $85:DA5C then determines and writes both finalists itself.
        selected = int.from_bytes(fulltime[0x0da0:0x0da2], "little")
        opponents = [team for team in campaign[0xddb0:0xddc0] if team != selected]
        assert selected in campaign[0xddb0:0xddc0] and len(opponents) >= 3
        entrants = bytes([selected, *opponents[:3]])
        data[offset + 0xddc8:offset + 0xddcc] = entrants
        opponent = entrants[1]
        # Normalize the test entrant to the first semifinal slot. The original
        # draw can place the selected team anywhere in the 16-team bracket.
        data[offset + 0xde1d:offset + 0xde21] = b"\0\0\0\0"
    for address, value in [(0xddff, stage), (0xde11, round_number),
                           (0x0ea0, opponent), (0x0da2, 2), (0x0ea2, 0)]:
        data[offset + address:offset + address + 2] = value.to_bytes(2, "little")
    data[160:192] = hashlib.sha256(data[:160] + data[192:]).digest()
    (folder / "saves/quicksave.sav").write_bytes(data)


@pytest.mark.skipif(not ROM.exists() or not GAME.exists(),
                    reason="requires user's cartridge ROM and native build")
@pytest.mark.parametrize("world", [False, True], ids=["cup-result", "world-result"])
def test_cartridge_result_autosaves_and_continues(tmp_path, world):
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n"
    if world:
        script += "530 DOWN\n540 NONE\n"
    script += "560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1600, 100))
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(1800, 2900, 200))
    live = _run_guest(tmp_path, script, 3101, "live", save=True)
    assert int.from_bytes(live[0x70:0x72], "little") == 8
    _accelerate_private_match(tmp_path, live, [(0x16d0, 1), (0x16d2, 0)])
    halftime = _run_guest(tmp_path, "0 NONE\n", 1000, "halftime", load=True, save=True)
    assert int.from_bytes(halftime[0x70:0x72], "little") == 0x12
    setup_save = next((tmp_path / "saves/campaigns").glob("*/campaign.sav"))
    assert int.from_bytes(setup_save.read_bytes()[24:32], "little") == 1
    advance = "0 NONE\n" + "".join(f"{f} A\n{f+10} NONE\n" for f in range(200, 1700, 150))
    second = _run_guest(tmp_path, advance, 1900, "second", load=True, save=True)
    assert int.from_bytes(second[0x70:0x72], "little") == 8
    _accelerate_private_match(tmp_path, second, [(0x16d0, 1), (0x16d2, 0), (0x0da2, 2)])
    fulltime = _run_guest(tmp_path, "0 NONE\n", 1000, "fulltime", load=True, save=True)
    assert int.from_bytes(fulltime[0x70:0x72], "little") == 0x12
    fulltime_snapshot = (tmp_path / "saves/quicksave.sav").read_bytes()
    assert int.from_bytes(setup_save.read_bytes()[24:32], "little") == 1
    committed = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                           "committed", load=True)
    assert int.from_bytes(committed[0x1652:0x1654], "little") == 1
    saved = setup_save.read_bytes()
    assert int.from_bytes(saved[24:32], "little") == 2, "result did not save exactly once"
    label = saved[80:144].split(b"\0")[0]
    assert label == (b"World Series result: Brazil" if world else b"Cup result: Brazil")
    restored = _run_guest(tmp_path, "0 NONE\n", 80, "continued", continued=True)
    assert restored[0xdc00:0xdeff] == committed[0xdc00:0xdeff]
    assert restored[0x1640:0x1698] == committed[0x1640:0x1698]
    assert setup_save.read_bytes() == saved, "Continue immediately resaved"
    if world:
        # $8B:9A2B advances $1652 up to 35. Seed round 34 in the private
        # pre-commit match snapshot, then let that original routine commit 35.
        _accelerate_private_match(tmp_path, fulltime, [(0xde11, 34)])
        last_result = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                                 "last-result", load=True, save=True)
        assert int.from_bytes(last_result[0x1652:0x1654], "little") == 35
        completed = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                               "completed", load=True)
        assert completed[0x1446:0x1449] == bytes.fromhex("ee948b")
        final_save = setup_save.read_bytes()
        assert int.from_bytes(final_save[24:32], "little") == 4
        assert final_save[80:144].split(b"\0")[0] == b"World Series complete: Brazil"
        final_restore = _run_guest(tmp_path, "0 NONE\n", 80, "final-continued", continued=True)
        assert final_restore[0x1640:0x1698] == completed[0x1640:0x1698]
        assert final_restore[0xdc00:0xdeff] == completed[0xdc00:0xdeff]
        assert setup_save.read_bytes() == final_save
    else:
        # Trace the whole Cup phase structure while shortening private prior
        # progress. Each outcome and each next-stage constructor still executes
        # original cartridge code; no committed callbacks/outcomes are injected.
        _accelerate_private_match(tmp_path, fulltime, [(0xddff, 1), (0xde11, 1)])
        qualified = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                               "qualified", load=True, save=True)
        assert qualified[0x1648:0x164a] == bytes.fromhex("0542")
        group = _run_guest(tmp_path, advance, 1700, "group", load=True, save=True)
        assert group[0xde07:0xde09] == bytes.fromhex("0500")
        _seed_cup_stage(tmp_path, fulltime_snapshot, fulltime, group, 4, 2)
        group_result = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                                  "group-result", load=True, save=True)
        assert group_result[0x1640:0x1642] == bytes.fromhex("0500")
        knockout = _run_guest(tmp_path, advance, 1700, "knockout", load=True, save=True)
        assert knockout[0xde07:0xde09] == bytes.fromhex("0c40")
        _seed_cup_stage(tmp_path, fulltime_snapshot, fulltime, knockout, 7, 1, semifinal=True)
        semifinal = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                               "semifinal", load=True, save=True)
        assert semifinal[0x1640:0x1642] == bytes.fromhex("0800")
        assert semifinal[0x1446:0x1449] == bytes.fromhex("2ed385")
        _seed_cup_stage(tmp_path, fulltime_snapshot, fulltime, semifinal, 8, 0, final=True)
        ceremony = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                              "ceremony", load=True, save=True)
        assert ceremony[0x1640:0x1642] == bytes.fromhex("0900")
        assert ceremony[0xddce] == 0x3c, "original final routine did not commit Brazil champion"
        terminal = _run_guest(tmp_path, "0 NONE\n", 3000, "terminal", load=True, save=True)
        assert terminal[0x1446:0x1449] == bytes.fromhex("c0c88b")
        before_final = int.from_bytes(setup_save.read_bytes()[24:32], "little")
        completed = _run_guest(tmp_path, "0 NONE\n200 A\n210 NONE\n", 550,
                               "cup-completed", load=True)
        final_save = setup_save.read_bytes()
        assert completed[0x1446:0x1449] == bytes.fromhex("2ed385")
        assert int.from_bytes(final_save[24:32], "little") == before_final + 1
        assert final_save[80:144].split(b"\0")[0] == b"Cup complete: Brazil"
        final_restore = _run_guest(tmp_path, "0 NONE\n", 80, "final-continued", continued=True)
        assert final_restore[0x1640:0x1698] == completed[0x1640:0x1698]
        assert final_restore[0xdc00:0xdeff] == completed[0xdc00:0xdeff]
        assert setup_save.read_bytes() == final_save
