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

from mod_studio import cartridge, model, preview, repo   # noqa: E402


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


ROM = os.path.join(REPO, "International Superstar Soccer Deluxe (USA).sfc")
needs_rom = pytest.mark.skipif(not os.path.isfile(ROM),
                               reason="no cartridge to read")


def test_the_cartridge_layout_parses_out_of_the_c():
    """Every offset the reader uses is taken from issd_mod_rom.c. If one
    stops parsing the reader would quietly fall back to a stale snapshot,
    so check the parse itself rather than the snapshot."""
    root = repo._repo_root()
    live = cartridge._parse_c(root)
    for key in cartridge._NEEDED:
        assert key in live, "%s no longer parses out of the C" % key
    assert live["ROM_NAME_BASE"] == 229774
    assert live["ROM_ATTR_BASE"] == 0x50000
    # The character set is the encoder's own table, read backwards.
    assert live["charset"][0x68] == "A"
    assert live["charset"][0x54] == "."
    # A strip is a pointer, not an index, so there is a table of addresses
    # rather than a list of record numbers.
    assert live["ROM_KIT_PTR_TEAMS"] >= live["ROM_STOCK_TEAMS"]


@needs_rom
def test_reading_a_team_gives_what_the_game_shows():
    rom = cartridge.load_rom(ROM)
    england = cartridge.read_team(rom, 2)
    assert england["name"] == "England"
    assert england["players"][0]["name"] == "R.Banks"
    assert england["players"][0]["position"] == "GK"
    assert england["players"][11]["position"] == "GK", "slot 11 is the reserve"
    assert len(england["players"]) == repo.SQUAD_SLOTS

    # Mexico wears green, Brazil yellow. If the kit table or the colour
    # arithmetic were wrong this is where it would show.
    mexico = cartridge.read_team(rom, 33)
    r, g, b = model.parse_colour(mexico["shirt"])
    assert g > r and g > b, mexico["shirt"]
    brazil = cartridge.read_team(rom, 30)
    r, g, b = model.parse_colour(brazil["shirt"])
    assert r > 200 and g > 200 and b < 80, brazil["shirt"]


@needs_rom
def test_stadiums_read_back_as_the_guide_documents_them():
    rom = cartridge.load_rom(ROM)
    stock = {s["id"]: s for s in repo.load()["stock_stadiums"]}
    for slot, expected in stock.items():
        got = cartridge.read_stadium(rom, slot)
        assert got["name"] == expected["name"]
        assert got["pitch_length"] == expected["length"]
        assert got["pitch_width"] == expected["width"]


@needs_rom
def test_an_imported_squad_written_back_is_the_same_bytes():
    """The point of reading the cartridge is to edit one player and leave
    the rest alone. That only holds if decoding and re-encoding is the
    identity - names, positions, appearance and every rating - so encode
    what was read and compare against the cartridge itself.

    Ratings are quantised, so the check is that the four bits come back the
    same, not the 0-99 number.
    """
    rom = cartridge.load_rom(ROM)
    c = cartridge.constants()
    encode = {v: k for k, v in c["charset"].items()}
    attrs = ("acceleration", "speed", "shooting", "technique", "balance",
             "intelligence", "dribbling", "jumping")

    for team in (0, 2, 17, 30, 33, 35):
        entry = cartridge.read_team(rom, team)
        names = cartridge.roster_base(rom, team)
        for slot, player in enumerate(entry["players"]):
            # the name, byte for byte
            want = rom[names + slot * 8:names + (slot + 1) * 8]
            got = bytearray(8)
            for i, ch in enumerate(player["name"][:8]):
                got[i] = 0x00 if ch == " " else encode.get(ch, 0x00)
            assert bytes(got) == bytes(want), (
                "team %d slot %d: %r" % (team, slot, player["name"]))

            a = c["ROM_ATTR_BASE"] + (team * 20 + slot) * 7
            raw = rom[a:a + 7]
            # the eight paired ratings
            for i in range(4):
                hi = repo.rating_to_nibble(player[attrs[i * 2]])
                lo = repo.rating_to_nibble(player[attrs[i * 2 + 1]])
                assert (hi << 4) | lo == raw[i], (
                    "team %d slot %d attr byte %d" % (team, slot, i))
            # position and stamina share a byte
            pos = player["position"]
            code = ({"GK": 1, "DF": 2, "MF": 4, "FW": 6}.get(pos)
                    if not pos.isdigit() else int(pos))
            stamina = repo.rating_to_nibble(player["stamina"])
            assert (code << 4) | stamina == raw[4], (
                "team %d slot %d position %r" % (team, slot, pos))
            # skin tone and hair style share the last one
            assert (player["skin_tone"] << 4) | player["hair_style"] == raw[6]


@needs_rom
def test_an_imported_team_passes_the_validator(tmp_path):
    """Whatever comes out of the cartridge has to be a pack the game will
    take back. Periods in names are the interesting case: the cartridge is
    full of them and the encoder used to turn them into spaces."""
    import validate_mod

    rom = cartridge.load_rom(ROM)
    pack = model.new_pack()
    pack["name"] = "Imported"
    for team in range(4):
        entry = cartridge.read_team(rom, team)
        entry.pop("_label", None)
        pack["teams"].append(entry)
    pack["stadiums"] = [cartridge.read_stadium(rom, s) for s in range(8)]

    out = tmp_path / "imported.json"
    model.save(pack, str(out))
    report = validate_mod.Report()
    validate_mod.validate(str(out), report)
    assert not report.errors, report.errors

    again = model.load(str(out))
    assert again["teams"][2]["players"][0]["name"] == "R.Banks"


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
