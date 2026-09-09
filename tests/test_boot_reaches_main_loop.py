"""The game must reach its main loop from a clean environment.

Three routines ($00B527, $00C2F4, $00C325) mishandle the SPC700 port-echo
handshake when run from their AOT bodies, so the reset vector spins forever on
'[apu] port echo timeout' and no window is ever created. The runner already has
a deny set that tiers those nodes down to the interpreter; the boot path has to
apply it by default rather than relying on an environment variable a player
will never set.

python -m pytest tests/test_boot_reaches_main_loop.py
"""
import os
from pathlib import Path
import subprocess

import pytest

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build-fixes/ISSDNative.exe"
ROM = REPO / "International Superstar Soccer Deluxe (USA).sfc"


@pytest.mark.skipif(not EXE.exists() or not ROM.exists(), reason="needs built exe and ROM")
def test_reset_vector_completes_without_env_override(tmp_path):
    # A player's environment carries no SNESRECOMP_* tuning, so strip any that
    # this shell happens to export before judging the boot path.
    env = {k: v for k, v in os.environ.items() if not k.startswith("SNESRECOMP_")}

    result = subprocess.run(
        [str(EXE), "--headless", "60", "--screenshot", "frame.bmp", str(ROM)],
        cwd=tmp_path, capture_output=True, text=True, timeout=180, env=env,
    )
    log = result.stdout + result.stderr
    (tmp_path / "run.log").write_text(log, encoding="utf-8")

    assert result.returncode == 0, log[-4000:]
    assert "Reset sequence completed" in log, (
        "reset vector never finished: the boot APU handshake is spinning\n"
        + log[-4000:])
    # One straggler is tolerable; a wedged handshake produces hundreds.
    assert log.count("port echo timeout") < 5, (
        f"{log.count('port echo timeout')} APU port echo timeouts during boot")
