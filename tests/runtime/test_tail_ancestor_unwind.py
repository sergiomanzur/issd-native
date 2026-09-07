"""Execute generated AOT tail propagation against the runtime skip resolver."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / "deps/snesrecomp"
sys.path.insert(0, str(ENGINE / "Recompiler"))
sys.path.insert(0, str(ENGINE / "tests/v2"))
from _helpers import make_lorom_bank0
from v2 import codegen
from v2.emit_function import emit_function


class TailAncestorUnwindTest(unittest.TestCase):
    def test_tail_host_frames_consume_ancestor_skip_levels(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "A C compiler is required")
        saved = dict(codegen._NAME_RESOLVER)
        try:
            codegen.set_name_resolver({0x8001: "TailTarget"})
            emitted = emit_function(
                make_lorom_bank0({0x8000: bytes([0xEA, 0x60])}),
                bank=0, start=0x8000, end=0x8001, entry_m=1, entry_x=1,
                func_name="TailOwner")
        finally:
            codegen.set_name_resolver(saved)
        tail_statement = next(line for line in emitted.splitlines()
                              if "tail-call past end:" in line)
        infra = (ENGINE / "runner/src/common_cpu_infra.c").read_text(encoding="utf-8")
        resolver = re.search(
            r"int cpu_resolve_ancestor_skip\(uint16_t ret_s\) \{.*?^\}",
            infra, re.DOTALL | re.MULTILINE)
        self.assertIsNotNone(resolver)
        harness = (ROOT / "tests/runtime/tail_ancestor_unwind_test.c").read_text()
        harness = harness.replace("/* ACTUAL_RUNTIME_RESOLVER */", resolver.group())
        harness = harness.replace("/* ACTUAL_EMITTED_TAIL */", tail_statement)
        with tempfile.TemporaryDirectory(prefix="issd-tail-unwind-") as directory:
            source = Path(directory) / "tail_test.c"
            executable = Path(directory) / "tail_test.exe"
            source.write_text(harness, encoding="utf-8")
            subprocess.run([compiler, "-std=c11", "-O1", "-Wall", "-Wextra",
                            f"-I{ENGINE / 'runner/src'}", str(source),
                            "-o", str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
