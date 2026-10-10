"""Read-only nine-match championship certificate for a continuous Cup probe."""
import argparse
import hashlib
import json
from pathlib import Path


def word(ram, address):
    return int.from_bytes(ram[address:address + 2], "little")


def checkpoint(path, expected_stage=None):
    data = path.read_bytes()
    assert data[:4] == b"ISCE" and len(data) >= 192, "Invalid native envelope"
    assert data[160:192] == hashlib.sha256(data[:160] + data[192:]).digest(), "Invalid envelope digest"
    assert int.from_bytes(data[12:16], "little") == 192 and int.from_bytes(data[16:24], "little") == len(data)-192
    assert data[192:196] == b"SLTR", "Unexpected native snapshot format"
    candidates = set()
    for callback in (0xa4beb6, 0x85c37c, 0x85d32e, 0x8bc8c0):
        needle = callback.to_bytes(3, "little")
        position = data.find(needle, 192)
        while position >= 0:
            offset = position - 0x1446
            if offset >= 200 and offset + 0x20000 <= len(data):
                ram = data[offset:offset + 0x20000]
                if (word(ram, 0x32) == 6 and word(ram, 0x70) == 12 and
                        word(ram, 0x1648) & 0x24 == 4 and
                        (word(ram, 0x1640) == expected_stage if expected_stage is not None else word(ram, 0x1640) <= 9) and
                        not word(ram, 0x1460) and not word(ram, 0x1462)):
                    candidates.add(offset)
            position = data.find(needle, position + 1)
    assert len(candidates) == 1, "Native campaign WRAM is not uniquely identifiable"
    offset = candidates.pop()
    ram = data[offset:offset + 0x20000]
    return dict(generation=int.from_bytes(data[24:32], "little"),
                label=data[80:144].split(b"\0")[0].decode("ascii"),
                stage=word(ram, 0x1640), round=word(ram, 0x1652),
                callback=f"{int.from_bytes(ram[0x1446:0x1449], 'little'):06x}",
                score=[word(ram, 0xda2) & 0x7f, word(ram, 0xea2) & 0x7f],
                raw_score=[word(ram, 0xda2), word(ram, 0xea2)],
                shootout_score=list(ram[0xd442:0xd444]),
                champion=ram[0xddce], player_team=word(ram, 0xda0),
                opponent=word(ram, 0xea0), sha256=hashlib.sha256(data).hexdigest(),
                file=path.name), ram


def certify(folder, continue_verification=None):
    outcome = json.loads((folder / "outcome.json").read_text())
    assert outcome["kind"] == "cup" and outcome["exit_code"] == 0
    assert outcome["one_process"] and outcome["save_loads"] == 0
    assert outcome["clocks_and_scores_edited"] is False
    assert outcome["campaign_outcome"] == "champion" and outcome["terminal_verified"]
    command = json.loads((folder / "command.json").read_text())
    forbidden = ("--continue", "--load-state", "--load-state-file",
                 "--import-password-symbols", "--import-password-at")
    assert not any(option.split("=", 1)[0] in forbidden for option in command), "Campaign did not start fresh"
    manifest = json.loads((folder / "build-manifest.json").read_text())
    assert manifest["input_driver_sha256"] == hashlib.sha256((folder / "probe_script.c").read_bytes()).hexdigest()
    assert manifest["exe_sha256"] == outcome["exe_sha256"]
    rows = [json.loads(line) for line in (folder / "progression.jsonl").read_text().splitlines()]
    assert all(a["frame"] < b["frame"] for a, b in zip(rows, rows[1:])), "Trace frames are not ordered"
    archives = sorted((folder / "observed-checkpoints").glob("*.sav"))
    assert len(archives) >= 10, "Require setup plus nine native result generations"
    states = [checkpoint(path) for path in archives]
    metadata = [state[0] for state in states]
    assert [state["generation"] for state in metadata] == list(range(1, len(metadata)+1)), "Missing native generation"
    assert [state["stage"] for state in metadata] == sorted(state["stage"] for state in metadata), "Native campaign stages moved backwards"
    assert metadata[0]["label"].startswith("Cup setup:") and metadata[0]["stage"] == 0
    results = [state for state in states if state[0]["label"].startswith(("Cup result:", "Cup complete:"))]
    assert {state[0]["stage"] for state in results} == set(range(1, 10)), "Require nine original committed result stages"
    assert all(state[0]["label"].startswith("Cup result:") and 1 <= state[0]["stage"] <= 8 for state in results[:-1])
    champion = results[-1][0]
    assert champion["label"].startswith("Cup complete:") and metadata[-1] == champion
    assert champion["champion"] == metadata[0]["player_team"], "Original champion is not the player team"
    assert champion["round"] == 0
    assert champion["callback"] in ("85d32e", "8bc8c0")
    for state, ram in states:
        if state["label"].startswith("Cup setup:"):
            assert word(ram, 0x1648) & 1 and state["round"] == 0 and state["callback"] == "a4beb6", "Invalid original phase setup"
            first_live = next((row["frame"] for row in rows if row["mode"] == 8 and row["live_stage"] == state["stage"]), None)
            assert first_live is not None, "Original phase setup has no played match"
            prior_end = max((row["frame"] for row in rows if row["mode"] == 18 and row["period"] >= 1 and
                             row["live_stage"] == state["stage"]-1 and row["frame"] < first_live), default=-1)
            assert any(prior_end < row["frame"] < first_live and row["mode"] == 12 and
                       row.get("stage") == state["stage"] and row.get("callback") == state["callback"] and
                       [score & 0x7f for score in row["score"]] == state["score"] for row in rows), "Unobserved original phase setup"
        else:
            assert state["label"].startswith(("Cup result:", "Cup complete:")), "Unexpected native checkpoint event"
    champion_ram = results[-1][1]
    assert word(champion_ram, 0x1648) & 8, "Final completion flag is missing"
    if champion["callback"] == "8bc8c0":
        assert word(champion_ram, 0xa8) == 3, "Shootout ceremony has the wrong period"
        left, right = champion["shootout_score"]
        assert left != right, "Shootout ceremony has no decisive winner"
        side = 0 if left > right else 2
        assert word(champion_ram, 0x1700) == side, "Original shootout winner side differs from kicks"
        assert champion_ram[0xda0 if side == 0 else 0xea0] == champion["champion"], "Shootout winner differs from champion"
    matches = []
    for stage in range(9):
        live = [row for row in rows if row["mode"] == 8 and row["live_stage"] == stage]
        assert live and live[0]["period"] == 0, f"Missing fresh first half at stage {stage}"
        assert any(row["period"] == 1 for row in live), f"Missing second half at stage {stage}"
        final_frame = live[-1]["frame"]
        next_frame = next((row["frame"] for row in rows if row["mode"] == 8 and row["live_stage"] == stage + 1), float("inf"))
        fulltime = [row for row in rows if final_frame < row["frame"] < next_frame and row["mode"] == 18]
        assert fulltime and fulltime[-1]["period"] >= 1, f"Missing original fulltime at stage {stage}"
        assert "clock" in fulltime[-1] and "display_clock" in fulltime[-1], "Missing original fulltime clocks"
        assert fulltime[-1]["clock"] == 0 and fulltime[-1]["display_clock"] == 0, "Fulltime clocks did not finish"
        boundaries = [state[0] for state in results if state[0]["stage"] == stage+1]
        result = boundaries[-1]
        # Original stage transitions reuse DA2/EA2 for menu state (notably
        # qualifying -> groups). Match goals therefore come from fulltime;
        # bind the checkpoint to the original settled callback independently.
        for boundary in boundaries:
            settled = [row for row in rows if final_frame < row["frame"] < next_frame and
                       row["mode"] == 12 and row.get("stage") == boundary["stage"] and
                       row.get("callback") == boundary["callback"] and
                       [score & 0x7f for score in row["score"]] == boundary["score"]]
            assert settled, f"Missing original committed checkpoint transition at stage {stage}"
        matches.append(dict(match=stage + 1, prior_stage=stage, result_stage=result["stage"],
                            opponent=live[0]["opponent"], fulltime_frame=fulltime[-1]["frame"],
                            score=[score & 0x7f for score in fulltime[-1]["score"]],
                            checkpoint_menu_score=result["score"], generation=result["generation"],
                            boundary_generations=[boundary["generation"] for boundary in boundaries]))
    final = (folder / "final.wram").read_bytes()
    assert len(final) == 0x20000 and word(final, 0x1640) == 9
    assert word(final, 0x32) == 6 and word(final, 0x70) == 12
    assert word(final, 0x1648) & 0x24 == 4
    assert final[0xddce] == metadata[0]["player_team"]
    certificate = dict(championship_verified=True, original_matches=matches,
                checkpoints=metadata, one_process=True, save_loads=0,
                clocks_and_scores_edited=False,
                continue_verification_required=True,
                driver_sha256=manifest["input_driver_sha256"], exe_sha256=outcome["exe_sha256"])
    if continue_verification is not None:
        verification = json.loads(continue_verification.read_text())
        assert verification["clones_only"] and verification["source_campaign_modified"] is False
        cases = verification["cases"]
        assert {case["checkpoint_sha256"] for case in cases} == {state["sha256"] for state in metadata}, "Continue did not verify every native generation"
        assert len(cases) == len(metadata), "Duplicate or missing Continue cases"
        assert all(case["continue_preserved_bytes"] and case["campaign_tables_settings_teams_preserved"] for case in cases)
        certificate.update(continue_verification_required=False, production_continue_verified=True,
                           continue_verification_sha256=hashlib.sha256(continue_verification.read_bytes()).hexdigest(),
                           production_continue_exe_sha256=verification["production_exe_sha256"])
    return certificate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--continue-verification", type=Path)
    opts = parser.parse_args()
    certificate = certify(opts.folder.resolve(), opts.continue_verification)
    (opts.folder / "winning-certificate.json").write_text(json.dumps(certificate, indent=2) + "\n")
    print(json.dumps(certificate, indent=2))


if __name__ == "__main__":
    main()
