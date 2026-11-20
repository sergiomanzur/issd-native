"""Every pack that ships has to pass the checker that ships with it.

Two of them did not, when the checker was first written: legacy integer
`formation` keys, names longer than the cartridge stores, and ratings packed
so tightly that a whole squad quantised to the same two values.

The second half of this file checks `skin_tones` directly. A team can offer
two palettes - one per player `skin_tone` value - and the loader is lenient
where the checker is not: a mistyped colour is skipped in silence there and
must be an error here, so a pack cannot look right and draw the wrong team.
"""
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

import validate_mod  # noqa: E402


def test_shipped_packs_validate():
    packs = sorted((REPO / "mods").glob("*.json"))
    assert packs, "no mod packs to check"
    result = subprocess.run(
        [sys.executable, str(REPO / "tools/validate_mod.py"), "--strict"]
        + [str(p) for p in packs],
        capture_output=True, text=True, cwd=REPO,
    )
    assert result.returncode == 0, result.stdout + result.stderr


def _report(team):
    report = validate_mod.Report()
    validate_mod.check_team(report, team, 0, [], set())
    return report


def _team(**fields):
    team = {"team_id": 9, "name": "France", "players": []}
    team.update(fields)
    return team


def test_skin_tones_must_be_one_or_two_objects():
    for tones in ("#EAC086", {}, [], [{"skin": "#EAC086"}] * 3,
                  ["#EAC086"], None):
        report = _report(_team(skin_tones=tones))
        assert report.errors, "skin_tones %r was accepted" % (tones,)

    # JSON null reaches the loader's array parser, which refuses the whole
    # pack, so the checker has to refuse it too.
    report = _report(_team(skin_tones=None))
    assert any("array" in e and "object" in e for e in report.errors), \
        report.errors


def test_skin_tone_parts_must_be_six_hex_digits():
    for tone in ({"skin": "red"}, {"hair": "#12345"},
                 {"skin": "#12345G"}, {"hair": 0x101010}):
        report = _report(_team(skin_tones=[tone]))
        assert any("must be a colour" in e for e in report.errors), \
            "skin tone %r was accepted: %s" % (tone, report.errors)


def test_skin_tone_with_neither_part_changes_nothing():
    report = _report(_team(skin_tones=[{"other": "value"}]))
    assert any("neither" in e and "skin" in e and "hair" in e
               for e in report.errors), report.errors


def test_player_using_tone_one_without_a_second_tone_warns_about_the_cartridge():
    report = _report(_team(skin_tones=[{"skin": "#EAC086", "hair": "#3A2A1A"}],
                           players=[{"shirt_number": 10, "name": "Mbappe",
                                     "skin_tone": 1}]))
    assert not report.errors, report.errors
    assert any("cartridge" in w and "skin_tone 1" in w
               for w in report.warnings), report.warnings


def test_skin_tone_above_one_names_the_goalkeeper_and_referee_palettes():
    report = _report(_team(players=[{"shirt_number": 1, "name": "Maignan",
                                     "skin_tone": 2}]))
    warnings = " ".join(report.warnings)
    assert "goalkeeper" in warnings and "referee" in warnings, report.warnings


def test_two_complete_tones_used_by_players_are_clean():
    report = _report(_team(
        skin_tones=[{"skin": "#EAC086", "hair": "#3A2A1A"},
                    {"skin": "#6B4226", "hair": "#101010"}],
        players=[{"shirt_number": 10, "name": "Mbappe", "skin_tone": 0},
                 {"shirt_number": 17, "name": "Saliba", "skin_tone": 1}]))
    assert not report.errors and not report.warnings, (report.errors,
                                                       report.warnings)


# A cartridge pack swaps the whole image instead of overlaying data. The
# loader is stricter still - size, mapping and cart type - but those are
# structural properties of the file, not of the pack, and the checker's job
# here is the pack: one source, the right extension, a sha256 if offered.
def _cartridge_report(tmp_path, cartridge, **extra):
    for name in ("hack.sfc", "hack.ips", "hack.smc"):
        (tmp_path / name).write_bytes(b"fixture")
    pack = {"name": "Cartridge hack", "cartridge": cartridge}
    pack.update(extra)
    report = validate_mod.Report()
    validate_mod.validate_data(pack, report, tmp_path)
    return report


def test_cartridge_must_be_an_object(tmp_path):
    for cartridge in ("hack.sfc", [], 3, None):
        report = _cartridge_report(tmp_path, cartridge)
        assert any("cartridge" in e and "object" in e for e in report.errors), \
            (cartridge, report.errors)


def test_cartridge_needs_exactly_one_of_image_and_patch(tmp_path):
    for cartridge in ({}, {"image": "hack.sfc", "patch": "hack.ips"}):
        report = _cartridge_report(tmp_path, cartridge)
        assert any("exactly one" in e for e in report.errors), \
            (cartridge, report.errors)


def test_cartridge_image_must_be_a_headerless_sfc(tmp_path):
    report = _cartridge_report(tmp_path, {"image": "hack.smc"})
    assert any("smc" in e and "headerless" in e for e in report.errors), \
        report.errors

    report = _cartridge_report(tmp_path, {"image": "hack.zip"})
    assert any(".sfc" in e for e in report.errors), report.errors


def test_cartridge_image_must_exist_beside_the_pack(tmp_path):
    report = _cartridge_report(tmp_path, {"image": "missing.sfc"})
    assert any("missing.sfc" in e and "beside" in e for e in report.errors), \
        report.errors


def test_cartridge_patch_must_be_ips_and_exist(tmp_path):
    report = _cartridge_report(tmp_path, {"patch": "hack.bps"})
    assert any(".ips" in e for e in report.errors), report.errors

    report = _cartridge_report(tmp_path, {"patch": "missing.ips"})
    assert any("missing.ips" in e and "beside" in e for e in report.errors), \
        report.errors

    report = _cartridge_report(tmp_path, {"patch": "hack.ips"})
    assert not report.errors, report.errors


def test_cartridge_sha256_is_sixty_four_lowercase_hex(tmp_path):
    for sha in ("A" * 64, "a" * 63, "g" * 64, "a" * 65, 1, True):
        report = _cartridge_report(tmp_path,
                                   {"image": "hack.sfc", "sha256": sha})
        assert any("sha256" in e and "64" in e for e in report.errors), \
            (sha, report.errors)

    report = _cartridge_report(tmp_path,
                               {"image": "hack.sfc", "sha256": "ab" * 32})
    assert not report.errors, report.errors


def test_cartridge_only_pack_passes(tmp_path):
    report = _cartridge_report(tmp_path, {"image": "hack.sfc"})
    assert not report.errors and not report.warnings, (report.errors,
                                                       report.warnings)


def test_cartridge_layers_with_teams_and_stadiums(tmp_path):
    report = _cartridge_report(
        tmp_path, {"patch": "hack.ips"},
        teams=[{"team_id": 9, "name": "France", "players": []}],
        stadiums=[{"stadium_id": 0, "name": "Wembley"}])
    assert not report.errors and not report.warnings, (report.errors,
                                                       report.warnings)


def test_empty_teams_beside_a_cartridge_is_not_an_empty_pack(tmp_path):
    """The cartridge key is content: an empty team list does not make it a
    pack that "changes nothing", so strict mode must not warn about it."""
    report = _cartridge_report(tmp_path, {"image": "hack.sfc"}, teams=[])
    assert not report.errors and not report.warnings, (report.errors,
                                                       report.warnings)


def test_cartridge_file_must_stay_inside_the_pack(tmp_path):
    report = _cartridge_report(tmp_path, {"image": "../outside.sfc"})
    assert any("cartridge" in e for e in report.errors), report.errors
