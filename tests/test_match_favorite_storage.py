import hashlib
import os
from pathlib import Path
import struct
import subprocess

import pytest
from test_config_persistence import compile_c


@pytest.fixture(scope="module")
def favorite_exe(tmp_path_factory):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path_factory.mktemp("favorite-bin") / "favorite.exe"
    compile_c(exe, repo, [repo / "tests/test_match_favorite_storage.c", repo / "deps/snesrecomp/runner/src/sha256.c"], [repo / "ISSDNative", repo / "deps/snesrecomp/runner/src"])
    return exe


def run(exe, root, action, value=11, context=4, flags=0, **settings):
    env = os.environ.copy()
    env["ISSD_TEST_GAMEPLAY_FLAGS"] = str(flags)
    env.update({key: "1" for key, selected in settings.items() if selected})
    result = subprocess.run([str(exe), str(root), str(context), action, str(value)], cwd=root.parent, env=env, text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr
    output = result.stdout.split("RESULT ", 1)[1].strip()
    assert "ram=99 callbacks=0" in output
    return output


def test_roundtrip_across_processes_and_independent_saves(favorite_exe, tmp_path):
    root = tmp_path / "saves"
    assert run(favorite_exe, root, "campaign").startswith("1 ")
    assert run(favorite_exe, root, "manual").startswith("1 ")
    prior = {p: p.read_bytes() for p in root.rglob("*.sav")}
    assert run(favorite_exe, root, "save", 42).startswith("1 ")
    path = root / "matches/favorite.sav"
    blob = path.read_bytes()
    assert blob[:4] == b"ISCE"
    assert blob[192:] == b"SLTR" + struct.pack("<II", 8, 42)
    assert hashlib.sha256(blob[:160] + blob[192:]).digest() == blob[160:192]
    assert run(favorite_exe, root, "read").startswith("1 42 ")
    assert "Training setup" in run(favorite_exe, root, "info")
    assert all(p.read_bytes() == data for p, data in prior.items())
    assert not list(root.rglob("*.tmp"))


@pytest.mark.parametrize("context,flags", [(5, 0), (4, 1), (4, 8)])
def test_incompatible_favorite_is_retained(favorite_exe, tmp_path, context, flags):
    root = tmp_path / "saves"
    assert run(favorite_exe, root, "save").startswith("1 ")
    path = root / "matches/favorite.sav"
    prior = path.read_bytes()
    for action in ("read", "info"):
        result = run(favorite_exe, root, action, context=context, flags=flags)
        assert result.startswith("0 ") and "incompatible" in result
    assert path.read_bytes() == prior
    assert run(favorite_exe, root, "read").startswith("1 11 ")


@pytest.mark.parametrize("damage", ["truncate", "payload", "metadata", "structure"])
def test_corrupt_favorites_rejected_without_mutation(favorite_exe, tmp_path, damage):
    root = tmp_path / "saves"
    assert run(favorite_exe, root, "save").startswith("1 ")
    path = root / "matches/favorite.sav"
    blob = bytearray(path.read_bytes())
    if damage == "truncate": blob = blob[:-1]
    elif damage == "payload": blob[-1] ^= 1
    elif damage == "metadata": blob[80] ^= 1
    else:
        blob[200] = 254
        blob[160:192] = hashlib.sha256(blob[:160] + blob[192:]).digest()
    path.write_bytes(blob)
    assert run(favorite_exe, root, "read").startswith("0 ")
    assert run(favorite_exe, root, "info").startswith("0 ")
    assert path.read_bytes() == blob


@pytest.mark.parametrize("action,value,settings", [
    ("save", 254, {}), ("null", 11, {}), ("empty", 11, {}),
    ("oversized", 11, {}), ("save", 11, {"ISSD_TEST_NO_CONTEXT": True}),
    ("save", 11, {"ISSD_TEST_NO_VALIDATOR": True}),
    ("save", 22, {"ISSD_FAVORITE_REPLACE_FAILURE": True}),
    ("save", 22, {"ISSD_FAVORITE_WRITE_FAILURE": True}),
    ("save", 22, {"ISSD_FAVORITE_FLUSH_FAILURE": True}),
    ("save", 22, {"ISSD_FAVORITE_CLOSE_FAILURE": True}),
    ("save", 22, {"ISSD_FAVORITE_DURABLE_FAILURE": True}),
])
def test_failed_write_preserves_favorite(favorite_exe, tmp_path, action, value, settings):
    root = tmp_path / "saves"
    assert run(favorite_exe, root, "save").startswith("1 ")
    path = root / "matches/favorite.sav"
    prior = path.read_bytes()
    assert run(favorite_exe, root, action, value, **settings).startswith("0 ")
    assert path.read_bytes() == prior
    assert run(favorite_exe, root, "read").startswith("1 11 ")
    assert not list(root.rglob("*.tmp"))


def test_missing_validator_and_context_reads_fail(favorite_exe, tmp_path):
    root = tmp_path / "saves"
    assert run(favorite_exe, root, "read").startswith("0 ")
    assert run(favorite_exe, root, "save").startswith("1 ")
    for key in ("ISSD_TEST_NO_VALIDATOR", "ISSD_TEST_NO_CONTEXT"):
        for action in ("read", "info"):
            assert run(favorite_exe, root, action, **{key: True}).startswith("0 ")
    assert run(favorite_exe, root, "nulloutputs").startswith("0 ")


def test_invalid_payload_is_rejected_before_creating_matches_directory(favorite_exe, tmp_path):
    root = tmp_path / "saves"
    assert run(favorite_exe, root, "save", 254).startswith("0 ")
    assert not (root / "matches").exists()
