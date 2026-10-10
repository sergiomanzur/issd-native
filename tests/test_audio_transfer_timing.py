"""Opt-in device regression for signal lost during retail sound transfers."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(os.environ.get("ISSD_RUN_AUDIO_TIMING") != "1",
                    reason="requires an otherwise idle machine and default audio device")
def test_retail_sound_transfers_do_not_drop_signal():
    result = subprocess.run([
        sys.executable, str(ROOT / "tests/test_audio_realtime.py"),
        "--exe", str(ROOT / "build/ISSDNative.exe"), "--device",
        "--config", "default", "--frames", "1200",
    ], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    directory = Path(re.search(r"Diagnostics: (.+)", result.stdout).group(1).strip())
    summary = json.loads((directory / "summary.json").read_text())
    assert summary["target_reached"] and summary["audio_device_opened"]
    assert summary["last_one_second_snapshot"]["dropped_audible"] == 0, (
        "Sound transfers discarded signal-bearing PCM", summary,
        str(directory / "run.log"))
