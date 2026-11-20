"""The ROM hack -> cartridge pack converter and the honesty contract it writes.

Every image here is synthetic. The one exception is the optional test at the
bottom, which reads the retail cartridge at the repository root as a *base*
fixture and is skipped when it is absent - the ROM itself is never committed.

The converter has two jobs and both are tested: it must produce a pack the
loader and `tools/validate_mod.py` accept (IPS whose application reproduces
the hack byte for byte), and it must classify what changed truthfully, because
the engine is a hybrid - boot, NMI and deny-listed routines run interpreted
from the cartridge, data tables are read from it, and the rest is precompiled
C whose bytes come from the retail image, not the hack.
"""
import hashlib
import json
import re
from pathlib import Path

import pytest

from tools import convert_rom_hack as conv
from tools import validate_mod

REPO = Path(__file__).resolve().parents[1]
IMAGE = 2 * 1024 * 1024
HEADER = 512
RETAIL_NAME = "International Superstar Soccer Deluxe (USA).sfc"


def _base_bytes():
    """A 2 MB deterministic image with a valid LoROM header.

    Patterned rather than random so RLE decisions are predictable and the
    fixture is cheap to build.
    """
    data = bytearray(bytes(range(256)) * (IMAGE // 256))
    data[0x7FD5] = 0x20          # LoROM, bit 0 clear
    data[0x7FD6] = 0x00          # ROM only
    return bytes(data)


@pytest.fixture
def base(tmp_path, monkeypatch):
    """A synthetic stand-in for the retail ROM, and the hash that accepts it."""
    path = tmp_path / "retail.sfc"
    path.write_bytes(_base_bytes())
    monkeypatch.setattr(conv, "RETAIL_SHA256",
                        hashlib.sha256(_base_bytes()).hexdigest())
    return path


def _hack(tmp_path, changes, name="hack.sfc", header=b""):
    """Write a hack image with {file offset: bytes} applied to the base."""
    data = bytearray(_base_bytes())
    for offset, payload in changes.items():
        data[offset:offset + len(payload)] = payload
    path = tmp_path / name
    path.write_bytes(header + bytes(data))
    return path


def _convert(tmp_path, rom=None, ips=None, name="Test Hack", extra=(),
             out_root=None, out_name="test_hack", **kw):
    """Run the CLI against a scratch mods root. Returns (json path, folder)."""
    stem = (out_root or tmp_path) / "mods" / out_name
    argv = ["--name", name, "--out", str(stem), *extra]
    if rom is not None:
        argv += ["--rom", str(rom)]
    if ips is not None:
        argv += ["--ips", str(ips)]
    argv += ["--base", str(kw.get("base") or tmp_path / "retail.sfc")]
    assert conv.main(argv) == 0
    return stem.with_suffix(".json"), stem


def _section(text, heading):
    """The part of REPORT.md under the '## ' heading containing `heading`."""
    lines = text.splitlines()
    start = next(i for i, line in enumerate(lines)
                 if line.startswith("## ") and heading.lower() in line.lower())
    end = next((i for i in range(start + 1, len(lines))
                if lines[i].startswith("## ")), len(lines))
    return "\n".join(lines[start:end])


# --- IPS codec and the round-trip guarantee ---------------------------------

def test_ips_round_trip_reproduces_the_hack_byte_for_byte(base):
    hack = _hack(Path(base).parent, {
        0x10: b"\x11\x22\x33",
        0x50000: b"ABCDEFGH",
        0x100000: b"\xFE",
    })
    patch = conv.make_ips(base.read_bytes(), hack.read_bytes())
    assert patch.startswith(b"PATCH") and patch.endswith(b"EOF")
    assert conv.apply_ips(base.read_bytes(), patch) == hack.read_bytes()


def test_long_identical_runs_use_rle(base):
    hack = _hack(Path(base).parent, {0x2000: b"\xA5" * 40})
    patch = conv.make_ips(base.read_bytes(), hack.read_bytes())
    # literal would be 5 + 40 + 6 bytes of framing; RLE is 8 + framing
    assert len(patch) < 20, len(patch)
    assert conv.apply_ips(base.read_bytes(), patch) == hack.read_bytes()


def test_changed_runs_merge_across_gaps_of_at_most_sixteen_bytes(base):
    data = bytearray(_base_bytes())
    for off in (0x1000, 0x1008, 0x1020):
        data[off] ^= 0xFF
    spans = conv.changed_spans(base.read_bytes(), bytes(data))
    # 0x1000..0x1009 and 0x1020..0x1021: the 23-byte gap is not merged
    assert spans == [(0x1000, 0x1009), (0x1020, 0x1021)]


def test_a_run_longer_than_one_ips_record_is_split(base):
    data = bytearray(_base_bytes())
    data[0x2000:0x2000 + 70000] = b"\x00" * 70000   # > 16-bit record length
    hack = bytes(data)
    patch = conv.make_ips(base.read_bytes(), hack)
    assert conv.apply_ips(base.read_bytes(), patch) == hack


def test_an_image_identical_to_the_base_says_the_pack_changes_nothing(base,
                                                                     tmp_path):
    _, folder = _convert(tmp_path, rom=base)
    report = (folder / "REPORT.md").read_text()
    assert "byte-identical" in report
    assert "changes nothing" in report

# --- the pack the tool writes ----------------------------------------------

def test_rom_mode_writes_a_valid_cartridge_pack(base, tmp_path):
    hack = _hack(tmp_path, {0x50000: b"NEWBYTES", 0x180000: b"\x42"})
    pack_json, folder = _convert(tmp_path, rom=hack, name="World Cup 2026")

    assert pack_json.is_file() and pack_json.name == "test_hack.json"
    assert folder.is_dir()
    pack = json.loads(pack_json.read_text())
    assert pack["name"] == "World Cup 2026"
    assert pack["cartridge"]["patch"] == "test_hack/test_hack.ips"
    assert pack["cartridge"]["sha256"] == hashlib.sha256(
        hack.read_bytes()).hexdigest()
    assert re.fullmatch(r"[0-9a-f]{64}", pack["cartridge"]["sha256"])

    ips = folder / "test_hack.ips"
    assert ips.is_file()
    assert conv.apply_ips(base.read_bytes(), ips.read_bytes()) == hack.read_bytes()
    assert (folder / "REPORT.md").is_file()

    # the shipped checker accepts what the converter writes
    report = validate_mod.Report()
    validate_mod.validate(str(pack_json), report)
    assert not report.errors, report.errors


def test_optional_metadata_is_carried_into_the_pack(base, tmp_path):
    hack = _hack(tmp_path, {0x50000: b"X"})
    pack_json, _ = _convert(tmp_path, rom=hack,
                            extra=("--author", "A Modder",
                                   "--version", "1.2.3"))
    pack = json.loads(pack_json.read_text())
    assert (pack["author"], pack["version"]) == ("A Modder", "1.2.3")


def test_ips_mode_produces_the_same_image_and_report(base, tmp_path):
    hack = _hack(tmp_path, {0x50000: b"ABCD", 0x180000: b"\x77"})
    patch = tmp_path / "hack.ips"
    patch.write_bytes(conv.make_ips(base.read_bytes(), hack.read_bytes()))

    pack_json, folder = _convert(tmp_path, ips=patch)
    pack = json.loads(pack_json.read_text())
    assert pack["cartridge"]["sha256"] == hashlib.sha256(
        hack.read_bytes()).hexdigest()
    assert conv.apply_ips(base.read_bytes(),
                          (folder / "test_hack.ips").read_bytes()) == \
        hack.read_bytes()

    report = (folder / "REPORT.md").read_text()
    assert "roster attributes" in report
    assert "0x180000" in _section(report, "not guaranteed")


def test_a_headered_hack_is_normalized_before_the_diff(base, tmp_path):
    changes = {0x50000: b"HEADERLESS"}
    plain = _hack(tmp_path, changes, name="plain.sfc")
    headered = _hack(tmp_path, changes, name="headered.sfc",
                     header=b"\x00" * HEADER)

    # Distinct output roots, same folder name under each: the pack JSON names
    # its folder, so this is what makes the two outputs comparable at all.
    plain_json, plain_dir = _convert(tmp_path, rom=plain,
                                     out_root=tmp_path / "plain")
    head_json, head_dir = _convert(tmp_path, rom=headered,
                                   out_root=tmp_path / "headered")

    plain_pack = json.loads(plain_json.read_text())
    head_pack = json.loads(head_json.read_text())
    assert plain_pack == head_pack, "a copier header must not change the pack"
    assert plain_pack["cartridge"]["sha256"] == \
        hashlib.sha256(plain.read_bytes()).hexdigest()
    assert (plain_dir / "test_hack.ips").read_bytes() == \
        (head_dir / "test_hack.ips").read_bytes()
    assert "copier header" in (head_dir / "REPORT.md").read_text().lower()


# --- the compatibility report ----------------------------------------------

def test_roster_change_is_reported_as_known_data(base, tmp_path):
    hack = _hack(tmp_path, {0x50000: b"0123456789"})
    _, folder = _convert(tmp_path, rom=hack)
    report = (folder / "REPORT.md").read_text()
    known = _section(report, "Known data")
    assert "0x050000" in known
    assert "roster attributes" in known.lower()
    assert "0x050000" not in _section(report, "not guaranteed")


def test_boot_change_is_reported_as_interpreted(base, tmp_path):
    hack = _hack(tmp_path, {0x10: b"\xDE\xAD"})
    _, folder = _convert(tmp_path, rom=hack)
    report = (folder / "REPORT.md").read_text()
    live = _section(report, "Interpreted")
    assert "0x000010" in live
    assert "live" in live.lower()
    assert "boot" in live.lower()


def test_unknown_code_space_is_reported_as_not_guaranteed(base, tmp_path):
    hack = _hack(tmp_path, {0x180000: b"\x42\x43"})
    _, folder = _convert(tmp_path, rom=hack)
    report = (folder / "REPORT.md").read_text()
    outside = _section(report, "not guaranteed")
    assert "0x180000" in outside
    assert "precompiled" in outside.lower()
    assert "relocated data is still read from its original addresses" in \
        " ".join(outside.lower().split())
    assert "0x180000" not in _section(report, "Known data")


def _deny_pcs():
    text = (REPO / "recomp" / "aot_boot_deny.txt").read_text()
    return [int(line.strip(), 16) for line in text.splitlines()
            if re.fullmatch(r"[0-9A-Fa-f]{6}", line.strip())]


def test_a_real_deny_listed_pc_is_reported_as_live(base, tmp_path):
    pcs = _deny_pcs()
    assert pcs, "the deny list is expected to hold the live routines"
    pc = pcs[0]
    offset = ((pc >> 16) & 0x7F) * 0x8000 + (pc & 0x7FFF)
    # the two mirrors of $80:CF2A both reach the same file byte
    assert offset == 0x4F2A
    hack = _hack(tmp_path, {offset: b"\x99"})
    _, folder = _convert(tmp_path, rom=hack)
    live = _section((folder / "REPORT.md").read_text(), "Interpreted")
    assert "0x%06X" % offset in live
    assert "aot_boot_deny.txt" in live


def test_the_deny_list_is_read_at_runtime_not_hardcoded(base, tmp_path,
                                                       monkeypatch):
    deny = tmp_path / "deny.txt"
    deny.write_text("# planted\n00B400\n")
    monkeypatch.setattr(conv, "DENY_LIST", deny)
    hack = _hack(tmp_path, {0x3400: b"\x5A"})
    _, folder = _convert(tmp_path, rom=hack)
    live = _section((folder / "REPORT.md").read_text(), "Interpreted")
    assert "0x003400" in live
    assert "$00:B400" in live


@pytest.mark.parametrize("offset,phrase", [
    (0x4FA42, "tone-1 free space"),
    (0x17B5D, "bank $82"),
    (0x3FAC8, "bank $87"),
    (0x3FC40, "added-roster space"),
])
def test_occupied_pack_free_space_is_flagged(base, tmp_path, offset, phrase):
    hack = _hack(tmp_path, {offset: b"\xFF\xFF"})
    _, folder = _convert(tmp_path, rom=hack)
    report = (folder / "REPORT.md").read_text()
    known = _section(report, "Known data")
    assert "occupied: pack free space at risk" in known.lower()
    assert phrase in known


def test_header_changes_are_reported_explicitly(base, tmp_path):
    hack = _hack(tmp_path, {0x7FC0: b"PATCHED HACK TITLE   ",
                            0x7FDB: b"\x02"})
    _, folder = _convert(tmp_path, rom=hack)
    header = _section((folder / "REPORT.md").read_text(), "Cartridge header")
    assert "0x7FC0" in header or "0x007FC0" in header
    assert "title" in header.lower()


# --- the loader's refusals are the converter's refusals --------------------

def test_an_image_past_two_megabytes_is_refused(base, tmp_path):
    path = tmp_path / "big.sfc"
    path.write_bytes(_base_bytes() + b"\x00" * 1024)
    out = tmp_path / "mods" / "big"
    with pytest.raises(conv.ConversionError, match="expand"):
        conv.convert(rom=path, name="Big", out=out, base=base)
    assert not out.with_suffix(".json").exists()
    assert not out.exists()


def test_an_ips_that_writes_past_two_megabytes_is_refused(base, tmp_path):
    # offset 0x1FFFFE, four bytes -> four past the last address
    patch = (b"PATCH" + (0x1FFFFE).to_bytes(3, "big") +
             (4).to_bytes(2, "big") + b"ABCD" + b"EOF")
    with pytest.raises(conv.ConversionError, match="expand"):
        conv.apply_ips(base.read_bytes(), patch)


def test_a_truncated_ips_is_refused(base, tmp_path):
    hack = _hack(tmp_path, {0x50000: b"X"})
    patch = conv.make_ips(base.read_bytes(), hack.read_bytes())[:-3]
    with pytest.raises(conv.ConversionError, match="EOF"):
        conv.apply_ips(base.read_bytes(), patch)


def test_data_after_the_eof_marker_is_refused(base, tmp_path):
    hack = _hack(tmp_path, {0x50000: b"X"})
    patch = conv.make_ips(base.read_bytes(), hack.read_bytes()) + b"junk"
    with pytest.raises(conv.ConversionError, match="after"):
        conv.apply_ips(base.read_bytes(), patch)


def test_a_non_lorom_image_is_refused(base, tmp_path):
    data = bytearray(_base_bytes())
    data[0x7FD5] = 0x21          # HiROM bit set
    path = tmp_path / "hirom.sfc"
    path.write_bytes(bytes(data))
    with pytest.raises(conv.ConversionError, match="LoROM"):
        conv.convert(rom=path, name="HiRom", out=tmp_path / "mods" / "h",
                     base=base)


def test_a_non_rom_only_image_is_refused(base, tmp_path):
    data = bytearray(_base_bytes())
    data[0x7FD6] = 0x02          # a coprocessor claim
    path = tmp_path / "co.sfc"
    path.write_bytes(bytes(data))
    with pytest.raises(conv.ConversionError, match="ROM-only"):
        conv.convert(rom=path, name="Co", out=tmp_path / "mods" / "c",
                     base=base)


def test_a_base_that_is_not_the_retail_image_is_refused(tmp_path, monkeypatch):
    bad = tmp_path / "bad.sfc"
    bad.write_bytes(b"\x00" * IMAGE)
    monkeypatch.setattr(conv, "RETAIL_SHA256", "0" * 64)
    with pytest.raises(conv.ConversionError) as exc:
        conv.load_base(bad)
    message = str(exc.value)
    assert "sha256" in message.lower()
    assert "--base" in message


def test_missing_base_names_the_retail_file_and_the_option(tmp_path,
                                                           monkeypatch):
    monkeypatch.chdir(tmp_path)
    with pytest.raises(conv.ConversionError) as exc:
        conv.load_base(None)
    message = str(exc.value)
    assert RETAIL_NAME in message
    assert "--base" in message


# --- the retail cartridge, when it is here --------------------------------

@pytest.mark.skipif(not (REPO / RETAIL_NAME).is_file(),
                    reason="retail cartridge is not present")
def test_the_retail_base_matches_the_recorded_hash():
    path, image = conv.load_base(REPO / RETAIL_NAME)
    assert path.name == RETAIL_NAME
    assert len(image) == IMAGE
    assert hashlib.sha256(image).hexdigest() == conv.RETAIL_SHA256
