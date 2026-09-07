"""ROM-free regression for AOT subroutine tails executed inside an NMI."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


class InterruptTailTest(unittest.TestCase):
    def test_subroutine_returns_preserve_nmi_ownership(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="issd-interrupt-test-") as directory:
            executable = Path(directory) / "interrupt_tail_test.exe"
            sources = [ROOT / "tests/runtime/interrupt_tail_test.c"]
            sources += [RUNTIME / "snes" / name for name in (
                "interp816.c", "interp_bridge.c", "tier2_capture.c", "cx4.c")]
            subprocess.run([
                compiler, "-std=c11", "-O1", "-DSNESRECOMP_TIER2_TEST=1",
                f"-I{RUNTIME}", f"-I{RUNTIME / 'snes'}",
                # The bridge's existing S-DD1 diagnostic needs this declaration.
                "-include", str(RUNTIME / "snes/sdd1.h"),
                *map(str, sources), "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], cwd=directory,
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
