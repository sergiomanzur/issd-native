"""Build/run a separate input-only acceptance executable in one process.

Only diagnostic main changes: disable periodic screenshot files and stop at
the original terminal state. All gameplay/runtime objects are unchanged.
No save loading, password import, or game-memory/score/clock writes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def args(command):
    return [part.strip('"') for part in shlex.split(command, posix=False)]


def validate_terminal(outcome, rows, ram):
    """Reject a clean exit that did not finish an original campaign path."""
    if outcome["exit_code"]:
        raise ValueError("Campaign process did not exit cleanly")
    if len(ram) != 0x20000:
        raise ValueError("Missing complete terminal WRAM capture")
    word = lambda address: int.from_bytes(ram[address:address+2], "little")
    if outcome["kind"] == "world":
        if (outcome["completed_rounds"] != list(range(1, 36)) or
            outcome["generation"] < 36 or not outcome["label"].startswith("World Series complete:") or
            word(0x70) != 12 or word(0x1652) != 35 or ram[0x1446:0x1449] != bytes.fromhex("ee948b")):
            raise ValueError("World Series did not commit all 35 original results")
        return "complete"
    if outcome["label"].startswith("Cup complete:") and word(0x1640) == 9 and outcome["generation"] > 1:
        return "champion"
    observed_elimination = any(row["mode"] == 12 and row["callback"] == "8bc1f6" for row in rows)
    menu = word(0x32) == 6 and word(0x70) == 12 and word(0x1538) == 0x9d72 and word(0x153a) == 0xa4
    title = word(0x70) == 0x1a and (word(0x1648)&0x24) != 4
    boot = word(0x70) == 0
    if observed_elimination and (menu or title or boot) and outcome["generation"] > 1:
        return "eliminated"
    raise ValueError("Cup did not reach a verified championship or elimination terminal")


def build_probe(folder, input_driver=None):
    build = ROOT / "build"
    rows = json.loads(subprocess.check_output(
        ["ninja", "-C", str(build), "-t", "compdb", "-x"], text=True))
    replacements = {}
    for suffix in ("/issd_script.c", "/main.c"):
        row = next(r for r in rows if r["file"].replace("\\", "/").endswith(suffix)
                   and "/ISSDNative.dir/" in "/" + r["output"])
        source = folder / "probe_script.c"
        if suffix == "/issd_script.c":
            shutil.copy2(input_driver or ROOT / "tools/ghidra/campaign_probe_input.c", source)
        if suffix == "/main.c":
            source = folder / "probe_main.c"
            original = (ROOT / "ISSDNative/main.c").read_text()
            needle = "if (g_headless && (frame_count % 120 == 0))"
            assert original.count(needle) == 1
            original = original.replace(needle, "if (false)")
            source.write_text(original + "\nvoid issd_probe_finish(unsigned frame) { g_target_frames = frame + 1; }\n")
        command = args(row["command"])
        output = folder / ("probe_script.obj" if suffix == "/issd_script.c" else "probe_main.obj")
        command[command.index("-o")+1] = str(output)
        command[command.index("-c")+1] = str(source)
        if "-MF" in command:
            command[command.index("-MF")+1] = str(output) + ".d"
        if "-MT" in command:
            command[command.index("-MT")+1] = str(output)
        command += ["-I", str(ROOT / "ISSDNative")]
        subprocess.run(command, cwd=build, check=True)
        replacements[row["output"]] = str(output)
    link = next(r["command"] for r in rows if r["output"] == "ISSDNative.exe")
    link = link.split(" && ", 1)[1].split(" && ", 1)[0].strip()
    command = args(link)
    exe = folder / "campaign_probe.exe"
    command[command.index("-o")+1] = str(exe)
    command = [replacements.get(part, part) for part in command]
    subprocess.run(command, cwd=build, check=True)
    for library in build.glob("*.dll"):
        shutil.copy2(library, folder / library.name)
    (folder / "build-manifest.json").write_text(json.dumps({
        "input_driver_sha256": hashlib.sha256((folder / "probe_script.c").read_bytes()).hexdigest(),
        "diagnostic_main_sha256": hashlib.sha256((folder / "probe_main.c").read_bytes()).hexdigest(),
        "exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "production_exe_sha256": hashlib.sha256((build / "ISSDNative.exe").read_bytes()).hexdigest(),
        "production_source_main_sha256": hashlib.sha256((ROOT / "ISSDNative/main.c").read_bytes()).hexdigest(),
        "replaced_objects": replacements,
        "linked_object_sha256": {part: hashlib.sha256((build / part).read_bytes()).hexdigest()
                                 for part in command if part.endswith((".obj", ".o"))},
    }, indent=2)+"\n")
    return exe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("cup", "world"))
    parser.add_argument("--frames", type=int, default=4000000)
    parser.add_argument("--folder", type=Path, required=True)
    opts = parser.parse_args()
    if opts.frames < 1:
        parser.error("--frames must be positive")
    folder = opts.folder.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    exe = build_probe(folder)
    (folder / "isolated.cfg").write_text("engine_mode=1\ninternal_res=0\ntrue_wide=0\n")
    command = [str(exe), "--rom", str(ROOT / "International Superstar Soccer Deluxe (USA).sfc"),
               "--headless", str(opts.frames), "--script", opts.kind,
               "--config", str(folder / "isolated.cfg"), "--mods-dir", str(folder / "empty-mods"),
               "--save-dir", str(folder / "owned-saves"),
               "--dump-state", str(folder / "final")]
    (folder / "command.json").write_text(json.dumps(command, indent=2))
    with (folder / "run.log").open("w") as log:
        result = subprocess.run(command, cwd=folder, stdout=log, stderr=subprocess.STDOUT,
                                env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"),
                                timeout=14400)
    if not (folder / "progression.jsonl").exists():
        raise SystemExit(f"Probe did not start: exit {result.returncode}; inspect {folder / 'run.log'}")
    rows = [json.loads(line) for line in (folder / "progression.jsonl").read_text().splitlines()]
    saves = list((folder / "owned-saves/campaigns").glob("*/campaign.sav"))
    data = saves[0].read_bytes() if len(saves) == 1 else b""
    if len(data) < 192 or data[160:192] != hashlib.sha256(data[:160]+data[192:]).digest():
        raise SystemExit("Missing or invalid native campaign checkpoint")
    outcome = dict(kind=opts.kind, exit_code=result.returncode, last=rows[-1],
                   generation=int.from_bytes(data[24:32], "little"),
                   label=data[80:144].split(b"\0")[0].decode("ascii"),
                   one_process=True, save_loads=0, clocks_and_scores_edited=False,
                   input_driver_sha256=hashlib.sha256((folder / "probe_script.c").read_bytes()).hexdigest())
    outcome["completed_rounds"] = sorted({row["round"] for row in rows
        if row["mode"] == 12 and row["callback"] in ("8b9496", "8b94ee")})
    outcome["exe_sha256"] = hashlib.sha256(exe.read_bytes()).hexdigest()
    outcome["rom_sha256"] = hashlib.sha256((ROOT / "International Superstar Soccer Deluxe (USA).sfc").read_bytes()).hexdigest()
    validation_error = None
    try:
        capture = folder / "final.wram"
        outcome["campaign_outcome"] = validate_terminal(outcome, rows, capture.read_bytes() if capture.exists() else b"")
        outcome["terminal_verified"] = True
    except ValueError as error:
        validation_error = str(error)
        outcome["terminal_verified"] = False
        outcome["validation_error"] = validation_error
    (folder / "outcome.json").write_text(json.dumps(outcome, indent=2)+"\n")
    print(json.dumps(outcome, indent=2), flush=True)
    if validation_error:
        raise SystemExit(validation_error)


if __name__ == "__main__":
    main()
