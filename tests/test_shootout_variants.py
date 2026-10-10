"""Original sudden-death selection and kicks; pre-shootout clocks/scores are adjusted."""
import hashlib
import json
import pytest

from test_game_acceptance import cup_final, shorten_clock, run, word
from test_password_flow import password_screen, guest, EXE, ROM

pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")


def right_kicks(frames, start=30):
    return "0 RIGHT\n" + "".join(
        f"{f} RIGHT,B\n{f+20} RIGHT\n" for f in range(start, frames, 120))


def test_original_sudden_death_kicker_selection_completion_and_continue(cup_final):
    folder, current, advance = cup_final
    campaign = next((folder / "owned-saves/campaigns").glob("*/campaign.sav"))
    imported = campaign.read_bytes()
    for period in range(4):
        shorten_clock(folder, current, score=(0, 0))
        stats = run(folder, f"variant-period-{period}-stats", 1000)
        assert word(stats, 0x70) == 0x12
        current = run(folder, f"variant-period-{period}-resume", 3100, advance)
        assert word(current, 0x70) == (8 if period < 3 else 0x0c)
        assert word(current, 0xa8) == min(period + 1, 3)
    ready = run(folder, "variant-ready", 90)
    # Explicitly retain the captured original mid-shootout envelope: later
    # quicksaves overwrite their slot, so an artifact claim needs its own file.
    (folder / "mid-shootout.sav").write_bytes((folder / "owned-saves/quicksave.sav").read_bytes())
    assert word(ready, 0x1704) == 2
    tied = run(folder, "variant-sudden-death", 4000, right_kicks(4000))
    assert tuple(tied[0xd442:0xd444]) == (3, 3)
    assert word(tied, 0x1704) == 5 and word(tied, 0x170e) == 1
    assert word(tied, 0x1700) == 0xffff and word(tied, 0x1640) == 8
    assert campaign.read_bytes() == imported, "tied sudden-death entry autosaved completion"
    (folder / "sudden-death.sav").write_bytes((folder / "owned-saves/quicksave.sav").read_bytes())

    # Original selection skips used kickers. A selects the unused taker and
    # a separate A confirms. Only then resume actual aimed shot/keeper input.
    choose = "0 NONE\n" + "".join(f"{f} DOWN\n{f+10} NONE\n" for f in (30, 60, 90, 120, 150))
    choose += "250 A\n260 NONE\n700 A\n710 NONE\n1200 RIGHT\n"
    choose += "".join(f"{f} RIGHT,B\n{f+20} RIGHT\n" for f in range(1230, 4000, 120))
    result = run(folder, "variant-sixth-kicks", 4000, choose)
    assert word(result, 0x1640) == 9, "original sixth-round inputs did not finish sudden death"
    assert word(result, 0x1704) >= 5 and word(result, 0x170e) == 1
    # Release all controls before checking the settled original ceremony.
    finished = run(folder, "variant-finished", 1000)
    assert finished[0x1446:0x1449] == bytes.fromhex("c0c88b")
    scores = tuple(finished[0xd442:0xd444])
    assert scores[0] != scores[1]
    side = 0 if scores[0] > scores[1] else 2
    assert word(finished, 0x1700) == side
    winner = finished[0xda0 if side == 0 else 0xea0]
    assert finished[0xddce] == winner
    saved = campaign.read_bytes()
    assert int.from_bytes(saved[24:32], "little") == int.from_bytes(imported[24:32], "little") + 1
    assert saved[80:144].split(b"\0")[0].startswith(b"Cup complete: ")
    assert saved[160:192] == hashlib.sha256(saved[:160] + saved[192:]).digest()
    guest(folder, 80, "--continue", "--dump-state", str(folder / "variant-continued"))
    continued = (folder / "variant-continued.wram").read_bytes()
    assert continued[0x1640:0x1698] == finished[0x1640:0x1698]
    assert continued[0xdc00:0xdeff] == finished[0xdc00:0xdeff]
    assert campaign.read_bytes() == saved
    (folder / "variant-evidence.json").write_text(json.dumps({
        "pre_shootout_clock_and_score_fixture": True, "shootout_state_writes": False,
        "tied_five_round_scores": list(tied[0xd442:0xd444]),
        "sudden_death_round": word(finished, 0x1704), "penalty_scores": scores,
        "winner_team": winner, "completion_label": saved[80:144].split(b"\0")[0].decode(),
        "exe_sha256": hashlib.sha256(EXE.read_bytes()).hexdigest(),
        "rom_sha256": hashlib.sha256(ROM.read_bytes()).hexdigest(),
        "retained_snapshots": ["mid-shootout.sav", "sudden-death.sav"],
    }, indent=2))
