"""Relink the current Windows production build with an exit-only audio exporter.

The production executable and all source files stay unchanged. Requires the
existing CMake/Ninja build in build/, clang, and a matching configured toolchain.
"""
import json
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
WORK = BUILD / "ghidra-investigation/audio-probe"
RUNTIME = ROOT / "deps/snesrecomp/runner/src"


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    commands = json.loads(subprocess.check_output(
        ["ninja", "-C", str(BUILD), "-t", "compdb", "-x"], text=True))
    command = next(row["command"] for row in commands if row["output"] == "ISSDNative.exe")
    # Discard Ninja's shell wrapper and post-link copy, and execute only the
    # compiler with a structured argument list. Never execute copied shell text.
    linker = command.split(" && ", 1)[1].split(" && ", 1)[0].strip()
    args = [token.strip('"') for token in shlex.split(linker, posix=False)]
    source = WORK / "audio_trace_export.c"
    original = (RUNTIME / "audio_trace.c").read_text()
    marker = "static void stats_line(uint32_t ring_fill) {\n  if (s_stats_mode < 0) {"
    assert original.count(marker) == 1, "Stats initialization changed; review exporter registration"
    instrumented = original.replace(marker,
        "static void issd_export_audio_investigation(void);\n" + marker +
        "\n    atexit(issd_export_audio_investigation);")
    source.write_text(instrumented + "\n" +
                      (Path(__file__).with_name("audio_export_tail.c")).read_text())
    obj = WORK / "audio_trace_export.obj"
    subprocess.run([args[0], "-O3", "-DNDEBUG", "-D_CRT_SECURE_NO_WARNINGS",
                    "-D_DLL", "-D_MT", "-Xclang", "--dependent-lib=msvcrt",
                    "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(RUNTIME),
                    "-c", str(source), "-o", str(obj)], check=True)
    old_obj = "CMakeFiles/ISSDNative.dir/deps/snesrecomp/runner/src/audio_trace.c.obj"
    assert args.count(old_obj) == 1, "Production link did not contain the expected audio object"
    args[args.index(old_obj)] = str(obj)
    args[args.index("-o") + 1] = str(WORK / "ISSDNative.exe")
    for index, value in enumerate(args):
        if value.startswith(("/implib:", "/pdb:")):
            args[index] = value.split(":", 1)[0] + ":" + str(WORK / value.split(":", 1)[1])
    subprocess.run(args, cwd=BUILD, check=True)
    for name in ("SDL2.dll", "aot_boot_deny.txt"):
        shutil.copy2(BUILD / name, WORK / name)
    print(WORK / "ISSDNative.exe")


if __name__ == "__main__":
    main()
