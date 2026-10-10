"""Passively retain valid native campaign checkpoints during a running probe.

Never launches, interrupts, or supplies input to the game. Each archived file
comes from the game's own atomic campaign writer, with its envelope validated.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--seconds", type=int, default=14400)
    opts = parser.parse_args()
    folder = opts.folder.resolve()
    archive = folder / "observed-checkpoints"
    archive.mkdir(exist_ok=True)
    seen = set()
    started = time.monotonic()
    while time.monotonic() - started < opts.seconds:
        for path in (folder / "owned-saves/campaigns").glob("*/campaign.sav"):
            try:
                data = path.read_bytes()
            except OSError:
                continue
            if len(data) < 192 or data[:4] != b"ISCE":
                continue
            if data[160:192] != hashlib.sha256(data[:160] + data[192:]).digest():
                continue
            generation = int.from_bytes(data[24:32], "little")
            key = (path.parent.name, generation)
            if key in seen:
                continue
            seen.add(key)
            target = archive / f"{key[0]}-{generation:03}.sav"
            target.write_bytes(data)
            row = dict(generation=generation, label=data[80:144].split(b"\0")[0].decode("ascii"),
                       sha256=hashlib.sha256(data).hexdigest(), file=target.name,
                       observed_elapsed_seconds=round(time.monotonic()-started, 3))
            with (archive / "index.jsonl").open("a") as output:
                output.write(json.dumps(row)+"\n")
            print(row, flush=True)
        if (folder / "outcome.json").exists():
            return
        time.sleep(2)


if __name__ == "__main__":
    main()
