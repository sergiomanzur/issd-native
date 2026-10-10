"""Fresh continuous run of a previously frozen input driver, with a host cap.

This does not load or replay a game snapshot. It recompiles the input source
recorded by an earlier probe and starts the retail campaign from boot.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import probe_uninterrupted_campaign as probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--frames", type=int, default=900000)
    parser.add_argument("--normal-ceremony-advance", action="store_true",
                        help="Keep ordinary A input through the normal final menu before stopping")
    opts = parser.parse_args()
    source = opts.source.resolve() / "probe_script.c"
    manifest = json.loads((source.parent / "build-manifest.json").read_text())
    assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["input_driver_sha256"]
    if opts.normal_ceremony_advance:
        driver = source.read_text()
        old = "(cb==0x85d32e || cb==0x8bc8c0)"
        assert driver.count(old) == 1, "Unexpected source terminal policy"
        driver = driver.replace(old, "(cb==0x85d32e || (cb==0x8bc8c0 && period==3))")
        staged = opts.folder.resolve().parent / (opts.folder.name + "-source")
        staged.mkdir(parents=True, exist_ok=False)
        (staged / "probe_script.c").write_text(driver)
        (staged / "source-provenance.json").write_text(json.dumps(dict(
            original_source=str(source), original_driver_sha256=manifest["input_driver_sha256"],
            diagnostic_change="Ordinary A through normal ceremony; only period-3 CBC8C0 can stop"), indent=2)+"\n")
        source = staged / "probe_script.c"
    original_copy = probe.shutil.copy2
    original_driver = (probe.ROOT / "tools/ghidra/campaign_probe_input.c").resolve()

    def frozen_copy(src, dst, *args, **kwargs):
        return original_copy(source if Path(src).resolve() == original_driver else src, dst, *args, **kwargs)

    probe.shutil.copy2 = frozen_copy
    sys.argv = [sys.argv[0], "cup", "--frames", str(opts.frames), "--folder", str(opts.folder.resolve())]
    probe.main()


if __name__ == "__main__":
    main()
