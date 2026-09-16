"""Exercise the production CPU handshake against real SPC700 instructions."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


class AudioHandshakeTest(unittest.TestCase):
    def test_commentary_handshakes(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "A C compiler is required")
        rtl = (RUNTIME / "common_rtl.c").read_text(encoding="utf-8")
        start = rtl.index("bool RtlApuWriteWaitEcho(")
        handshake = rtl[start:rtl.index("\n#ifdef SNESRECOMP_INTERP_PROFILE", start)]
        support = (ROOT / "deps/snesrecomp/tests/runtime_dispatch/apu_port_guest_time_test.c").read_text(encoding="utf-8")
        support = support[:support.index("static int check(")]
        harness = (ROOT / "tests/test_audio_handshake.c").read_text(encoding="utf-8")
        prelude, cases = harness.split("/* PRODUCTION HANDSHAKE */")
        with tempfile.TemporaryDirectory(prefix="issd-handshake-") as directory:
            source = Path(directory) / "handshake.c"
            executable = Path(directory) / "handshake.exe"
            source.write_text(support + prelude + handshake + cases, encoding="utf-8")
            subprocess.run([
                compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-Wno-error=unknown-pragmas", "-Wno-error=comment",
                "-ffunction-sections", "-fdata-sections", f"-I{RUNTIME}",
                f"-I{RUNTIME / 'snes'}", str(source),
                *(str(RUNTIME / "snes" / name) for name in ("apu.c", "spc.c", "dsp.c")),
                "-Wl,--gc-sections", "-o", str(executable),
            ], check=True)
            for scenario in ("cleared-command", "queued-release", "word-echo",
                             "cleared-word", "post-echo-clear"):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(executable), scenario], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
