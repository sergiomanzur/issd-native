"""Package ISSD Mod Studio as a single .exe.

    python tools/build_mod_studio.py

Writes tools/mod_studio/dist/ISSDModStudio.exe. The build directories are kept
out of the repository's own `build/`, which belongs to CMake.

Three things have to travel with the code: the snapshot of what the
cartridge looks like (baked.json), because a frozen exe has no repository
to read; validate_mod.py, so the Check button runs the same rules the
command line does rather than a second copy of them; and assets/, the
pictures cut out of the game - the eight pitches and the player - because
without them the editor quietly goes back to drawing its own.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
PKG = os.path.join(HERE, "mod_studio")
DIST = os.path.join(PKG, "dist")
NAME = "ISSDModStudio"


def main() -> int:
    sys.path.insert(0, HERE)
    from mod_studio import repo

    # Re-read the C before packaging, so the exe can never ship a stale idea
    # of what the game supports.
    data = repo.bake(REPO)
    print("baked %d formations, %d team names, %d stadiums"
          % (len(data["formations"]), len(data["team_names"]),
             len(data["stock_stadiums"])))

    # A missing capture is not a crash, just a worse editor, so it would
    # ship unnoticed. Say so here instead.
    from mod_studio import preview
    missing = [n for n in (["pitch/slot%d.png" % i for i in range(8)]
                           + ["player/%02X.png" % (v << 4) for v in range(4)])
               if not os.path.isfile(os.path.join(preview.ASSETS, *n.split("/")))]
    if missing:
        print("missing captures: %s\n"
              "run python tools/capture_editor_assets.py"
              % ", ".join(missing))
        return 2

    try:
        import PyInstaller                     # noqa: F401
    except ImportError:
        print("PyInstaller is not installed. Install it with:\n"
              "    python -m pip install pyinstaller")
        return 2

    work = tempfile.mkdtemp(prefix="issd_studio_build_")
    sep = ";" if os.name == "nt" else ":"
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--noconfirm", "--clean", "--onefile", "--windowed",
        "--name", NAME,
        "--distpath", DIST,
        "--workpath", os.path.join(work, "work"),
        "--specpath", work,
        "--paths", HERE,
        "--add-data", "%s%s%s" % (os.path.join(PKG, "baked.json"), sep, "mod_studio"),
        "--add-data", "%s%s%s" % (os.path.join(HERE, "validate_mod.py"), sep, "."),
        "--add-data", "%s%s%s" % (os.path.join(PKG, "assets"), sep,
                                  os.path.join("mod_studio", "assets")),
        "--hidden-import", "PIL._tkinter_finder",
        os.path.join(HERE, "mod_studio_launch.py"),
    ]
    print(" ".join(cmd))
    result = subprocess.run(cmd, cwd=REPO)
    shutil.rmtree(work, ignore_errors=True)
    if result.returncode != 0:
        return result.returncode

    exe = os.path.join(DIST, NAME + (".exe" if os.name == "nt" else ""))
    if not os.path.isfile(exe):
        print("the build reported success but produced no executable")
        return 1
    print("\n%s  (%.1f MB)" % (exe, os.path.getsize(exe) / (1024 * 1024)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
