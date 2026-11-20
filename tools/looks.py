"""Skin and hair, fitted to the two palettes a cartridge team actually has.

A player in a pack may say what he should look like - `"skin": "light"`,
`"hair_color": "blond"`, `"hair_style": 3` - and this module works out which
of the team's two palettes to use, writing nothing itself. The loader already
understands the result (`skin_tones` plus a per-player `skin_tone` nibble);
this is the offline authoring side, deliberately not a second runtime solver.

The solver, the classifier and the search - including its tie-break, where the
palette a team already has wins every tie - are a port of the contributed
patch `issd-skin-hair-color.patch`'s `tools/mod_studio/looks.py` (the same
search as `resolve_player_looks` in that patch's `issd_mod_rom.c`). Its
private-record allocator, team-level scalar colour keys and C-parity
scaffolding are NOT ported: the shipped pack format is `skin_tones`, tone 0 in
the kit record and tone 1 in free space, so the two tones may be any colour
the solver picks.

Read alongside `docs/MODDING.md` section 6a and `docs/APPEARANCE_GUIDE.md`.
"""
from __future__ import annotations

from collections import Counter, OrderedDict

try:                                    # imported as tools.looks
    from .mod_studio import cartridge as _cartridge
except ImportError:                     # run from inside tools/
    from mod_studio import cartridge as _cartridge

# --------------------------------------------------------------- vocabulary --
HAIR_CLASSES = ("black", "brown", "blond", "red", "grey")
HAIR_NAMES = {"black": "Black", "brown": "Brown", "blond": "Blond",
              "red": "Red", "grey": "Grey"}
SKINS = ("light", "dark")

# What a hair colour is painted with when no record on the cartridge has it.
# The same values as HAIR_PAINT in the patch's issd_mod_rom.c.
HAIR_PAINT = {
    "black": (0x1C, 0x1C, 0x1C), "brown": (0x5A, 0x33, 0x10),
    "blond": (0xE0, 0xC0, 0x60), "red": (0xB0, 0x4A, 0x1E),
    "grey": (0xA0, 0xA0, 0xA0),
}
# Five shades around a colour, used only when there is no cartridge record to
# borrow. The middle is the colour itself, as in the patch.
SKIN_CANON = {"light": (232, 184, 144), "dark": (141, 85, 36)}
SKIN_RATIOS = (115, 107, 100, 88, 75)

# The measured cartridge layout. These mirror the loader's defines; the two
# pointer tables sit in bank $80 and their records are length-prefixed:
# alternate skin = $0009 + five colours, alternate hair = $0001 + one colour,
# keeper kit = $0015 + eleven colours (five skin shades, four kit, two
# constants - no hair slot). Only the skin and hair tables are needed here;
# keeper parity is the loader's job (skin_tones[0].skin drives it).
ALT_SKIN_PTRS = 0x01037C
ALT_HAIR_PTRS = 0x0103D2
KIT_GK_PTRS = 0x010326
KIT_HAIR = 1
KIT_SKIN = 2
SKIN_SHADES = 5
SKIN_BASE = 2               # the middle shade: the classification pivot
GK_COLOURS = 11

LOOK_KEYS = ("skin", "hair_color", "hair_colour")


# ------------------------------------------------------------- colours -----
def to555(rgb) -> int:
    r, g, b = rgb
    return (r * 31 // 255) | ((g * 31 // 255) << 5) | ((b * 31 // 255) << 10)


def to_rgb(word: int) -> tuple:
    """BGR555 to the `#RRGGBB` an author reads: five bits widen to eight."""
    return ((word & 31) * 255 // 31, ((word >> 5) & 31) * 255 // 31,
            ((word >> 10) & 31) * 255 // 31)


def luma(word: int) -> int:
    return (299 * (word & 31) + 587 * ((word >> 5) & 31) +
            114 * ((word >> 10) & 31))


def hair_class(word: int) -> str:
    """Black, brown, blond, red or grey, from a five-bit colour.

    Hue, saturation and luma thresholds exactly as the contributed patch's
    classifier; the cartridge's own four alternate hair records land on
    grey, black, brown and brown under it.
    """
    r, g, b = to_rgb(word)
    mx, mn = max(r, g, b), min(r, g, b)
    y = (299 * r + 587 * g + 114 * b) // 1000
    sat = (mx - mn) * 100 // mx if mx else 0
    if mx < 75:
        return "black"
    if sat < 22:
        return "black" if y < 90 else "grey"
    if mx == r:
        hue = int(60 * (g - b) / (mx - mn))
    elif mx == g:
        hue = 120 + int(60 * (b - r) / (mx - mn))
    else:
        hue = 240 + int(60 * (r - g) / (mx - mn))
    if hue < 0:
        hue += 360
    if (hue >= 345 or hue < 24) and sat >= 55 and mx >= 140:
        return "red"
    return "blond" if y >= 140 else "brown"


def _rescale(old: int, old_base: int, new_base: int) -> int:
    out = 0
    for ch in range(3):
        sh = ch * 5
        o, ob, nb = (old >> sh) & 31, (old_base >> sh) & 31, (new_base >> sh) & 31
        v = (o * nb + ob // 2) // ob if ob else nb
        out |= min(v, 31) << sh
    return out


def ramp(olds, base: int, new_base: int) -> list:
    """`olds` with the colour at `base` replaced and the others scaled to it."""
    return [new_base if i == base else _rescale(o, olds[base], new_base)
            for i, o in enumerate(olds)]


def default_skin(rgb) -> list:
    """Five shades around `rgb`, for drawing when there is nothing better."""
    return [to555(tuple(min(255, c * k // 100) for c in rgb)) for k in SKIN_RATIOS]


def lightest(words):
    """The colour the loader shades down from: the lightest of the five.

    The cartridge's own ramps run dark to light and the loader's
    `write_skin_steps` ends at 100 percent, so the pack colour is the ramp's
    lightest word (MODDING section 6a). The patch stores its synthetic ramps
    base-in-the-middle; taking the maximum luma covers both.
    """
    return max(words, key=luma)


# --------------------------------------------------------------- the ROM ---
def _word(rom, off):
    return rom[off] | (rom[off + 1] << 8)


def _lightest_rgb(words):
    return to_rgb(lightest(words))


class Palettes:
    """Every team's skin and hair palettes, read once from a cartridge.

    `teams` holds the cartridge's own pair per slot. The donors are every
    distinct alternate record on the image in table order - the set the solver
    may borrow from, deduped by pointer exactly as the patch did.
    """

    def __init__(self, rom):
        self.rom = rom
        c = _cartridge.constants()
        self.team_count = c["ROM_KIT_PTR_TEAMS"]
        self._bank = c["ROM_KIT_BANK"] & 0x7F
        self.teams = []
        for team in range(self.team_count):
            rec = _cartridge.kit_offset(rom, team, False)
            kit = ([_word(rom, rec + i * 2) for i in range(17)] if rec else None)
            skin_ptr, skin = self._record(ALT_SKIN_PTRS, team, SKIN_SHADES)
            hair_ptr, hair_words = self._record(ALT_HAIR_PTRS, team, 1)
            gk_ptr, gk = self._record(KIT_GK_PTRS, team, GK_COLOURS)
            self.teams.append({
                "kit": kit,
                "skin": skin,
                "skin_ptr": skin_ptr,
                "hair": hair_words[0] if hair_words else None,
                "hair_ptr": hair_ptr,
                "gk": gk,
                "gk_ptr": gk_ptr,
            })

        lumas = []
        for team in self.teams:
            if team["kit"]:
                lumas.append(luma(team["kit"][KIT_SKIN + SKIN_BASE]))
            if team["skin"]:
                lumas.append(luma(team["skin"][SKIN_BASE]))
        # The cartridge-wide boundary between a light and a dark skin. Every
        # palette on the image is on one side or the other.
        self.darkness_midpoint = ((min(lumas) + max(lumas)) // 2
                                  if lumas else 0)

        self.skin_donors = OrderedDict()
        self.hair_donors = OrderedDict()
        for team in self.teams:
            if team["skin"] and team["skin_ptr"] not in self.skin_donors:
                self.skin_donors[team["skin_ptr"]] = team["skin"]
            if team["hair"] is not None and team["hair_ptr"] not in self.hair_donors:
                self.hair_donors[team["hair_ptr"]] = team["hair"]

    def _record(self, table, team, colours):
        """(pointer, words) for a length-prefixed record, or (0, None)."""
        at = table + team * 2
        if not 0 <= team < self.team_count or at + 2 > len(self.rom):
            return 0, None
        addr = _word(self.rom, at)
        if addr < 0x8000:
            return 0, None
        head = self._bank * 0x8000 + (addr - 0x8000)
        if (head + 2 + colours * 2 > len(self.rom) or
                _word(self.rom, head) != colours * 2 - 1):
            return 0, None
        return addr, [_word(self.rom, head + 2 + i * 2) for i in range(colours)]

    # ---- what a team has now ------------------------------------------------
    def has_kit(self, team) -> bool:
        return (isinstance(team, int) and not isinstance(team, bool) and
                0 <= team < self.team_count and bool(self.teams[team]["kit"]))

    def kit_skin(self, team) -> list:
        return self.teams[team]["kit"][KIT_SKIN:KIT_SKIN + SKIN_SHADES]

    def current_skin(self, team, tone: int = 0) -> list:
        """Tone 0's ramp from the kit; tone 1's alternate when it has one."""
        if tone == 1 and self.teams[team]["skin"]:
            return self.teams[team]["skin"]
        return self.kit_skin(team)

    def current_hair(self, team, tone: int = 0) -> int:
        if tone == 1 and self.teams[team]["hair"] is not None:
            return self.teams[team]["hair"]
        return self.teams[team]["kit"][KIT_HAIR]

    def is_dark(self, words) -> bool:
        return luma(words[SKIN_BASE]) < self.darkness_midpoint

    def donor_skins(self):
        """One ramp per darkness, first in table order - the C's donor set."""
        out = []
        for words in self.skin_donors.values():
            dark = self.is_dark(words)
            if not any(dark == seen for seen, _ in out):
                out.append((dark, words))
        return out

    def donor_hairs(self):
        """One colour per class, first in table order - the C's donor set."""
        out = []
        for word in self.hair_donors.values():
            cls = hair_class(word)
            if not any(cls == seen for seen, _ in out):
                out.append((cls, word))
        return out

    def current_rgb(self, team, tone: int = 0):
        """(skin, hair) as RGB for the palette a team has now."""
        return (_lightest_rgb(self.current_skin(team, tone)),
                to_rgb(self.current_hair(team, tone)))


# ------------------------------------------------------------ the squad ----
def _is_keeper(player) -> bool:
    return str(player.get("position", "")).upper() in ("GK", "1")


def wants(player):
    """(skin, hair, problem) a player asks for. Either may be None.

    Both bad values are reported: fixing one at a time should not take a
    second run to discover the other.
    """
    skin = player.get("skin") or None
    hair = player.get("hair_color") or player.get("hair_colour") or None
    problems = []
    if skin is not None and skin not in SKINS:
        problems.append("skin must be light or dark")
        skin = None
    if hair == "gray":
        hair = "grey"
    if hair is not None and hair not in HAIR_CLASSES:
        problems.append("hair_color must be black, brown, blond, red or grey")
        hair = None
    return skin, hair, "; ".join(problems) if problems else None


def _hits(want_skin, want_hair, skin, hair):
    return ((want_skin is not None and skin == want_skin) +
            (want_hair is not None and hair == want_hair))


class Palette:
    """One of a team's two palettes: its labels and the colours to write.

    `skin_words` is the ramp the choice came from; `skin_rgb` is what the pack
    should carry. They are not the same colour: the loader does not copy a
    ramp, it rebuilds one at {46, 68, 82, 89, 100}% of the authored colour
    (`write_skin_steps`), so an emitted value has to be chosen for that
    RE-DERIVED ramp.
    """

    def __init__(self, index, skin, hair, skin_words, hair_word, paint=False,
                 emit_skin=True):
        self.index = index
        self.skin = skin
        self.hair = hair
        self.skin_words = skin_words
        self.hair_word = hair_word
        self.hair_paint = paint
        # False when this tone keeps the team's own record: the fitter omits
        # the skin part and the loader leaves the stock ramp untouched.
        self.emit_skin = emit_skin

    @property
    def skin_middle_rgb(self):
        """The source ramp's own middle shade, as RGB."""
        if not self.skin_words:
            return SKIN_CANON.get(self.skin or "light")
        return to_rgb(self.skin_words[SKIN_BASE])

    @property
    def skin_rgb(self):
        """The colour to author, so the loader's 82% step is the middle.

        The loader's middle shade is 82% of the authored colour, so invert
        that step: middle / 0.82. The old `lightest word` value made a dark
        donor's rebuilt middle read as light (0x8366 came out #E6AC83).
        """
        if not self.skin_words:
            return SKIN_CANON.get(self.skin or "light")
        return tuple(min(255, channel * 100 // 82)
                     for channel in self.skin_middle_rgb)

    @property
    def hair_rgb(self):
        if self.hair_paint or self.hair_word is None:
            return HAIR_PAINT.get(self.hair or "brown")
        return to_rgb(self.hair_word)

    def label(self):
        return "%s/%s" % (self.skin or "?", self.hair or "?")

    # The labels under the names the studio and the patch's callers use.
    @property
    def skin_class(self):
        return self.skin

    @property
    def hair_class(self):
        return self.hair

    def __repr__(self):
        return "<Palette %d %s>" % (self.index, self.label())


class PlayerLook:
    """What one player asked for, and how the solver served it."""

    def __init__(self, slot, player):
        player = player if isinstance(player, dict) else {}
        self.slot = slot
        self.name = str(player.get("name") or "slot %d" % slot)
        self.keeper = _is_keeper(player)
        self.skin, self.hair, self.problem = wants(player)
        self.attempted = any(key in player for key in LOOK_KEYS)
        self.tone = 0
        self.hits = 0
        self.need = 0

    @property
    def requested(self):
        """A player who chooses between the two palettes (keepers do not)."""
        return not self.keeper and (self.skin is not None or
                                    self.hair is not None)

    @property
    def representable(self):
        return not self.requested or self.hits >= self.need

    def wanted(self):
        return "%s/%s" % (self.skin or "any", self.hair or "any")


class Resolution:
    """Two palettes, who wears which, and everything that could not be met."""

    def __init__(self):
        self.team_id = None
        self.name = "This team"
        self.tones = []
        self.players = OrderedDict()
        self.tone_counts = Counter()
        self.warnings = []
        self.exact = False

    @property
    def p0(self):
        return self.tones[0] if self.tones else None

    @property
    def p1(self):
        return self.tones[1] if len(self.tones) > 1 else None

    @property
    def unmet(self):
        return [p for p in self.players.values()
                if p.requested and not p.representable]


class _Skin:
    def __init__(self, dark, words, current=False):
        self.dark, self.words, self.current = dark, words, current

    @property
    def cls(self):
        return "dark" if self.dark else "light"


class _Hair:
    def __init__(self, cls, word, paint=False):
        self.cls, self.word, self.paint = cls, word, paint


def _has_hair(options, cls):
    return any(o.cls == cls for o in options)


def _skin_options(palettes, dark, words):
    """The current skin first, then the cartridge's first donor of the other
    darkness, in the same order the patch's search used."""
    options = [_Skin(dark, words, True)]
    for alternative, donor in palettes.donor_skins():
        if alternative != dark and not any(o.dark == alternative
                                           for o in options):
            options.append(_Skin(alternative, donor))
    return options


def _hair_options(palettes, cls, word):
    """Current hair first, then distinct cartridge donors, then paint."""
    options = [_Hair(cls, word)]
    for donor_cls, donor in palettes.donor_hairs():
        if not _has_hair(options, donor_cls):
            options.append(_Hair(donor_cls, donor))
    for name in HAIR_CLASSES:
        if not _has_hair(options, name):
            options.append(_Hair(name, to555(HAIR_PAINT[name]), True))
    return options


def _generic_options():
    """A team the cartridge cannot describe: let the classes be free."""
    skins = [_Skin(False, default_skin(SKIN_CANON["light"])),
             _Skin(True, default_skin(SKIN_CANON["dark"]))]
    hairs = [_Hair(name, to555(HAIR_PAINT[name]), True) for name in HAIR_CLASSES]
    return skins, hairs


def _search(asked, s0o, h0o, s1o, h1o):
    """4-way exhaustive best pair. Strict `>` plus the lists' order means the
    first candidate - the palettes the team already has - wins every tie."""
    best, best_score = None, -1
    for a in s0o:
        for b in h0o:
            for c in s1o:
                for d in h1o:
                    score = 0
                    for p in asked:
                        m = max(_hits(p.skin, p.hair, a.cls, b.cls),
                                _hits(p.skin, p.hair, c.cls, d.cls))
                        score += 2 * m + (m == p.need)
                    if score > best_score:
                        best, best_score = (a, b, c, d), score
    return best, best_score


def resolve(palettes, team_requests) -> Resolution:
    """Fit a squad's named looks to the two palettes a team may have.

    `team_requests` is a team object (`{"team_id", "name", "players"}`) or
    just its list of player objects. With `palettes` (a `Palettes`) the two
    palettes start as the cartridge's own; without one, or for a team the
    cartridge cannot describe, the standard light/dark and hair-class
    candidates are used and the answer says so.
    """
    res = Resolution()
    if isinstance(team_requests, dict):
        players = team_requests.get("players") or []
        team_id = team_requests.get("team_id")
        if isinstance(team_id, int) and not isinstance(team_id, bool):
            res.team_id = team_id
        res.name = team_requests.get("name") or (
            "Team %s" % res.team_id if res.team_id is not None else "This team")
    else:
        players = list(team_requests or [])

    squad = [PlayerLook(slot, player) for slot, player in enumerate(players)]
    res.players = OrderedDict((p.slot, p) for p in squad)
    asked = [p for p in squad if p.requested]
    keepers = [p for p in squad if p.keeper and p.attempted]
    for p in asked:
        p.need = (p.skin is not None) + (p.hair is not None)

    exact = (palettes is not None and palettes.has_kit(res.team_id))
    res.exact = exact
    has_alt_skin = False
    if exact:
        team = palettes.teams[res.team_id]
        skin0 = palettes.kit_skin(res.team_id)
        hair0 = team["kit"][KIT_HAIR]
        has_alt_skin = bool(team["skin"])
        skin1 = team["skin"] if has_alt_skin else list(skin0)
        hair1 = team["hair"] if team["hair"] is not None else hair0
        dark0 = palettes.is_dark(skin0)
        dark1 = palettes.is_dark(skin1) if has_alt_skin else dark0
        s0o = _skin_options(palettes, dark0, skin0)
        h0o = _hair_options(palettes, hair_class(hair0), hair0)
        s1o = _skin_options(palettes, dark1, skin1)
        h1o = _hair_options(palettes, hair_class(hair1), hair1)
    else:
        s0o, h0o = _generic_options()
        s1o, h1o = _generic_options()

    (a, b, c, d), _ = _search(asked, s0o, h0o, s1o, h1o)

    # A tone that keeps the team's own record needs no skin colour at all:
    # omitting the part leaves the cartridge's ramp untouched, which is
    # class-exact by construction. Tone 1 shares the kit record when the team
    # has no alternate of its own, so it can only stay omitted while tone 0
    # also keeps the kit.
    if not exact:
        emit0 = emit1 = True
    elif has_alt_skin:
        emit0, emit1 = not a.current, not c.current
    else:
        emit0 = not a.current
        emit1 = not (c.current and a.current)

    res.tones = [
        Palette(0, a.cls, b.cls, a.words, b.word, b.paint, emit0),
        Palette(1, c.cls, d.cls, c.words, d.word, d.paint, emit1),
    ]

    for p in asked:
        m0 = _hits(p.skin, p.hair, res.p0.skin, res.p0.hair)
        m1 = _hits(p.skin, p.hair, res.p1.skin, res.p1.hair)
        p.tone = 1 if m1 > m0 else 0
        p.hits = max(m0, m1)
    res.tone_counts = Counter(p.tone for p in asked)

    # ---- everything the modder has to be told ------------------------------
    for p in squad:
        if p.problem:
            res.warnings.append("%s: %s." % (p.name, p.problem))
    for p in keepers:
        res.warnings.append(
            "%s is a goalkeeper: keeper kits are parity-only (the team's "
            "tone-0 skin is copied to the keeper record automatically), and "
            "keeper hair is not settable." % p.name)
    if not exact and (asked or keepers):
        res.warnings.append(
            "%s is not one of the cartridge's own teams, or has no readable "
            "strip, so its current palettes are unknown; the standard light/"
            "dark and hair colours are used." % res.name)
    for p in res.unmet:
        drawn = res.p0 if p.tone == 0 else res.p1
        res.warnings.append(
            "%s asks for %s, but %s has only two palettes (%s, %s); the "
            "closest is %s." % (p.name, p.wanted(), res.name,
                                res.p0.label(), res.p1.label(), drawn.label()))
    return res


def team_messages(team: dict, palettes=None) -> list:
    """The warnings for one team, ready to print."""
    return resolve(palettes, team).warnings
