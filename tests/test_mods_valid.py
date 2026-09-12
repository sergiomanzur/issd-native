"""Every pack that ships has to pass the checker that ships with it.

Two of them did not, when the checker was first written: legacy integer
`formation` keys, names longer than the cartridge stores, and ratings packed
so tightly that a whole squad quantised to the same two values.
"""
import subprocess
import sys
from pathlib import Path


def test_shipped_packs_validate():
    repo = Path(__file__).resolve().parents[1]
    packs = sorted((repo / "mods").glob("*.json"))
    assert packs, "no mod packs to check"
    result = subprocess.run(
        [sys.executable, str(repo / "tools/validate_mod.py"), "--strict"]
        + [str(p) for p in packs],
        capture_output=True, text=True, cwd=repo,
    )
    assert result.returncode == 0, result.stdout + result.stderr
