"""Verify Continue on isolated copies of a continuous run's native saves.

The original running process/save directory is never modified. No snapshot
fields are edited. Each clone uses the normal production Continue path.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("cup", "world"))
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--expect-world-complete", action="store_true")
    opts = parser.parse_args()
    source, output = opts.source.resolve(), opts.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    rows = [json.loads(line) for line in (source / "progression.jsonl").read_text().splitlines()]
    candidates = {(row["stage"], row["round"]) for row in rows if row["mode"] == 12}
    cases = []
    for checkpoint in sorted((source / "observed-checkpoints").glob("*.sav")):
        data = checkpoint.read_bytes()
        assert data[:4] == b"ISCE" and data[160:192] == hashlib.sha256(data[:160]+data[192:]).digest()
        generation = int.from_bytes(data[24:32], "little")
        label = data[80:144].split(b"\0")[0].decode("ascii")
        case = output / f"generation-{generation:03}"
        saved = case / "owned-saves/campaigns" / checkpoint.stem.rsplit("-", 1)[0] / "campaign.sav"
        saved.parent.mkdir(parents=True)
        saved.write_bytes(data)
        shutil.copy2(source / "isolated.cfg", case / "isolated.cfg")
        (case / "input.txt").write_text("0 NONE\n")
        command = [str(ROOT / "build/ISSDNative.exe"), "--rom", str(ROOT / "International Superstar Soccer Deluxe (USA).sfc"),
                   "--headless", "80", "--continue", "--script", str(case / "input.txt"),
                   "--config", str(case / "isolated.cfg"), "--mods-dir", str(case / "empty-mods"),
                   "--save-dir", str(case / "owned-saves"), "--dump-state", str(case / "restored"),
                   "--screenshot", str(case / "restored.bmp")]
        result = subprocess.run(command, cwd=case, capture_output=True, text=True, timeout=120,
                                env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"))
        (case / "run.log").write_text(result.stdout+result.stderr)
        assert result.returncode == 0 and "[interp_cap]" not in result.stderr, result.stdout+result.stderr
        assert saved.read_bytes() == data, "Continue republished its checkpoint"
        ram = (case / "restored.wram").read_bytes()
        assert len(ram) == 0x20000
        # The uncompressed native snapshot contains one contiguous WRAM block.
        # Locate it using the original tournament tables, then compare other
        # independent campaign blocks at that same offset, not just counters.
        tables = ram[0xdc00:0xdeff]
        table_offset = data.find(tables, 192)
        assert table_offset >= 192 and data.find(tables, table_offset+1) == -1
        ram_offset = table_offset-0xdc00
        assert ram_offset >= 192 and ram_offset+0x20000 <= len(data)
        expected_ram = data[ram_offset:ram_offset+0x20000]
        preserved_ranges = ((0xdc00, 0xdeff), (0x1640, 0x1698),
                            (0x1e4a, 0x1e70), (0x0da0, 0x0da2), (0x0ea0, 0x0ea2))
        for begin, end in preserved_ranges:
            assert ram[begin:end] == expected_ram[begin:end], f"Campaign block {begin:04x} changed"
        word = lambda a: int.from_bytes(ram[a:a+2], "little")
        assert word(0x32) == 6 and word(0x70) == 12
        assert (word(0x1648)&0x24) == (4 if opts.kind == "cup" else 0x20)
        assert (word(0x1640), word(0x1652)) in candidates
        if opts.kind == "world":
            expected = 0 if "setup:" in label else 35 if "complete:" in label else generation-1
            assert word(0x1652) == expected, (generation, label, word(0x1652))
        row = dict(generation=generation, label=label, stage=word(0x1640), round=word(0x1652),
                   callback=ram[0x1446:0x1449].hex(), checkpoint_sha256=hashlib.sha256(data).hexdigest(),
                   continue_preserved_bytes=True, campaign_tables_settings_teams_preserved=True)
        cases.append(row)
        print(row, flush=True)
    assert cases, "No observed native checkpoints"
    if opts.expect_world_complete:
        assert opts.kind == "world"
        assert {case["round"] for case in cases if case["round"]} == set(range(1, 36))
        assert any(case["label"].startswith("World Series complete:") for case in cases)
    (output / "verification.json").write_text(json.dumps(dict(
        cases=cases, production_exe_sha256=hashlib.sha256((ROOT / "build/ISSDNative.exe").read_bytes()).hexdigest(),
        clones_only=True, source_campaign_modified=False), indent=2)+"\n")


if __name__ == "__main__":
    main()
