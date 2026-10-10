"""Reject incomplete or mislabeled winning evidence without launching a game."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

import pytest

MODULE = Path(__file__).resolve().parents[1] / "tools/ghidra/cup_controller_certificate.py"
SPEC = importlib.util.spec_from_file_location("cup_controller_certificate", MODULE)
CERT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CERT)


@pytest.fixture
def evidence(tmp_path):
    archive = tmp_path / "observed-checkpoints"
    archive.mkdir()
    final = None
    for stage in range(10):
        ram = bytearray(0x20000)
        for address, value in ((0x32, 6), (0x70, 12), (0x1640, stage),
                               (0x1648, 12 if stage == 9 else 5 if stage == 0 else 4), (0xda0, 60), (0xea0, 62),
                               (0xda2, 2 if stage else 0)):
            struct.pack_into("<H", ram, address, value)
        ram[0x1446:0x1449] = (0xa4beb6 if stage == 0 else 0x85d32e if stage == 9 else 0x85c37c).to_bytes(3, "little")
        if stage == 9:
            ram[0xddce] = 60
        data = bytearray(512) + ram
        data[:4] = b"ISCE"
        struct.pack_into("<IQQ", data, 12, 192, len(data)-192, stage+1)
        data[192:196] = b"SLTR"
        label = f"Cup {'setup' if stage == 0 else 'complete' if stage == 9 else 'result'}: Brazil".encode()
        data[80:80+len(label)] = label
        data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
        (archive / f"campaign-{stage+1:03}.sav").write_bytes(data)
        final = ram
    (tmp_path / "final.wram").write_bytes(final)
    rows = [dict(frame=0, mode=12, stage=0, callback="a4beb6", score=[0, 0])]
    for stage in range(9):
        for offset, mode, period in ((0, 8, 0), (1, 8, 1), (2, 18, 1), (3, 12, 1)):
            rows.append(dict(frame=stage*4+offset+1, mode=mode, period=period,
                             live_stage=stage, opponent=62+stage*2, score=[2, 0], clock=0, display_clock=0))
            if mode == 12:
                rows[-1].update(stage=stage+1, callback="85d32e" if stage == 8 else "85c37c")
    (tmp_path / "progression.jsonl").write_text("".join(json.dumps(row)+"\n" for row in rows))
    (tmp_path / "probe_script.c").write_text("fixture controller\n")
    driver_hash = hashlib.sha256((tmp_path / "probe_script.c").read_bytes()).hexdigest()
    (tmp_path / "build-manifest.json").write_text(json.dumps(dict(input_driver_sha256=driver_hash, exe_sha256="fixture")))
    (tmp_path / "command.json").write_text(json.dumps(["campaign_probe.exe", "--headless", "500000", "--script", "cup"]))
    (tmp_path / "outcome.json").write_text(json.dumps(dict(kind="cup", exit_code=0, one_process=True,
        save_loads=0, clocks_and_scores_edited=False, campaign_outcome="champion", terminal_verified=True, exe_sha256="fixture")))
    return tmp_path


def test_requires_nine_played_matches_and_native_results(evidence):
    result = CERT.certify(evidence)
    assert len(result["original_matches"]) == 9
    assert result["original_matches"][0]["opponent"] == 62
    assert result["original_matches"][-1]["generation"] == 10


def test_label_and_final_stage_do_not_replace_fulltime_evidence(evidence):
    path = evidence / "progression.jsonl"
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    path.write_text("".join(json.dumps(row)+"\n" for row in rows if row["frame"] != 19))
    with pytest.raises(AssertionError, match="Missing original fulltime"):
        CERT.certify(evidence)


def test_opponent_championship_is_not_player_win(evidence):
    path = evidence / "observed-checkpoints/campaign-010.sav"
    data = bytearray(path.read_bytes())
    data[512+0xddce] = 62
    data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
    path.write_bytes(data)
    with pytest.raises(AssertionError, match="champion is not the player team"):
        CERT.certify(evidence)


def test_missing_native_generation_is_rejected(evidence):
    (evidence / "observed-checkpoints/campaign-004.sav").unlink()
    with pytest.raises(AssertionError, match="nine native result"):
        CERT.certify(evidence)


def test_missing_clock_fields_do_not_prove_fulltime(evidence):
    path = evidence / "progression.jsonl"
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    del rows[19]["clock"]
    path.write_text("".join(json.dumps(row)+"\n" for row in rows))
    with pytest.raises(AssertionError, match="Missing original fulltime clocks"):
        CERT.certify(evidence)


def test_original_menu_score_reuse_does_not_change_fulltime_goals(evidence):
    path = evidence / "observed-checkpoints/campaign-003.sav"
    data = bytearray(path.read_bytes())
    struct.pack_into("<H", data, 512+0xda2, 1)
    data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
    path.write_bytes(data)
    trace = evidence / "progression.jsonl"
    rows = [json.loads(line) for line in trace.read_text().splitlines()]
    rows[8]["score"] = [1, 0]
    trace.write_text("".join(json.dumps(row)+"\n" for row in rows))
    match = CERT.certify(evidence)["original_matches"][1]
    assert match["score"] == [2, 0]
    assert match["checkpoint_menu_score"] == [1, 0]


def test_native_generation_requires_observed_original_transition(evidence):
    trace = evidence / "progression.jsonl"
    rows = [json.loads(line) for line in trace.read_text().splitlines()]
    rows[8]["stage"] = 8
    trace.write_text("".join(json.dumps(row)+"\n" for row in rows))
    with pytest.raises(AssertionError, match="Missing original committed checkpoint"):
        CERT.certify(evidence)


def test_additional_original_phase_setup_is_retained(evidence):
    archive = evidence / "observed-checkpoints"
    for generation in range(10, 3, -1):
        path = archive / f"campaign-{generation:03}.sav"
        data = bytearray(path.read_bytes())
        struct.pack_into("<Q", data, 24, generation+1)
        data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
        path.rename(archive / f"campaign-{generation+1:03}.sav")
        (archive / f"campaign-{generation+1:03}.sav").write_bytes(data)
    data = bytearray((archive / "campaign-001.sav").read_bytes())
    struct.pack_into("<Q", data, 24, 4)
    struct.pack_into("<H", data, 512+0x1640, 2)
    data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
    (archive / "campaign-004.sav").write_bytes(data)
    trace = evidence / "progression.jsonl"
    rows = [json.loads(line) for line in trace.read_text().splitlines()]
    for row in rows:
        if row["frame"] >= 9:
            row["frame"] += 1
    rows.append(dict(frame=9, mode=12, stage=2, callback="a4beb6", score=[0, 0]))
    rows.sort(key=lambda row: row["frame"])
    trace.write_text("".join(json.dumps(row)+"\n" for row in rows))
    result = CERT.certify(evidence)
    assert len(result["checkpoints"]) == 11
    assert result["original_matches"][2]["generation"] == 5


@pytest.mark.parametrize("missing_case", [False, True])
def test_production_continue_requires_every_native_generation(evidence, missing_case):
    result = CERT.certify(evidence)
    cases = [dict(checkpoint_sha256=state["sha256"], continue_preserved_bytes=True,
                  campaign_tables_settings_teams_preserved=True) for state in result["checkpoints"]]
    if missing_case:
        cases.pop(3)
    path = evidence / "continue-verification.json"
    path.write_text(json.dumps(dict(cases=cases, clones_only=True, source_campaign_modified=False,
                                    production_exe_sha256="fixture production")))
    if missing_case:
        with pytest.raises(AssertionError, match="every native generation"):
            CERT.certify(evidence, path)
    else:
        certificate = CERT.certify(evidence, path)
        assert certificate["production_continue_verified"]
        assert not certificate["continue_verification_required"]


@pytest.mark.parametrize("option", ["--import-password-symbols", "--import-password-at"])
def test_password_import_cannot_establish_fresh_run_proof(evidence, option):
    path = evidence / "command.json"
    command = json.loads(path.read_text())
    command += [option, "fixture"]
    path.write_text(json.dumps(command))
    with pytest.raises(AssertionError, match="did not start fresh"):
        CERT.certify(evidence)


def test_additional_original_result_boundary_maps_to_same_played_match(evidence):
    archive = evidence / "observed-checkpoints"
    for generation in range(10, 6, -1):
        path = archive / f"campaign-{generation:03}.sav"
        data = bytearray(path.read_bytes())
        struct.pack_into("<Q", data, 24, generation+1)
        data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
        path.rename(archive / f"campaign-{generation+1:03}.sav")
        (archive / f"campaign-{generation+1:03}.sav").write_bytes(data)
    data = bytearray((archive / "campaign-006.sav").read_bytes())
    struct.pack_into("<Q", data, 24, 7)
    struct.pack_into("<H", data, 512+0x1648, 12)
    data[512+0x1446:512+0x1449] = bytes.fromhex("2ed385")
    data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
    (archive / "campaign-007.sav").write_bytes(data)
    trace = evidence / "progression.jsonl"
    rows = [json.loads(line) for line in trace.read_text().splitlines()]
    for row in rows:
        if row["frame"] >= 21:
            row["frame"] += 1
    rows.append(dict(frame=21, mode=12, stage=5, callback="85d32e", score=[2, 0]))
    rows.sort(key=lambda row: row["frame"])
    trace.write_text("".join(json.dumps(row)+"\n" for row in rows))
    result = CERT.certify(evidence)
    assert len(result["checkpoints"]) == 11
    assert result["original_matches"][4]["boundary_generations"] == [6, 7]


@pytest.mark.parametrize("fault", ["tied", "wrong_side", "wrong_period", "missing_flag"])
def test_shootout_ceremony_requires_original_decisive_winner(evidence, fault):
    path = evidence / "observed-checkpoints/campaign-010.sav"
    data = bytearray(path.read_bytes())
    ram = memoryview(data)[512:]
    ram[0x1446:0x1449] = bytes.fromhex("c0c88b")
    struct.pack_into("<H", ram, 0xa8, 3)
    ram[0xd442:0xd444] = bytes([5, 4])
    if fault == "tied":
        ram[0xd443] = 5
    if fault == "wrong_side":
        struct.pack_into("<H", ram, 0x1700, 2)
    if fault == "wrong_period":
        struct.pack_into("<H", ram, 0xa8, 2)
    if fault == "missing_flag":
        struct.pack_into("<H", ram, 0x1648, 4)
    data[160:192] = hashlib.sha256(data[:160]+data[192:]).digest()
    path.write_bytes(data)
    with pytest.raises(AssertionError, match="Shootout|shootout|completion flag"):
        CERT.certify(evidence)
