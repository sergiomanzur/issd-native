"""Read-only compact progress stream for a running diagnostic campaign."""
from pathlib import Path
import sys
import time

folder = Path(sys.argv[1])
last = None
while not (folder / "outcome.json").exists():
    lines = (folder / "progression.jsonl").read_text().splitlines()
    if lines and lines[-1] != last:
        last = lines[-1]
        print(last, flush=True)
    time.sleep(30)
print((folder / "outcome.json").read_text(), flush=True)
