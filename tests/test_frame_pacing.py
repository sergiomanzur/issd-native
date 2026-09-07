"""Compile and run the presentation-starvation pacing contract."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FramePacingTest(unittest.TestCase):
    def test_slow_simulation_has_a_bounded_present_skip(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="issd-frame-pacing-") as directory:
            executable = Path(directory) / "frame_pacing.exe"
            result = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ROOT / 'ISSDNative'}",
                str(ROOT / "tests/test_frame_pacing.c"), "-o", str(executable),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
