"""The offline looks fitter: named per-player looks become `skin_tones`.

The fitter is the authoring side of the per-player looks feature. Its whole
promise is that a pack carrying `"skin": "dark", "hair_color": "blond"` comes
out the other side with the two team palettes the loader already understands,
that a squad asking for more than two combinations is told honestly who lost,
and that a pack without look keys is not touched at all.

The retail cartridge at the repository root is the fixture every measured
expectation here comes from; the tests are skipped when it is absent so a
source-only checkout still runs.
"""
import json
import subprocess
import sys
from pathlib import Path

import pytest

from tools import looks

REPO = Path(__file__).resolve().parents[1]
ROM = REPO / "International Superstar Soccer Deluxe (USA).sfc"

needs_rom = pytest.mark.skipif(not ROM.is_file(),
                               reason="the retail cartridge is not in the checkout")


# --------------------------------------------------------------- fixtures ---
def pack_document(teams):
    return {"name": "Looks test", "author": "test", "version": "1",
            "description": "a fitter fixture", "teams": teams}


def write_pack(path, teams):
    path.write_text(json.dumps(pack_document(teams), indent=2) + "\n",
                    encoding="utf-8")
    return path


def player(slot, **look):
    p = {"shirt_number": slot + 1,
         "name": "P%s" % chr(ord("A") + slot % 26)}
    p.update(look)
    return p


def run_fitter(path, *extra):
    return subprocess.run(
        [sys.executable, str(REPO / "tools" / "fit_looks.py"),
         "--pack", str(path), *extra],
        cwd=str(REPO), capture_output=True, text=True)


def validate_strict(path):
    return subprocess.run(
        [sys.executable, str(REPO / "tools" / "validate_mod.py"),
         "--strict", str(path)],
        cwd=str(REPO), capture_output=True, text=True)


def keys_named(obj, names):
    found = []
    if isinstance(obj, dict):
        for key, value in obj.items():
            if key in names:
                found.append(key)
            # skin_tones legitimately has a "skin" key; look keys are the
            # per-player vocabulary this fitter consumes.
            if key != "skin_tones":
                found.extend(keys_named(value, names))
    elif isinstance(obj, list):
        for value in obj:
            found.extend(keys_named(value, names))
    return found


@pytest.fixture(scope="module")
def rom():
    return ROM.read_bytes()


@pytest.fixture(scope="module")
def palettes(rom):
    return looks.Palettes(rom)


# ------------------------------------------------------- the hair classes ---
def test_the_canonical_hair_colours_classify_as_themselves():
    for name, rgb in looks.HAIR_PAINT.items():
        assert looks.hair_class(looks.to555(rgb)) == name


@needs_rom
def test_the_cartridges_own_four_hair_records_classify(palettes):
    # Measured on the retail image: 34 teams point at the alternate hair
    # table, and those pointers name exactly four distinct records.
    assert len(palettes.hair_donors) == 4
    measured = {0x08386: "grey", 0x0837A: "black",
                0x08372: "brown", 0x08376: "brown"}
    assert {ptr: looks.hair_class(word)
            for ptr, word in palettes.hair_donors.items()} == measured


@needs_rom
def test_the_darkness_midpoint_is_the_cartridges_own(palettes):
    # Measured over every kit skin and alternate skin on the retail image.
    assert palettes.darkness_midpoint == 13020
    assert not palettes.is_dark(palettes.kit_skin(9))     # France's tone 0: light
    assert palettes.is_dark(palettes.current_skin(9, 1))  # its alternate: dark


# ------------------------------------------------------------ the solver ----
def team(players, team_id=9):
    return {"team_id": team_id, "name": "France", "players": players}


@needs_rom
def test_a_squad_that_fits_uses_both_tones(palettes):
    # France's own pair is light/brown and dark/black; the squad asks for
    # light/black and dark/blond, which two palettes can serve exactly.
    res = looks.resolve(palettes, team([
        player(0, skin="light", hair_color="black"),
        player(1, skin="light", hair_color="black"),
        player(2, skin="dark", hair_color="blond"),
        player(3, skin="dark", hair_color="blond"),
    ]))
    assert not res.unmet
    assert [t.label() for t in res.tones] == ["light/black", "dark/blond"]
    assert [p.tone for p in res.players.values()] == [0, 0, 1, 1]
    assert res.tone_counts == {0: 2, 1: 2}


@needs_rom
def test_staying_as_it_is_wins_every_tie(palettes):
    # No request, or a request the current palettes already satisfy, must not
    # move a palette: the search enumerates the current pair first and only a
    # strictly better score replaces it.
    current = ["light/brown", "dark/black"]
    assert [t.label() for t in looks.resolve(palettes, team([])).tones] == current

    first = looks.resolve(palettes, team([
        player(0, skin="light", hair_color="brown")]))
    assert not first.unmet
    assert [t.label() for t in first.tones] == current
    assert first.players[0].tone == 0

    second = looks.resolve(palettes, team([
        player(0, skin="dark", hair_color="black")]))
    assert not second.unmet
    assert [t.label() for t in second.tones] == current
    assert second.players[0].tone == 1


@needs_rom
def test_a_third_hair_colour_cannot_be_shown_and_is_named(palettes):
    res = looks.resolve(palettes, team([
        player(0, skin="dark", hair_color="black"),
        player(1, skin="dark", hair_color="brown"),
        player(2, skin="dark", hair_color="red"),
    ]))
    assert len(res.tones) == 2
    assert len({t.hair_class for t in res.tones}) == 2
    assert sum(1 for p in res.players.values() if p.representable) == 2
    assert len(res.unmet) == 1
    loser = res.unmet[0]
    assert loser.name in " ".join(res.warnings)
    assert "closest" in " ".join(res.warnings).lower()


@needs_rom
def test_an_unknown_skin_value_is_a_warning_not_a_guess(palettes):
    res = looks.resolve(palettes, team([
        player(0, skin="tan", hair_color="blond")]))
    text = " ".join(res.warnings)
    assert "skin must be light or dark" in text
    assert res.players[0].problem


@needs_rom
def test_a_keeper_is_told_that_kits_are_parity_only(palettes):
    res = looks.resolve(palettes, team([
        player(0, skin="light", hair_color="brown"),
        player(11, position="GK", skin="light", hair_color="blond"),
    ]))
    text = " ".join(res.warnings)
    assert "parity-only" in text
    assert "not settable" in text
    assert res.players[1].tone == 0
    # the keeper is not one of the two palettes' customers
    assert res.tone_counts == {0: 1}


# --------------------------------------------------------------- the CLI ----
def test_a_pack_without_look_keys_is_byte_identical(tmp_path):
    raw = (b'{"name":"t","teams":[{"team_id":9,"players":['
           b'{"shirt_number":1,"name":"A","skin_tone":1,"hair_style":3}]}]}')
    path = tmp_path / "plain.json"
    path.write_bytes(raw)
    out = tmp_path / "copy.json"

    result = run_fitter(path)
    assert result.returncode == 0, result.stderr
    assert path.read_bytes() == raw

    result = run_fitter(path, "--out", str(out))
    assert result.returncode == 0, result.stderr
    assert out.read_bytes() == raw


@needs_rom
def test_a_light_and_dark_squad_becomes_two_tones_the_loader_accepts(tmp_path):
    players = [player(i, skin="light", hair_color="black", hair_style=1)
               for i in range(4)]
    players += [player(4 + i, skin="dark", hair_color="blond", hair_style=7)
                for i in range(4)]
    path = write_pack(tmp_path / "looks.json", [{"team_id": 9,
                                                 "name": "France",
                                                 "players": players}])

    result = run_fitter(path)
    assert result.returncode == 0, result.stderr

    fitted = json.loads(path.read_text(encoding="utf-8"))
    france = fitted["teams"][0]
    tones = france["skin_tones"]
    assert len(tones) == 2
    for tone in tones:
        assert set(tone) == {"skin", "hair"}
        for colour in tone.values():
            assert colour.startswith("#") and len(colour) == 7

    assert [p["skin_tone"] for p in france["players"]] == [0, 0, 0, 0, 1, 1, 1, 1]
    assert [p["hair_style"] for p in france["players"]] == [1] * 4 + [7] * 4
    assert not keys_named(fitted, {"skin", "hair_color", "hair_colour"})

    checked = validate_strict(path)
    assert checked.returncode == 0, checked.stdout + checked.stderr


@needs_rom
def test_the_fitter_is_deterministic(tmp_path):
    players = [player(0, skin="light", hair_color="red"),
               player(1, skin="dark", hair_color="grey"),
               player(2, skin="light", hair_color="blond")]
    first = write_pack(tmp_path / "a.json", [{"team_id": 9, "players": players}])
    second = write_pack(tmp_path / "b.json", [{"team_id": 9, "players": players}])

    assert run_fitter(first).returncode == 0
    assert run_fitter(second).returncode == 0
    assert first.read_bytes() == second.read_bytes()

    once = first.read_bytes()
    assert run_fitter(first).returncode == 0
    assert first.read_bytes() == once          # idempotent: looks are consumed


@needs_rom
def test_dry_run_reports_without_writing(tmp_path):
    path = write_pack(tmp_path / "dry.json", [{"team_id": 9, "players": [
        player(0, skin="light", hair_color="blond")]}])
    before = path.read_bytes()

    result = run_fitter(path, "--dry-run")
    assert result.returncode == 0, result.stderr
    assert path.read_bytes() == before
    assert "tone" in result.stderr and "would" in result.stdout.lower()


@needs_rom
def test_three_hair_colours_leave_one_player_named_in_the_report(tmp_path):
    players = [player(0, skin="dark", hair_color="black"),
               player(1, skin="dark", hair_color="brown"),
               player(2, skin="dark", hair_color="red")]
    path = write_pack(tmp_path / "three.json",
                      [{"team_id": 9, "name": "France", "players": players}])

    result = run_fitter(path)
    assert result.returncode == 0, result.stderr
    assert "PC" in result.stderr
    assert "cannot be shown exactly" in result.stderr

    fitted = json.loads(path.read_text(encoding="utf-8"))
    tones = fitted["teams"][0]["skin_tones"]
    assert len(tones) == 2 and tones[0] != tones[1]
    assert sorted(p["skin_tone"] for p in fitted["teams"][0]["players"]) == [0, 0, 1]


@needs_rom
def test_a_keeper_request_warns_that_hair_is_not_settable(tmp_path):
    path = write_pack(tmp_path / "keeper.json", [{"team_id": 9, "players": [
        player(0, skin="light", hair_color="black"),
        player(11, position="GK", skin="dark", hair_color="blond"),
    ]}])

    result = run_fitter(path)
    assert result.returncode == 0, result.stderr
    assert "parity-only" in result.stderr
    assert "not settable" in result.stderr

    fitted = json.loads(path.read_text(encoding="utf-8"))
    keeper = fitted["teams"][0]["players"][1]
    assert keeper["skin_tone"] == 0
    assert not keys_named(fitted, {"skin", "hair_color"})


@needs_rom
def test_an_unreadable_pack_or_cartridge_is_the_only_failure(tmp_path):
    missing = tmp_path / "nope.json"
    assert run_fitter(missing).returncode == 1

    bad = tmp_path / "bad.json"
    bad.write_text("{not json", encoding="utf-8")
    assert run_fitter(bad).returncode == 1

    path = write_pack(tmp_path / "fine.json", [{"team_id": 9, "players": [
        player(0, skin="dark", hair_color="black")]}])
    assert run_fitter(path, "--base", str(tmp_path / "nope.sfc")).returncode == 1
