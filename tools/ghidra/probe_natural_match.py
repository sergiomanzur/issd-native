"""Observe a fresh Cup match using input only, with untouched clocks and scores.

Runs in bounded chunks through the existing save/load harness, so this is not
an uninterrupted/device playtest. Emits exact sampled modes and final outcome;
never changes WRAM, tournament entrants, results, clocks, or score fixtures.
"""
import hashlib
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests"))
from test_campaign_full_acceptance import run, word, callback, campaign_file


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--chunks", type=int, default=120)
    args = parser.parse_args()
    if args.chunks < 1:
        parser.error("--chunks must be positive")
    folder = ROOT / "build/ghidra-investigation/natural-match"
    # A fresh directory is required; never overwrite an earlier diagnostic run.
    if args.resume:
        trace = json.loads((folder / "progression.json").read_text())
        start = trace[-1]["chunk"]
        ram = (folder / f"chunk-{start:02}.wram").read_bytes()
    else:
        folder.mkdir(exist_ok=False)
    (folder / "isolated.cfg").write_text("", encoding="ascii")
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1600, 100))
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(1800, 2900, 200))
    if not args.resume:
        ram = run(folder, "fresh", 3101, script, load=False)
        trace = []
        start = 0
        assert int.from_bytes(campaign_file(folder).read_bytes()[24:32], "little") == 1
    initial_generation = 1
    for index in range(start, args.chunks + 1):
        row = dict(chunk=index, simulated_frames=3101 + index * 1000,
                   mode=word(ram, 0x70), period=word(ram, 0xa8),
                   callback=callback(ram), stage=word(ram, 0x1640),
                   round=word(ram, 0x1652), clock=word(ram, 0x16d0),
                   display_clock=word(ram, 0x16d2),
                   score=[word(ram, 0xda2), word(ram, 0xea2)],
                   wram_sha256=hashlib.sha256(ram).hexdigest())
        if not trace or trace[-1]["chunk"] != index:
            trace.append(row)
        (folder / "progression.json").write_text(json.dumps(trace, indent=2) + "\n")
        print(row, flush=True)
        generation = int.from_bytes(campaign_file(folder).read_bytes()[24:32], "little")
        if generation > initial_generation and index > 0:
            (folder / "outcome.json").write_text(json.dumps(dict(
                result_checkpoint=True, sampled_final_state=row,
                generation=generation, checkpoints_between_chunks=True,
                clocks_and_scores_edited=False), indent=2) + "\n")
            return
        # Real gameplay buttons also dismiss original intros/stats/replays.
        inputs = "0 NONE\n"
        if word(ram, 0x70) == 8:
            inputs += "".join(f"{f} RIGHT,Y,B\n{f+40} NONE\n" for f in range(80, 950, 160))
        else:
            inputs += "".join(f"{f} A\n{f+10} NONE\n" for f in range(80, 900, 160))
            if word(ram, 0x70) == 0x13:
                inputs = "0 NONE\n100 START\n110 NONE\n"
        if index != args.chunks:
            ram = run(folder, f"chunk-{index+1:02}", 1000, inputs)
    (folder / "outcome.json").write_text(json.dumps(dict(
        result_checkpoint=False, bounded_frames=3101 + args.chunks * 1000, sampled_final_state=trace[-1],
        clocks_and_scores_edited=False), indent=2) + "\n")
    raise SystemExit("Natural input probe reached its bound without a committed result; inspect progression.json")


if __name__ == "__main__":
    main()
