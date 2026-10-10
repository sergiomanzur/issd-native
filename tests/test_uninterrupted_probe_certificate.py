"""An interrupted/partial diagnostic run must never certify completion."""
import importlib.util
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("continuous_probe", ROOT / "tools/ghidra/probe_uninterrupted_campaign.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def ram_words(**values):
    ram = bytearray(0x20000)
    for address, value in values.items():
        offset = int(address, 16)
        ram[offset:offset+2] = value.to_bytes(2, "little")
    return ram


def test_cup_result_and_menu_return_are_not_elimination():
    outcome = dict(kind="cup", exit_code=0, generation=2, label="Cup result: Brazil")
    menu = ram_words(**{"32": 6, "70": 12, "1538": 0x9d72, "153a": 0xa4})
    with pytest.raises(ValueError, match="elimination terminal"):
        probe.validate_terminal(outcome, [], menu)
    rows = [dict(mode=12, callback="8bc1f6")]
    assert probe.validate_terminal(outcome, rows, menu) == "eliminated"
    assert probe.validate_terminal(outcome, rows, bytearray(0x20000)) == "eliminated"


def test_cup_completed_checkpoint_requires_final_stage():
    outcome = dict(kind="cup", exit_code=0, generation=2, label="Cup complete: Brazil")
    with pytest.raises(ValueError):
        probe.validate_terminal(outcome, [], ram_words(**{"1640": 8}))
    assert probe.validate_terminal(outcome, [], ram_words(**{"1640": 9})) == "champion"


def test_world_requires_schedule_terminal_and_native_commit():
    outcome = dict(kind="world", exit_code=0, generation=37, label="World Series complete: Brazil",
                   completed_rounds=list(range(1, 36)))
    ram = ram_words(**{"70": 12, "1652": 35})
    ram[0x1446:0x1449] = bytes.fromhex("ee948b")
    assert probe.validate_terminal(outcome, [], ram) == "complete"
    for changed in [dict(completed_rounds=list(range(2, 36))), dict(generation=35),
                    dict(label="World Series result: Brazil"), dict(exit_code=1)]:
        with pytest.raises(ValueError):
            probe.validate_terminal(dict(outcome, **changed), [], ram)
    ram[0x1446:0x1449] = bytes.fromhex("96948b")
    with pytest.raises(ValueError):
        probe.validate_terminal(outcome, [], ram)


def test_missing_capture_cannot_certify_any_campaign():
    with pytest.raises(ValueError, match="WRAM"):
        probe.validate_terminal(dict(kind="cup", exit_code=0), [], b"")
