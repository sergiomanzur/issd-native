"""Compile and execute the real resampler/FIFO without booting a game."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


class AudioOutputTest(unittest.TestCase):
    def test_overflow_joins_pcm_at_the_queued_gap(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "A C compiler is required")
        dsp = (RUNTIME / "snes/dsp.c").read_text(encoding="utf-8")
        enqueue = dsp[dsp.index("  // Write into the output ring."):
                      dsp.index("  dsp->evenCycle = !dsp->evenCycle;")]
        source_text = ('#include "snes/dsp.h"\n#include "audio_trace.h"\n'
                       'static void queue_sample(Dsp *dsp, int totalL, int totalR) {\n'
                       + enqueue + '}\n'
                       + (ROOT / "tests/test_audio_overflow.c").read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory(prefix="issd-audio-overflow-") as directory:
            source = Path(directory) / "overflow.c"
            executable = Path(directory) / "overflow.exe"
            source.write_text(source_text, encoding="utf-8")
            subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra",
                            "-Werror", f"-I{RUNTIME}", str(source),
                            "-o", str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_starvation_fade_continues_across_callback_boundaries(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "A C compiler is required")
        rtl = (RUNTIME / "common_rtl.c").read_text(encoding="utf-8")
        dsp = (RUNTIME / "snes/dsp.c").read_text(encoding="utf-8")
        # Compile exact source slices, rather than duplicating the algorithm.
        # The rest of common_rtl.c needs the full generated game to link.
        resampler = rtl[rtl.index("#define RTL_AUDIO_NATIVE_RATE"):
                        rtl.index("void RtlRenderAudio(")]
        fifo = dsp[dsp.index("uint32_t dsp_available("):
                   dsp.index("uint32_t dsp_getSamples(")]
        harness = (ROOT / "tests/test_audio_output.c").read_text(encoding="utf-8")
        prelude = ('#include "types.h"\n#include "snes/dsp.h"\n'
                   '#include "audio_trace.h"\n')
        with tempfile.TemporaryDirectory(prefix="issd-audio-test-") as directory:
            source = Path(directory) / "audio_test.c"
            executable = Path(directory) / "audio_test.exe"
            source.write_text(prelude + fifo + resampler + harness, encoding="utf-8")
            subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra",
                            "-Werror", f"-I{RUNTIME}", str(source),
                            "-o", str(executable)], check=True)
            for arguments in ([], ["recover"]):
                with self.subTest(recovery=bool(arguments)):
                    result = subprocess.run([str(executable), *arguments],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
