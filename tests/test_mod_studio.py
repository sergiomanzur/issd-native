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
import tkinter as tk

import pytest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))

from mod_studio import (cartridge, model, preview, repo,  # noqa: E402
                        tiles)


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


def test_skin_tones_are_carried_through_unchanged(tmp_path):
    """A team's two skin tones have no field in the editor, so they must
    survive as the pack wrote them. The World Cup pack's France entry is the
    shipped example: same values, and the block still ahead of the squad,
    because a person reads the file as well as the game."""
    pack = model.load(os.path.join(REPO, "mods", "world_cup_2026.json"))
    france = next(t for t in pack["teams"] if t.get("name") == "France")
    tones = france["skin_tones"]
    assert [t["skin"] for t in tones] == ["#EAC086", "#6B4226"]

    out = tmp_path / "world_cup_2026.json"
    model.save(pack, str(out))
    again = model.load(str(out))
    team = next(t for t in again["teams"] if t.get("name") == "France")
    assert team["skin_tones"] == tones
    keys = list(team)
    assert keys.index("skin_tones") < keys.index("players")


def test_the_appearance_labels_name_the_team_tones():
    """The byte's top half is not a hair colour: 0 and 1 are the team's own
    skins, and 2 and 3 belong to the goalkeeper and the referee. Anyone
    reading the editor is shown what the value does, not what it is called."""
    assert preview.APPEARANCE[0][0] == "Tone 0 (team default skin)"
    assert preview.APPEARANCE[1][0] == "Tone 1 (team alternate skin)"
    assert "goalkeeper" in preview.APPEARANCE[2][1].lower()
    assert "referee" in preview.APPEARANCE[3][1].lower()


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
            # both halves of the appearance byte share the last one
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


# ------------------------------------------------------- skin and hair ----
# The loader's own ramp rule: ISSDNative/issd_mod_rom.c:write_skin_steps
# stores five shades at these percentages of the authored colour. Mirrored
# here so a preview is judged by what the game will draw, not by itself.
LOADER_STEPS = (46, 68, 82, 89, 100)


def _loader_words(rgb, steps=LOADER_STEPS):
    words = []
    for step in steps:
        word = 0
        for channel, value in enumerate(rgb):
            value = min(255, value * step // 100)
            word |= (value * 31 // 255) << (5 * channel)
        words.append(word)
    return words


def _loader_ramp(rgb):
    return [((w & 31) * 255 // 31, ((w >> 5) & 31) * 255 // 31,
             ((w >> 10) & 31) * 255 // 31) for w in _loader_words(rgb)]


def _loader_colour(rgb):
    return _loader_ramp(rgb)[-1]


FRANCE = {"team_id": 9, "name": "France",
          "skin_tones": [{"skin": "#EAC086", "hair": "#3A2A1A"},
                         {"skin": "#6B4226", "hair": "#101010"}],
          "players": [{"name": "A", "skin_tone": 0},
                      {"name": "B", "skin_tone": 1}]}


def test_a_palette_pair_is_the_loaders_own_ramp():
    """The two tones are drawn through write_skin_steps, so the colours in
    the picture are the five the game will store - not the authored hex."""
    im = preview.palette_pair(FRANCE)
    seen = {colour for _n, colour in im.getcolors(maxcolors=1 << 20)}
    for entry in FRANCE["skin_tones"]:
        rgb = model.parse_colour(entry["skin"])
        for expected in _loader_ramp(rgb):
            assert expected in seen, expected
        assert _loader_colour(model.parse_colour(entry["hair"])) in seen

    report = preview.tone_report(FRANCE)
    assert [t["skin"] for t in report] == ["#EAC086", "#6B4226"]
    assert [t["hair"] for t in report] == ["#3A2A1A", "#101010"]
    assert [t["count"] for t in report] == [1, 1]
    assert not any(t["warning"] for t in report)
    # The pack wrote these colours, so they are neither the cartridge's nor
    # the editor's invention.
    assert [t["skin_source"] for t in report] == ["authored", "authored"]
    assert [t["hair_source"] for t in report] == ["authored", "authored"]
    assert [preview.tone_caption(t, "skin") for t in report] == \
        ["#EAC086", "#6B4226"]


@needs_rom
def test_a_tone_without_a_skin_part_shows_the_cartridge_ramp():
    """The fitter omits the skin from a tone that keeps the team's stock
    palette; the preview then has to show the cartridge's own five words."""
    import looks

    palettes = looks.Palettes(cartridge.load_rom(ROM))
    team = {"team_id": 9, "name": "France",
            "skin_tones": [{"hair": "#3A2A1A"}, {"hair": "#101010"}],
            "players": []}
    report = preview.tone_report(team, palettes)
    assert report[0]["skin"] is None
    assert report[0]["ramp"] == [looks.to_rgb(w)
                                 for w in palettes.current_skin(9, 0)]
    assert report[1]["ramp"] == [looks.to_rgb(w)
                                 for w in palettes.current_skin(9, 1)]

    seen = {colour for _n, colour
            in preview.palette_pair(team, palettes).getcolors(maxcolors=1 << 20)}
    for colour in report[0]["ramp"]:
        assert colour in seen


def test_a_tone_without_a_cartridge_says_its_colours_are_assumed():
    """The fitter omits `skin` from a tone that keeps the team's stock palette.
    With no cartridge there is no ramp to read, so the preview invents a
    default - and must say so rather than call it the cartridge's."""
    team = {"team_id": 9, "name": "France", "players": [],
            "skin_tones": [{"hair": "#3A2A1A"}, {"hair": "#101010"}]}
    report = preview.tone_report(team, None)
    assert [t["skin_source"] for t in report] == ["default", "default"]
    assert [t["hair_source"] for t in report] == ["authored", "authored"]
    for entry in report:
        skin = preview.tone_caption(entry, "skin")
        assert "assumed" in skin
        assert "(cartridge)" not in skin
    # The authored hair is still the pack's own colour, so it is neither
    # assumed nor the cartridge's.
    assert [preview.tone_caption(t, "hair") for t in report] == \
        ["#3A2A1A", "#101010"]
    assert preview.has_assumed(report)
    note = preview.pair_note(report)
    assert "no cartridge" in note.lower()
    assert "assumed" in note.lower()


@needs_rom
def test_only_a_loaded_cartridge_earns_the_cartridge_caption():
    """The same team, with a Palettes in hand, really is showing the
    cartridge's ramp - so that is when the caption may say so."""
    import looks

    palettes = looks.Palettes(cartridge.load_rom(ROM))
    team = {"team_id": 9, "name": "France", "players": [],
            "skin_tones": [{"hair": "#3A2A1A"}, {}]}
    report = preview.tone_report(team, palettes)
    assert [t["skin_source"] for t in report] == ["cartridge", "cartridge"]
    assert [t["hair_source"] for t in report] == ["authored", "cartridge"]
    for entry in report:
        assert "(cartridge)" in preview.tone_caption(entry, "skin")
    assert preview.tone_caption(report[0], "hair") == "#3A2A1A"
    assert not preview.has_assumed(report)
    assert "assumed" not in preview.pair_note(report).lower()


@needs_rom
def test_a_loaded_cartridge_that_lacks_the_team_says_no_record():
    """The note must not blame the cartridge when a cartridge is loaded.

    A team the image has no readable kit for still gets assumed colours, but
    the sentence underneath has to say that about the team, not claim there is
    no cartridge."""
    import looks

    palettes = looks.Palettes(cartridge.load_rom(ROM))
    team = {"team_id": 99, "name": "Not on the image", "players": [],
            "skin_tones": [{"hair": "#3A2A1A"}, {}]}
    report = preview.tone_report(team, palettes)
    assert preview.has_assumed(report)
    note = preview.pair_note(report, palettes).lower()
    assert "no cartridge record" in note
    assert "no cartridge loaded" not in note
    # Without a cartridge the older, true sentence is still the one used.
    assert "no cartridge loaded" in preview.pair_note(report, None).lower()


def test_a_pair_captions_the_players_who_get_a_closest_match():
    """A squad with three hair colours cannot be served exactly. The studio
    uses the solver's own analysis, so the closest-match line appears here."""
    team = dict(FRANCE, players=[
        {"name": "A", "position": "MF", "skin": "dark", "hair_color": "black"},
        {"name": "B", "position": "MF", "skin": "dark", "hair_color": "brown"},
        {"name": "C", "position": "MF", "skin": "dark", "hair_color": "red"},
    ])
    report = preview.tone_report(team)
    warning = next(t["warning"] for t in report if t["warning"])
    assert "closest" in warning.lower()
    assert "C" in warning                        # the player who cannot be shown
    assert sum(t["count"] for t in report) == 3
    assert preview.palette_pair(team).size[0] > 0


@needs_rom
def test_a_look_card_draws_the_decoded_hair_graphic():
    """hair_style 0 is the standard sprite head; 1-13 overlay one of the
    thirteen graphics at $81:ED6D. Both are drawn from the cartridge."""
    import looks

    palettes = looks.Palettes(cartridge.load_rom(ROM))
    plain = preview.look_card({"hair_style": 0, "skin_tone": 0}, FRANCE,
                              palettes)
    short = preview.look_card({"hair_style": 5, "skin_tone": 1}, FRANCE,
                              palettes)
    long = preview.look_card({"hair_style": 13, "skin_tone": 1}, FRANCE,
                             palettes)
    assert plain.size == short.size == long.size
    assert len({plain.tobytes(), short.tobytes(), long.tobytes()}) == 3
    assert plain.tobytes() != long.tobytes()

    hair = _loader_colour(model.parse_colour(FRANCE["skin_tones"][1]["hair"]))
    assert hair in {c for _n, c in short.getcolors(maxcolors=1 << 20)}


@needs_rom
def test_a_look_card_without_a_cartridge_is_a_placeholder():
    import looks

    palettes = looks.Palettes(cartridge.load_rom(ROM))
    without = preview.look_card({"hair_style": 13}, FRANCE, None)
    real = preview.look_card({"hair_style": 13}, FRANCE, palettes)
    assert without.size == real.size
    assert without.tobytes() != real.tobytes()
    # 14 and 15 read past the table's thirteen entries: "no graphic", not an
    # invented head.
    missing = preview.look_card({"hair_style": 14}, FRANCE, palettes)
    assert missing.tobytes() != real.tobytes()


def test_a_hair_overlay_is_read_through_the_cartridge_table():
    """The decode is the contributed patch's, measured against these offsets:
    $81:ED6D holds thirteen 24-bit pointers, four bytes apart, and the shown
    frame is the fourth 32-byte tile."""
    rom = bytearray(0x200000)
    rom[preview.HAIR_TABLE + 4:preview.HAIR_TABLE + 7] = bytes(
        [0x00, 0x80, 0x9F])
    tile = bytearray(32)
    tile[0] = 0x80                                   # one pixel, colour 1
    at = preview._lorom(0x9F8000) + preview.HAIR_FRAME * 32
    rom[at:at + 32] = tile
    grid = preview.hair_graphic(bytes(rom), 2)       # style 2 is the second entry
    assert grid[0][0] == 1 and sum(map(sum, grid)) == 1
    assert preview.hair_graphic(bytes(rom), 0) is None
    assert preview.hair_graphic(bytes(rom), 14) is None


def test_a_tile_decodes_plane_by_plane():
    tile = bytearray(32)
    for y in range(8):
        tile[y * 2] = 0xF0            # plane 0 on the left four pixels
        tile[y * 2 + 1] = 0x0F        # plane 1 on the right four
    assert preview.decode_tiles(bytes(tile), 4, 1)[0] == \
        [1, 1, 1, 1, 2, 2, 2, 2]


def test_a_skin_tones_pack_is_written_byte_identically(tmp_path):
    """The model layer still round-trips the key it does not edit: saving a
    pack twice from the same data produces the same bytes, skin_tones and
    all."""
    pack = model.load(os.path.join(REPO, "mods", "world_cup_2026.json"))
    first = tmp_path / "first.json"
    second = tmp_path / "second.json"
    model.save(pack, str(first))
    model.save(model.load(str(first)), str(second))
    assert first.read_bytes() == second.read_bytes()
    again = model.load(str(second))
    france = next(t for t in again["teams"] if t.get("name") == "France")
    assert france["skin_tones"] == pack["teams"][
        [t.get("name") for t in pack["teams"]].index("France")]["skin_tones"]


def _studio_or_skip():
    """A Studio, or a skip when this machine's Tcl cannot start.

    A second Tk root in one process on Windows occasionally cannot read Tcl's
    own init.tcl. That is a display problem, not a pane problem, so it skips
    the same way a missing display does. Only Tcl's own failure is caught: a
    broken `Studio.__init__` or pane builder must fail the test, not skip it.
    """
    try:
        from mod_studio.app import Studio
        return Studio()
    except tk.TclError as exc:                    # pragma: no cover
        pytest.skip("no usable display: %s" % exc)


@pytest.mark.skipif(not os.environ.get("DISPLAY") and sys.platform != "win32",
                    reason="needs a display")
def test_every_editor_pane_builds():
    """Walk the tree over a real pack and build each pane. A pane that throws
    only shows up when somebody clicks it, so click them all here."""
    studio = _studio_or_skip()
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

        # A team with authored skin tones gets its "Skin tones" section, and
        # one of its players gets the per-player look card beside the
        # appearance spinboxes. Neither pane asks for the cartridge.
        world = model.load(os.path.join(REPO, "mods", "world_cup_2026.json"))
        france = next(t for t in world["teams"] if t.get("name") == "France")
        studio.pack_data["teams"].append(france)
        team_index = len(studio.pack_data["teams"]) - 1
        studio.refresh_tree(select_iid="team:%d" % team_index)
        assert studio.skin_section is not None
        studio.show_selected()
        assert studio.skin_section is not None
        studio.tree.selection_set("player:%d:1" % team_index)
        studio.show_selected()
        studio.update_idletasks()
        assert studio.look_card_label is not None
    finally:
        studio.destroy()


@pytest.mark.skipif(not os.environ.get("DISPLAY") and sys.platform != "win32",
                    reason="needs a display")
@needs_rom
def test_a_pane_reads_a_remembered_cartridge_without_a_dialog(monkeypatch):
    """The skin preview wants the cartridge's palettes before the user has
    imported anything. A remembered dump is read headlessly - a pane never
    opens a file dialog - and with nothing to read it falls back honestly."""
    from mod_studio import app as app_module

    def no_dialog(*_args, **_kwargs):
        pytest.fail("building a pane opened a file dialog")

    monkeypatch.setattr(app_module.filedialog, "askopenfilename", no_dialog)
    studio = _studio_or_skip()
    try:
        studio._remember_rom = lambda _path: None
        studio.rom = None
        studio.rom_path = ROM
        assert studio._palettes() is not None
        assert studio.rom is not None

        studio.rom = None
        studio.rom_path = None
        assert studio._palettes() is None
    finally:
        studio.destroy()


def test_the_pitches_and_the_players_are_real_pictures():
    """The editor shows photographs of the game, not drawings of it.

    A drawn rectangle was the right shape and the wrong pitch, and the head it
    used to draw for the appearance byte was of something the cartridge does
    not have. If a capture goes missing the editor silently falls back to
    invention, so check the files are there as well as that they load.
    """
    for slot in range(8):
        shot = preview._asset("pitch", "slot%d.png" % slot)
        assert shot is not None, "no capture for ground %d" % slot
        # A plan view off the stadium screen: wider than it is tall, and
        # small, because that is the size the screen draws it.
        assert shot.width > shot.height
        assert shot.width < 200

    # Every ground gives a different pitch. Two grounds sharing bytes would
    # mean the capture caught the same screen twice.
    seen = {preview._asset("pitch", "slot%d.png" % n).tobytes() for n in range(8)}
    assert len(seen) == 8

    for value in range(4):
        assert value in preview.APPEARANCE
        assert preview._asset("player", "%02X.png" % (value << 4)) is not None
        im = preview.player_appearance(value)
        assert im.size == (104, 176)


def test_a_formation_sits_on_the_ground_it_names():
    """The grass behind a shape is that ground's own, so two grounds must not
    produce the same picture."""
    a = preview.formation("4-4-2", "balanced", stadium=0)
    b = preview.formation("4-4-2", "balanced", stadium=3)
    assert a.size == b.size
    assert a.tobytes() != b.tobytes()
    # And the shape still moves when the tactics do.
    c = preview.formation("4-4-2", "attacking", stadium=0)
    assert a.tobytes() != c.tobytes()


def test_a_tile_survives_the_trip_through_a_pack(tmp_path):
    """The game reads 32-bit top-down BMPs. Anything else is a black tile in
    the middle of the pitch."""
    from PIL import Image

    src = Image.new("RGBA", (8, 8))
    src.putpixel((0, 0), (10, 200, 30, 255))
    src.putpixel((7, 7), (200, 10, 30, 128))
    out = tiles.put(str(tmp_path), "0123456789abcdef", src, scale=4)

    with open(out, "rb") as f:
        header = f.read(54)
    assert header[:2] == b"BM"
    import struct
    width, height, _planes, bits = struct.unpack_from("<iiHH", header, 18)
    assert (width, height, bits) == (32, -32, 32), "top-down 32-bit or nothing"

    back = tiles.read_bmp(out)
    assert back.size == (32, 32)
    assert back.getpixel((0, 0)) == (10, 200, 30, 255)
    assert back.getpixel((31, 31)) == (200, 10, 30, 128)
    assert tiles.pack_contents(str(tmp_path)) == {"0123456789abcdef"}


def test_a_replacement_is_always_a_size_the_game_will_load():
    """Square, a multiple of 8, and no bigger than 512 - whatever was dropped
    in. Refusing art for being 37 pixels wide helps nobody."""
    assert tiles.legal_edge(32) == 32
    assert tiles.legal_edge(37) == 32
    assert tiles.legal_edge(0) == tiles.TILE
    assert tiles.legal_edge(99999) == tiles.MAX_EDGE
    for edge in (1, 7, 8, 9, 63, 512, 4096):
        got = tiles.legal_edge(edge)
        assert got % tiles.TILE == 0 and tiles.TILE <= got <= tiles.MAX_EDGE


def test_a_dump_shows_the_grass_first():
    """A dump is hundreds of files named after a hash. Sorted by how much of
    each tile is pitch, someone building a pitch finds what they came for."""
    dump = tiles.Dump(os.path.join(REPO, "mods", "estadio_akron_hd"))
    assert len(dump) > 100
    grassiness = [t[2] for t in dump.tiles]
    assert grassiness == sorted(grassiness, reverse=True)
    assert dump.grass, "a pitch pack with no grass in it"
    assert dump.grass == dump.tiles[:len(dump.grass)]
    for _name, im, _g in dump.tiles:
        assert im.width == im.height


@pytest.mark.skipif(not os.environ.get("DISPLAY") and sys.platform != "win32",
                    reason="needs a display")
def test_the_tile_pane_puts_art_into_a_pack(tmp_path):
    """Open a dump, pick a tile, put it in a pack - the whole path a person
    takes, without the file dialogs."""
    studio = _studio_or_skip()
    from mod_studio.app import TileDialog

    try:
        pane = TileDialog(studio)
        pane.withdraw()
        # Nothing open yet: an empty grid, and a click that lands on nothing.
        pane.redraw()
        assert pane.shown() == []

        pane.dump = tiles.Dump(os.path.join(REPO, "mods", "estadio_akron_hd"))
        pane.pack_dir = str(tmp_path)
        pane.redraw()
        assert len(pane._thumbs) == len(pane.dump.grass)

        pane.only_grass.set(False)
        pane.redraw()
        assert len(pane._thumbs) == len(pane.dump)

        class Click:
            x = 12 + 5
            y = 12 + 5
        pane.on_click(Click())
        assert pane.selected == pane.shown()[0][0]

        pane.enlarge()
        assert tiles.pack_contents(str(tmp_path)) == {pane.selected}
        assert tiles.read_bmp(
            os.path.join(str(tmp_path), pane.selected + ".bmp")).size == (32, 32)
    finally:
        studio.destroy()


def _walk(tree, node):
    for child in tree.get_children(node):
        yield child
        yield from _walk(tree, child)
