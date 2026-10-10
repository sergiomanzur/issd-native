"""Summarize retained audio port/drop evidence without distributing PCM."""
import argparse
from collections import Counter, deque
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    args = parser.parse_args()
    prefix = str(args.prefix)
    stats = json.loads(Path(prefix + "-stats.json").read_text())
    ports = deque(maxlen=8)
    drops, frame_drops = [], Counter()
    counts = Counter()
    first = last = None
    frame = None
    for line in Path(prefix + "-events.jsonl").open():
        event = json.loads(line)
        if first is None:
            first = event["index"]
        last = event["index"]
        counts[event["type"]] += 1
        if event["type"] in (4, 5, 6, 7, 8):
            frame = event["aux"]
        if event["type"] in (4, 8):
            ports.append(event)
        if event["type"] == 2:
            # Port events carry a frame; DROP itself does not. This is temporal
            # association with the preceding port traffic, not causal proof.
            frame_drops[str(frame)] += event["aux"]
            drops.append(dict(event, preceding_port_frame=frame,
                              preceding_cpu_ports=list(ports)))
    summary = dict(stats=stats, first_retained_event=first,
                   last_retained_event=last, event_type_counts=dict(counts),
                   retained_dropped_pairs=sum(event["aux"] for event in drops),
                   dropped_pairs_by_preceding_port_frame=dict(frame_drops),
                   largest_drop_runs=sorted(drops, key=lambda event: event["aux"], reverse=True)[:12])
    output = Path(prefix + "-summary.json")
    output.write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({key: value for key, value in summary.items()
                      if key != "largest_drop_runs"}, indent=2))
    print(f"Details: {output}")


if __name__ == "__main__":
    main()
