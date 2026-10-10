"""Explore a normal Cup final boundary on a clone; never acceptance evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import probe_uninterrupted_campaign as probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("folder", type=Path)
    opts = parser.parse_args()
    source = opts.source.resolve()
    folder = opts.folder.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    driver = (source / "probe_script.c").read_text()
    driver = driver.replace("if (frame<4201)", "if (false)")
    driver = driver.replace("(cb==0x85d32e || cb==0x8bc8c0)", "(cb==0x85d32e || (cb==0x8bc8c0 && period==3))")
    assert "if (false)" in driver and "cb==0x8bc8c0 && period==3" in driver
    edited = folder / "boundary-input.c"
    edited.write_text(driver)
    original_copy = probe.shutil.copy2
    original_driver = (probe.ROOT / "tools/ghidra/campaign_probe_input.c").resolve()

    def exploratory_copy(src, dst, *args, **kwargs):
        return original_copy(edited if Path(src).resolve() == original_driver else src, dst, *args, **kwargs)

    probe.shutil.copy2 = exploratory_copy
    exe = probe.build_probe(folder)
    original_copy(source / "isolated.cfg", folder / "isolated.cfg")
    checkpoint = opts.checkpoint.resolve()
    saved = folder / "owned-saves/campaigns" / checkpoint.stem.rsplit("-", 1)[0] / "campaign.sav"
    saved.parent.mkdir(parents=True)
    saved.write_bytes(checkpoint.read_bytes())
    command = [str(exe), "--rom", str(probe.ROOT / "International Superstar Soccer Deluxe (USA).sfc"),
               "--headless", "100000", "--continue", "--script", "cup",
               "--config", str(folder / "isolated.cfg"), "--mods-dir", str(folder / "empty-mods"),
               "--save-dir", str(folder / "owned-saves"), "--dump-state", str(folder / "final")]
    (folder / "command.json").write_text(json.dumps(command, indent=2))
    with (folder / "run.log").open("w") as log:
        result = subprocess.run(command, cwd=folder, stdout=log, stderr=subprocess.STDOUT,
                                env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"), timeout=1800)
    data = saved.read_bytes()
    outcome = dict(exploratory_only=True, one_final_clone=True, native_continue=True,
                   source_checkpoint_sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
                   exit_code=result.returncode, generation=int.from_bytes(data[24:32], "little"),
                   label=data[80:144].split(b"\0")[0].decode("ascii"))
    (folder / "boundary-outcome.json").write_text(json.dumps(outcome, indent=2)+"\n")
    print(json.dumps(outcome, indent=2))


if __name__ == "__main__":
    main()
