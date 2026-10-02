"""Exercise the production runner loaders and serializers without booting a ROM."""
from pathlib import Path
import subprocess

from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


def function(source, name):
    start = source.index(name)
    start = source.rfind("\n", 0, start) + 1
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def runner_source():
    rtl = (RUNTIME / "common_rtl.c").read_text()
    snes = (RUNTIME / "snes/snes.c").read_text()
    parts = [(ROOT / "tests/test_snapshot_transactional.c").read_text()]
    for filename, name in [("cpu.c", "cpu_saveload("),
                           ("apu.c", "apu_saveload("),
                           ("dsp.c", "dsp_saveload("),
                           ("spc.c", "spc_saveload("),
                           ("dma.c", "dma_saveload("),
                           ("ppu.c", "ppu_saveload(")]:
        parts.append(function((RUNTIME / "snes" / filename).read_text(), name))
    parts.append(snes[snes.index("static uint32_t s_saveload_version"):
                      snes.index("void snes_reset(")])
    parts.append(rtl[rtl.index("#define RTL_SAV_MAGIC"):
                     rtl.index("void RtlReset(")])
    parts.append(rtl[rtl.index("bool RtlSaveSnapshot("):
                     rtl.index("void RtlSaveLoad(")])
    return "\n".join(parts)


def test_rejected_snapshot_preserves_machine_and_host(tmp_path):
    source = tmp_path / "transaction.c"
    source.write_text(runner_source() + (ROOT / "tests/test_snapshot_transactional_main.c").read_text())
    exe = tmp_path / "transaction.exe"
    compile_c(exe, ROOT, [source], [RUNTIME, RUNTIME / "snes"])
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_issd_snapshot_extra_and_legacy_guest_only(tmp_path):
    source = tmp_path / "extra.c"
    source.write_text(runner_source() + (ROOT / "tests/test_snapshot_extra.c").read_text())
    exe = tmp_path / "extra.exe"
    compile_c(exe, ROOT, [source, ROOT / "ISSDNative/issd_snapshot.c",
               ROOT / "ISSDNative/issd_animation.c", ROOT / "ISSDNative/issd_pose_history.c"],
              [ROOT / "ISSDNative", RUNTIME, RUNTIME / "snes"])
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
