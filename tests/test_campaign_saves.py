import os
import hashlib
import struct
import subprocess
from pathlib import Path

import pytest
from test_config_persistence import compile_c


@pytest.fixture(scope="module")
def campaign_exe(tmp_path_factory):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path_factory.mktemp("campaign-bin") / "campaign.exe"
    compile_c(exe, repo, [repo / "tests/test_campaign_saves.c", repo / "ISSDNative/issd_save.c", repo / "deps/snesrecomp/runner/src/sha256.c"], [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"])
    return exe


@pytest.fixture(scope="module")
def fault_exe(tmp_path_factory):
    repo = Path(__file__).resolve().parents[1]
    directory = tmp_path_factory.mktemp("campaign-fault-bin")
    wrapper = directory / "fault.c"
    wrapper.write_text('#define ISSD_CAMPAIGN_FAULTS 1\n#include "' + (repo / "tests/test_campaign_saves.c").as_posix() + '"\n')
    exe = directory / "campaign-fault.exe"
    compile_c(exe, repo, [wrapper, repo / "deps/snesrecomp/runner/src/sha256.c"], [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"])
    return exe


def run(exe, root, action, value=None, context=4, failure=None, flags=0, validate=False):
    args = [str(exe), str(root), str(context), action]
    if value is not None:
        args.append(str(value))
    env = os.environ.copy()
    env["ISSD_TEST_GAMEPLAY_FLAGS"] = str(flags)
    if validate:
        env["ISSD_TEST_VALIDATE_PAYLOAD"] = "1"
    if failure:
        env["ISSD_TEST_SAVE_FAILURE"] = failure
    result = subprocess.run(args, cwd=root.parent, text=True, capture_output=True, env=env)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout.split("RESULT ", 1)[1].strip()


def saves(root):
    return sorted(root.glob("campaigns/*/campaign*.sav"))


def test_generations_recover_and_retain_only_valid_history(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    for value in [11, 22, 33]:
        assert run(campaign_exe, root, "save", value).startswith("1 ")
    files = saves(root)
    assert len(files) == 3
    data = [path.read_bytes() for path in files]
    assert sorted(struct.unpack_from("<Q", blob, 24)[0] for blob in data) == [1, 2, 3]
    for blob in data:
        assert struct.unpack_from("<Q", blob, 16)[0] == len(blob) - 192
        assert hashlib.sha256(blob[:160] + blob[192:]).digest() == blob[160:192]
    assert run(campaign_exe, root, "continue").startswith("1 33 ")
    primary = root.glob("campaigns/*/campaign.sav")
    primary = next(primary)
    primary.write_bytes(b"corrupt")
    assert "1 22 Recovered previous autosave" in run(campaign_exe, root, "continue")
    assert run(campaign_exe, root, "save", 44).startswith("1 ")
    primary.write_bytes(b"corrupt")
    assert run(campaign_exe, root, "continue").startswith("1 22 ")


@pytest.mark.parametrize("damage", ["truncate", "payload", "metadata", "future", "length", "context"])
def test_reject_before_mutation(campaign_exe, tmp_path, damage):
    root = tmp_path / "isolated"
    assert run(campaign_exe, root, "save", 11).startswith("1 ")
    path = saves(root)[0]
    data = bytearray(path.read_bytes())
    if damage == "truncate": data = data[:-1]
    elif damage == "payload": data[-1] ^= 1
    elif damage == "metadata": data[80] ^= 1
    elif damage == "future": struct.pack_into("<I", data, 4, 999)
    elif damage == "length": struct.pack_into("<Q", data, 16, (1 << 63))
    elif damage == "context": data[40] ^= 1
    path.write_bytes(data)
    result = run(campaign_exe, root, "continue")
    assert result.startswith("0 99 No recoverable campaign save")
    assert "callbacks=0" in result
    assert run(campaign_exe, root, "info").startswith("0 99 ")
    assert path.exists()


def test_context_histories_and_manual_compatibility(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    assert run(campaign_exe, root, "save", 11).startswith("1 ")
    assert run(campaign_exe, root, "save", 22, context=5).startswith("1 ")
    assert run(campaign_exe, root, "continue").startswith("1 11 ")
    assert run(campaign_exe, root, "continue", context=5).startswith("1 22 ")
    assert run(campaign_exe, root, "continue", flags=1).startswith("0 99 ")
    assert run(campaign_exe, root, "continue", flags=8).startswith("0 99 ")
    assert run(campaign_exe, root, "manual", 33).startswith("1 ")
    assert run(campaign_exe, root, "load", context=5).startswith("0 99 ")
    assert run(campaign_exe, root, "load", flags=8).startswith("0 99 ")
    result = run(campaign_exe, root, "load")
    assert result.startswith("1 33 ")
    assert "callbacks=1" in result
    assert run(campaign_exe, root, "legacy").startswith("0 ")
    legacy = tmp_path / "saves"
    legacy.mkdir()
    (legacy / "slot_0.sav").write_bytes(b"SLTR" + struct.pack("<II", 8, 44))
    (root / "slot_0.sav").unlink()
    assert run(campaign_exe, root, "legacy").startswith("1 ")
    assert run(campaign_exe, root, "load").startswith("0 99 ")
    assert run(campaign_exe, root, "load", 1).startswith("1 44 ")
    assert (legacy / "slot_0.sav").exists()


@pytest.mark.parametrize("failure", ["write", "flush", "durable", "close", "rotation", "replace"])
def test_failed_publication_keeps_current_checkpoint(fault_exe, tmp_path, failure):
    root = tmp_path / "isolated"
    for value in [11, 22, 33]:
        assert run(fault_exe, root, "save", value).startswith("1 ")
    assert run(fault_exe, root, "save", 44, failure=failure).startswith("0 ")
    assert run(fault_exe, root, "continue").startswith("1 33 ")
    assert not list(root.rglob("*.tmp"))


def test_temporary_paths_are_unique_and_ignore_stale_files(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    assert run(campaign_exe, root, "save", 11).startswith("1 ")
    primary = saves(root)[0]
    stale = Path(str(primary) + ".tmp")
    stale.write_bytes(b"uncommitted")
    blocked = Path(str(primary) + ".payload.tmp")
    blocked.mkdir()
    assert run(campaign_exe, root, "save", 22).startswith("1 ")
    assert run(campaign_exe, root, "continue").startswith("1 22 ")
    assert stale.read_bytes() == b"uncommitted"


def test_newest_structurally_bad_payload_recovers(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    assert run(campaign_exe, root, "save", 11).startswith("1 ")
    assert run(campaign_exe, root, "save", 254).startswith("1 ")
    result = run(campaign_exe, root, "continue")
    assert result.startswith("1 11 Recovered previous autosave")
    assert "callbacks=1" in result


def test_structurally_bad_payload_is_not_continue_eligible(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    assert run(campaign_exe, root, "save", 254).startswith("1 ")
    assert run(campaign_exe, root, "info", validate=True).startswith("0 99 No recoverable campaign save")
    result = run(campaign_exe, root, "continue", validate=True)
    assert result.startswith("0 99 No recoverable campaign save")
    assert "callbacks=0" in result
    assert run(campaign_exe, root, "manual", 254).startswith("1 ")
    result = run(campaign_exe, root, "load", validate=True)
    assert result.startswith("0 99 ")
    assert "callbacks=0" in result


def test_invalid_snapshot_candidates_do_not_replace_or_enter_history(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    assert run(campaign_exe, root, "save", 11).startswith("1 ")
    primary = saves(root)[0]
    original = primary.read_bytes()
    assert run(campaign_exe, root, "save", 254, validate=True).startswith("0 ")
    assert primary.read_bytes() == original
    assert run(campaign_exe, root, "save", 254).startswith("1 ")
    assert run(campaign_exe, root, "continue", validate=True).startswith("1 11 Recovered previous autosave")
    assert run(campaign_exe, root, "save", 22, validate=True).startswith("1 ")
    primary.write_bytes(b"corrupt")
    assert run(campaign_exe, root, "continue", validate=True).startswith("1 11 Recovered previous autosave")
    for path in saves(root):
        blob = path.read_bytes()
        assert blob == b"corrupt" or blob[192 + 8] != 254


def test_generation_beats_mtime_and_all_damage_is_preserved(campaign_exe, tmp_path):
    root = tmp_path / "isolated"
    for value in [11, 22, 33]:
        assert run(campaign_exe, root, "save", value).startswith("1 ")
    files = saves(root)
    for path in files:
        os.utime(path, (1000 if path.name == "campaign.sav" else 2000,) * 2)
    assert run(campaign_exe, root, "continue").startswith("1 33 ")
    for path in files:
        path.write_bytes(b"bad")
    assert run(campaign_exe, root, "continue").startswith("0 99 No recoverable campaign save")
    assert all(path.read_bytes() == b"bad" for path in files)


def test_default_context_root_is_user_storage(campaign_exe, tmp_path):
    env = os.environ.copy()
    env["LOCALAPPDATA"] = str(tmp_path / "appdata")
    env["XDG_DATA_HOME"] = str(tmp_path / "xdg")
    env["ISSD_TEST_GAMEPLAY_FLAGS"] = "0"
    result = subprocess.run([str(campaign_exe), "-", "4", "save", "11"], cwd=tmp_path, env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RESULT 1 11 " in result.stdout
    root = tmp_path / ("appdata/ISSDNative/saves" if os.name == "nt" else "xdg/issd-native/saves")
    assert len(saves(root)) == 1
    assert not (tmp_path / "saves").exists()


def test_simultaneous_campaign_writers_keep_monotonic_generations(fault_exe, tmp_path):
    root = tmp_path / "isolated"
    assert run(fault_exe, root, "save", 1).startswith("1 ")
    env = os.environ.copy()
    env["ISSD_TEST_SLOW_CAPTURE"] = "1"
    env["ISSD_TEST_GAMEPLAY_FLAGS"] = "0"
    processes = [subprocess.Popen([str(fault_exe), str(root), "4", "save", str(i)], cwd=tmp_path, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE) for i in range(10, 18)]
    outcomes = [p.communicate(timeout=30) for p in processes]
    assert all(p.returncode == 0 for p in processes), outcomes
    succeeded = sum("RESULT 1 " in stdout for stdout, _ in outcomes)
    assert succeeded >= 1
    newest = max(struct.unpack_from("<Q", path.read_bytes(), 24)[0] for path in saves(root))
    assert newest == succeeded + 1
    assert run(fault_exe, root, "continue").startswith("1 ")
    assert not list(root.rglob("*.tmp"))
