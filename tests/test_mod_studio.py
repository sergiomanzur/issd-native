"""The editor must not drift from the game it edits.

Two things are worth pinning. First, everything the editor claims to know
about the cartridge is parsed out of this repository, and a frozen .exe uses a
baked snapshot instead - so the snapshot has to still match. Second, the
editor has to be able to open every pack that ships here, walk each of its
panes, and write the file back unchanged; a round trip that loses a key is how
an editor quietly eats somebody's work.
"""
import json
import os
import sys

import pytest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))

from mod_studio import model, preview, repo          # noqa: E402


def test_baked_snapshot_matches_the_repository():
    """The .exe carries a copy of what the C says. If they disagree the editor
    is describing a game that no longer exists."""
    root = repo._repo_root()
    assert root, "the test must run inside the repository"
    live = repo.gather(root)
    with open(repo.BAKED, encoding="utf-8") as f:
        baked = json.load(f)
    assert live == baked, ("tools/mod_studio/baked.json is out of date - "
                           "run `python tools/mod_studio/repo.py`")


def test_quantiser_matches_the_documented_bands():
    """The bands are published in docs/MODDING.md so a modder can see what a
    rating becomes. The editor draws them, so it has to agree."""
    expected = {2: (0, 7), 3: (8, 21), 4: (22, 35), 5: (36, 49),
                6: (50, 63), 7: (64, 77), 8: (78, 91), 9: (92, 99)}
    for nibble, band in expected.items():
        assert repo.nibble_band(nibble) == band
    assert repo.rating_to_nibble(0) == 2
    assert repo.rating_to_nibble(99) == 9
    assert repo.rating_to_nibble(200) == 9, "a silly rating must clamp"


def test_every_formation_the_game_knows_can_be_drawn():
    names = repo.formation_names()
    assert "4-3-3" in names and "4-4-2" in names
    for name in names:
        for tactics in repo.TACTICS:
            im = preview.formation(name, tactics)
            assert im.size[0] > 0
            # Ten outfield slots, every one of them a role the drawing knows.
            shape = repo.formation(name)
            assert len(shape["slots"]) == 10
            for slot in shape["slots"]:
                assert slot["role"] in repo.ROLE_NAMES


def test_shipped_packs_survive_a_round_trip(tmp_path):
    """Open, save, re-open: the second file must hold everything the first did,
    including keys the editor has no field for."""
    packs = [os.path.join(REPO, "mods", n)
             for n in sorted(os.listdir(os.path.join(REPO, "mods")))
             if n.endswith(".json")]
    assert packs, "no packs to check"
    for path in packs:
        first = model.load(path)
        out = tmp_path / os.path.basename(path)
        model.save(first, str(out))
        second = model.load(str(out))
        assert _same(first, second), "%s changed on a round trip" % path


def _same(a, b):
    if isinstance(a, dict):
        return set(a) == set(b) and all(_same(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(_same(x, y) for x, y in zip(a, b))
    return a == b


def test_comments_are_accepted_because_packs_have_them():
    """The game's loader takes // and /* */, so the editor must too, and must
    not mistake a URL inside a string for a comment."""
    text = ('{ // which cup\n  "name": "a//b", /* note */ "teams": [],\n'
            '  "description": "see https://example.com/x" }')
    data = json.loads(model._strip_comments(text))
    assert data["name"] == "a//b"
    assert data["description"].endswith("example.com/x")


def test_a_new_pack_passes_the_validator(tmp_path):
    """Whatever the editor creates from scratch must be something the game
    will accept, or the first thing a modder sees is a red error."""
    import validate_mod

    pack = model.new_pack()
    pack["name"] = "Fresh"
    team = model.new_team(added=True)
    team["name"] = "Newtown"
    pack["teams"].append(team)
    pack["stadiums"] = [model.new_stadium(8)]
    pack["stadium_count"] = 9

    out = tmp_path / "fresh.json"
    model.save(pack, str(out))
    report = validate_mod.Report()
    validate_mod.validate(str(out), report)
    assert not report.errors, report.errors


def test_colour_round_trip():
    assert model.parse_colour("#C8102E") == (200, 16, 46)
    assert model.parse_colour("c8102e") == (200, 16, 46)
    assert model.parse_colour("red") is None
    assert model.parse_colour("#12") is None
    # Shading goes through five bits, so the preview shows what is stored.
    assert model.shade((255, 255, 255), 100) == (255, 255, 255)
    dark = model.shade((200, 16, 46), 60)
    assert all(0 <= c <= 255 for c in dark) and dark[0] < 200


@pytest.mark.skipif(not os.environ.get("DISPLAY") and sys.platform != "win32",
                    reason="needs a display")
def test_every_editor_pane_builds():
    """Walk the tree over a real pack and build each pane. A pane that throws
    only shows up when somebody clicks it, so click them all here."""
    try:
        import tkinter
        tkinter.Tk().destroy()
    except Exception as exc:                      # pragma: no cover
        pytest.skip("no usable display: %s" % exc)

    from mod_studio.app import Studio

    studio = Studio()
    try:
        studio.pack_data = model.load(
            os.path.join(REPO, "mods", "chivas_guadalajara.json"))
        studio.path = os.path.join(REPO, "mods", "chivas_guadalajara.json")
        studio.pack_data.setdefault("stadiums", []).append(model.new_stadium(8))
        studio.refresh_tree(select_root=True)
        for iid in _walk(studio.tree, ""):
            studio.tree.selection_set(iid)
            studio.show_selected()
            studio.update_idletasks()
    finally:
        studio.destroy()


def _walk(tree, node):
    for child in tree.get_children(node):
        yield child
        yield from _walk(tree, child)
