"""Complete cartridge shootout; clocks/scores accelerate only pre-shootout setup."""
import hashlib
import json

import pytest
from test_game_acceptance import cup_final, shorten_clock, run, word
from test_password_flow import password_screen, guest, EXE, ROM

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


def kicks(frames, offset=0):
    """Ordinary released B presses; no kick counters/results/state writes."""
    return "0 NONE\n" + "".join(
        f"{f-offset} B\n{f-offset+20} NONE\n"
        for f in range(30, offset + frames, 120) if f >= offset)


def test_drawn_cup_final_completes_original_shootout_and_continue(cup_final):
    folder, current, advance = cup_final
    campaign = next((folder / "owned-saves/campaigns").glob("*/campaign.sav"))
    imported = campaign.read_bytes()
    for period in range(4):
        shorten_clock(folder, current, score=(0, 0))
        stats = run(folder, f"shootout-period-{period}-stats", 1000)
        assert word(stats, 0x70) == 0x12
        current = run(folder, f"shootout-period-{period}-resume", 3100, advance)
        assert word(current, 0x70) == (8 if period < 3 else 0x0c)
        assert word(current, 0xa8) == min(period + 1, 3)
    assert campaign.read_bytes() == imported, "uncommitted draw autosaved"
    ready = run(folder, "shootout-ready", 90)
    assert word(ready, 0x1704) < 5
    assert word(ready, 0x1700) == 0xffff

    midway = run(folder, "shootout-midway", 1500, kicks(1500))
    assert word(midway, 0x70) == 0x0c
    assert word(midway, 0x1704) > 0, "ordinary shot input did not advance rounds"
    assert campaign.read_bytes() == imported
    snapshot = (folder / "owned-saves/quicksave.sav").read_bytes()
    finished = run(folder, "shootout-finished", 8500, kicks(8500, 1500))
    assert word(finished, 0x1640) == 9, "ROM did not leave final after the kicks"
    assert finished[0x1446:0x1449] == bytes.fromhex("c0c88b")
    scores = tuple(finished[0xd442:0xd444])
    assert scores[0] != scores[1], "shootout did not produce a winner"
    assert (word(finished, 0xda2), word(finished, 0xea2)) == (0, 0)
    winner = finished[0xda0 if scores[0] > scores[1] else 0xea0]
    assert finished[0xddce] == winner, "shootout winner did not become Cup champion"

    # A true mid-shootout snapshot resumes in another process and reproduces
    # the full guest state, including winner/callback, under identical inputs.
    replay_folder = folder / "replay"
    (replay_folder / "owned-saves").mkdir(parents=True)
    (replay_folder / "owned-saves/quicksave.sav").write_bytes(snapshot)
    config = folder / "isolated.cfg"
    if config.exists():
        (replay_folder / "isolated.cfg").write_bytes(config.read_bytes())
    replayed = run(replay_folder, "shootout-replayed", 8500, kicks(8500, 1500))
    assert replayed == finished
    before = int.from_bytes(imported[24:32], "little")
    terminal_saved = campaign.read_bytes()
    assert int.from_bytes(terminal_saved[24:32], "little") == before + 1
    completed = run(folder, "shootout-completed", 550, "0 NONE\n200 A\n210 NONE\n", save=False)
    assert word(completed, 0x70) == 0x1a, "original terminal exit did not return to title"
    saved = campaign.read_bytes()
    assert saved == terminal_saved, "terminal exit rewrote completion"
    assert saved[80:144].split(b"\0")[0].startswith(b"Cup complete: ")
    assert saved[160:192] == hashlib.sha256(saved[:160] + saved[192:]).digest()
    guest(folder, 80, "--continue", "--dump-state", str(folder / "shootout-continued"))
    continued = (folder / "shootout-continued.wram").read_bytes()
    assert continued[0x1640:0x1698] == finished[0x1640:0x1698]
    assert continued[0xdc00:0xdeff] == finished[0xdc00:0xdeff]
    assert campaign.read_bytes() == saved
    (folder / "shootout-evidence.json").write_text(json.dumps({
        "pre_shootout_clock_and_score_fixture": True,
        "shootout_guest_state_writes": False, "penalty_scores": scores,
        "winner_team": winner, "terminal_callback": finished[0x1446:0x1449].hex(),
        "mid_shootout_snapshot_replay_equal": replayed == finished,
        "completion_label": saved[80:144].split(b"\0")[0].decode(),
        "exe_sha256": hashlib.sha256(EXE.read_bytes()).hexdigest(),
        "rom_sha256": hashlib.sha256(ROM.read_bytes()).hexdigest(),
    }, indent=2))
