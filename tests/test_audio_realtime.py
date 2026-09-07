"""Opt-in wall-clock audio diagnostics; never changes the user's configuration.

Example (muted real audio device, software-renderer stress):
  python tests/test_audio_realtime.py --device --config default --frames 1200
No --device uses SDL's dummy audio driver, whose clock is not a hardware oracle.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
PROFILES = {
    "default": {},
    "minimal": {"internal_res": 0, "vsync": 0,
                "window_width": 256, "window_height": 224},
    "worst": {"internal_res": 5, "aspect_ratio": 4, "target_fps": 240,
              "window_width": 2560, "window_height": 1080, "vsync": 0},
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build-fixes/ISSDNative.exe")
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "International Superstar Soccer Deluxe (USA).sfc")
    parser.add_argument("--frames", type=int, default=1200)
    parser.add_argument("--auto-start", type=int, default=180)
    parser.add_argument("--config", choices=PROFILES, default="default")
    parser.add_argument("--device", action="store_true",
                        help="Open the real default audio device; playback stays muted")
    parser.add_argument("--video", choices=("software", "accelerated"), default="software",
                        help="Software runs invisibly; accelerated opens a native game window")
    args = parser.parse_args()
    executable, rom = args.exe.resolve(), args.rom.resolve()
    if not executable.is_file() or not rom.is_file():
        parser.error("The executable and ROM must exist")
    if args.frames < 1:
        parser.error("--frames must be positive")

    # A fresh cwd isolates config, save files, mods and any diagnostic images.
    directory = Path(tempfile.mkdtemp(prefix=f"issd-audio-{args.config}-"))
    config = dict(PROFILES[args.config], master_volume=0)
    (directory / "issd_config.json").write_text(json.dumps(config, indent=2) + "\n")
    environment = os.environ.copy()
    if args.device:
        environment.pop("SDL_AUDIODRIVER", None)
    else:
        environment["SDL_AUDIODRIVER"] = "dummy"
    if args.video == "software":
        environment.update(SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software")
    else:
        environment.pop("SDL_VIDEODRIVER", None)
        environment.pop("SDL_RENDER_DRIVER", None)
    environment["SNESRECOMP_AUDIO_STATS"] = str(directory / "stats.txt")
    command = [str(executable), "--frames", str(args.frames), "--auto-start",
               str(args.auto_start), str(rom)]
    print(f"Diagnostics: {directory}", flush=True)
    started = time.monotonic()
    timed_out = False
    with (directory / "run.log").open("w", encoding="utf-8") as output:
        try:
            result = subprocess.run(command, cwd=directory, env=environment,
                                    stdout=output, stderr=subprocess.STDOUT,
                                    timeout=max(45, args.frames / 20 + 30))
            returncode = result.returncode
        except subprocess.TimeoutExpired:
            timed_out, returncode = True, 1
    log = (directory / "run.log").read_text(encoding="utf-8", errors="replace")
    stats_path = directory / "stats.txt"
    rows = []
    columns = None
    if stats_path.exists():
        for line in stats_path.read_text().splitlines():
            if line.startswith("# "):
                columns = line[2:].split()
            elif columns:
                values = line.split()
                if len(values) == len(columns):
                    rows.append(dict(zip(columns, map(int, values))))
    summary = {
        "executable": str(executable), "config": args.config,
        "audio_driver": "default device (muted)" if args.device else "dummy",
        "video": args.video, "frames_requested": args.frames,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "returncode": returncode, "timed_out": timed_out,
        "audio_device_opened": "[Audio] SDL Audio Device opened" in log,
        "target_reached": "[Done] Target frame count reached" in log,
        "last_one_second_snapshot": rows[-1] if rows else None,
        "snapshots": len(rows),
    }
    (directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    # Counts are evidence, not a claim of audible quality. Overflow joins keep
    # raw DSP drop counters intact, and software video can still overload a CPU.
    return 0 if (returncode == 0 and summary["target_reached"]
                 and summary["audio_device_opened"] and rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
