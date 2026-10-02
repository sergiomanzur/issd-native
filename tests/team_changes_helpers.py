"""Original team management reached through controller input and a real stoppage."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"


def word(ram, address):
    return int.from_bytes(ram[address:address + 2], "little")


def native(folder, frames, script="0 NONE\n", *, seed=None, flags=3, extra=(), trace=False):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "owned-saves").mkdir(exist_ok=True)
    (folder / "isolated.cfg").write_text(
        "engine_mode=1\ninternal_res=0\ntrue_widescreen=0\n"
        f"gameplay_goalkeeper_ai={int(bool(flags & 1))}\n"
        f"gameplay_player_ai={int(bool(flags & 2))}\n", encoding="ascii")
    (folder / "input.txt").write_text(script, encoding="ascii")
    command = [str(EXE), "--rom", str(ROM), "--config", str(folder / "isolated.cfg"),
               "--mods-dir", str(folder / "empty-mods"), "--save-dir", str(folder / "owned-saves"),
               "--headless", str(frames),
               "--save-state", str(frames), "--dump-state", str(folder / "state"),
               "--screenshot", str(folder / "final.bmp"), *extra]
    if "--auto-start" not in extra:
        command += ["--script", str(folder / "input.txt")]
    if seed is not None:
        (folder / "owned-saves/quicksave.sav").write_bytes(seed)
        command += ["--load-state", "1"]
    result = subprocess.run(command, cwd=folder, capture_output=True, text=True, timeout=120,
        env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy",
                 **({"ISSD_GAMEPLAY_TRACE": "1"} if trace else {})))
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "[interp_cap]" not in result.stderr
    return ((folder / "owned-saves/quicksave.sav").read_bytes(),
            (folder / "state.wram").read_bytes(), result)


def bootstrap(folder, flags=3):
    seed, ram, _ = native(folder, 600, flags=flags, extra=("--auto-start", "60"))
    assert word(ram, 0x32) == 6 and word(ram, 0x70) == 8
    return seed, ram


REQUEST_MENU = (1800, "0 NONE\n40 SELECT\n50 NONE\n")
OPEN_FORMATION = (300, "0 NONE\n20 DOWN\n30 NONE\n60 DOWN\n70 NONE\n100 A\n110 NONE\n")
CHANGE_FORMATION = (300, "0 NONE\n20 A\n30 NONE\n60 DOWN\n70 NONE\n100 A\n110 NONE\n140 B\n150 NONE\n")
EXIT_FORMATION = (250, "0 NONE\n20 A\n30 NONE\n")
OPEN_SQUAD_AFTER_FORMATION = (300, "0 NONE\n20 UP\n30 NONE\n60 A\n70 NONE\n")
# Starting outfield slot 1; LEFT opens the bench, whose first player is a keeper.
SUBSTITUTE = (500, "0 NONE\n20 DOWN\n30 NONE\n60 A\n70 NONE\n100 LEFT\n110 NONE\n180 DOWN\n190 NONE\n220 A\n230 NONE\n")
EXIT_SQUAD = (350, "0 NONE\n20 B\n30 NONE\n120 B\n130 NONE\n220 A\n230 NONE\n")
RESUME = (700, "0 NONE\n20 UP\n30 NONE\n60 A\n70 NONE\n")
ADVANCE_LIVE = (1200, "0 NONE\n40 B\n50 NONE\n")
