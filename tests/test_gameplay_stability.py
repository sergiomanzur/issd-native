"""ROM-backed regression: a completed frame must return to the idle stack."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def run(exe, frames=1200, auto_start=60):
    with tempfile.TemporaryDirectory(prefix="issd-stability-") as work:
        result = subprocess.run([
            str(Path(exe).resolve()), "--headless", str(frames),
            "--auto-start", str(auto_start), "--dump-state", "state",
            str(ROOT / "International Superstar Soccer Deluxe (USA).sfc")],
            cwd=work, capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stderr
        assert "[interp_cap]" not in result.stderr, result.stderr[-4000:]
        cpu = re.search(r"\[CPU\] S=([0-9A-F]+) PB=([0-9A-F]+) NMI-busy=([0-9A-F]+)", result.stdout)
        assert cpu, "Missing CPU diagnostic"
        assert int(cpu[1], 16) == 0x1AF, f"Unbalanced NMI stack: {cpu[0]}"
        assert int(cpu[3], 16) == 0, f"NMI never completed: {cpu[0]}"
        ram = (Path(work) / "state.wram").read_bytes()
        assert int.from_bytes(ram[0x70:0x72], "little") <= 0x1D, "Corrupted game mode"
        # A hung renderer can keep returning successful host frames. Catch the
        # field symptom directly: three identical 120-frame captures while the
        # game reports its live-match mode means gameplay stopped moving.
        modes = {
            int(frame): int(mode, 16)
            for frame, mode in re.findall(
                r"\[Frame (\d+)\].*Mode2: 0x([0-9A-F]+)", result.stdout)
        }
        same_live = 0
        previous = None
        for shot in sorted(Path(work).glob("test_step_*.bmp")):
            frame = int(re.search(r"(\d+)", shot.stem)[1])
            digest = hashlib.sha256(shot.read_bytes()).digest()
            if modes.get(frame) == 0x08 and digest == previous:
                same_live += 1
            else:
                same_live = 0
            assert same_live < 3, f"Gameplay framebuffer froze by frame {frame}"
            previous = digest
        print(f"PASS: {frames} frames, idle stack balanced, NMI completed")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default=str(ROOT / "build-fixes/ISSDNative.exe"))
    parser.add_argument("--frames", type=int, default=1200)
    parser.add_argument("--auto-start", type=int, default=60)
    args = parser.parse_args()
    run(args.exe, args.frames, args.auto_start)
