"""Package current Windows/Android builds using an explicit file list.

Run after building both targets: python tools/assemble_release.py
ROMs, local configuration, captures and saves are never copied from build trees.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    version = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+-beta\.\d+", version):
        raise ValueError("Expected a beta version in VERSION")
    tag = f"v{version}"
    output = ROOT / "dist/releases" / tag
    output.mkdir(parents=True, exist_ok=True)
    metadata = json.loads((ROOT / "android/app/build/outputs/apk/release/output-metadata.json").read_text())
    element, = metadata["elements"]
    if element["versionName"] != version:
        raise ValueError("Rebuild Android: APK version does not match VERSION")
    apk_source = ROOT / "android/app/build/outputs/apk/release" / element["outputFile"]
    common = {"README.md": ROOT / "README.md", "CHANGELOG.md": ROOT / "CHANGELOG.md",
              "VERSION": ROOT / "VERSION", "RELEASE_NOTES.md": ROOT / f"docs/releases/{tag}.md",
              f"docs/releases/{tag}.md": ROOT / f"docs/releases/{tag}.md",
              "licenses/SNESRecomp.txt": ROOT / "deps/snesrecomp/LICENSE",
              "licenses/SDL2.txt": ROOT / "deps/SDL2/LICENSE.txt"}
    for name in ("GAMEPLAY_TWEAKS", "LOCAL_MULTIPLAYER", "CAMPAIGN_SAVES",
                 "CAMPAIGN_CARTRIDGE_FLOW", "PASSWORD_BRIDGE", "MODDING"):
        common[f"docs/{name}.md"] = ROOT / f"docs/{name}.md"
    for path in (ROOT / "deps/snesrecomp/third_party/psxrecomp_color_lut").glob("LICENSE-*.txt"):
        common[f"licenses/psxrecomp_color_lut/{path.name}"] = path

    def archive(name, files, prefix="", empty_dirs=()):
        destination = output / name
        # Validate before opening, so a missing input never replaces a valid ZIP.
        for path in files.values():
            if not path.is_file():
                raise FileNotFoundError(path)
        with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as package:
            for folder in empty_dirs:
                package.writestr(f"{prefix}{folder}/", b"")
            for relative, source in sorted(files.items()):
                package.write(source, f"{prefix}{relative}")
        return destination

    windows = dict(common, **{"ISSDNative.exe": ROOT / "build/ISSDNative.exe",
                             "SDL2.dll": ROOT / "build/SDL2.dll",
                             "aot_boot_deny.txt": ROOT / "recomp/aot_boot_deny.txt"})
    assets = [archive(f"ISSDNative-{tag}-windows-x64.zip", windows,
                      prefix=f"ISSDNative-{tag}-windows-x64/", empty_dirs=("mods",))]
    apk = output / f"ISSDNative-{tag}-android.apk"
    shutil.copy2(apk_source, apk)
    assets.append(apk)
    assets.append(archive(f"ISSDNative-{tag}-android.zip", dict(common, **{apk.name: apk})))
    tracked = subprocess.check_output(["git", "ls-files", "-z", "mods"], cwd=ROOT).decode().split("\0")
    for manifest, directory in (("liga_mx_expansion.json", "liga_mx"),
                                ("world_cup_2026.json", "world_cup_2026")):
        files = {path.removeprefix("mods/"): ROOT / path for path in tracked
                 if path == f"mods/{manifest}" or path.startswith(f"mods/{directory}/")}
        if manifest not in files:
            raise ValueError(f"Missing tracked mod manifest: {manifest}")
        assets.append(archive(f"{Path(manifest).stem}.zip", files))
    checksums = output / "SHA256SUMS.txt"
    checksums.write_text("".join(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
                                 for path in assets), encoding="ascii")
    for path in [*assets, checksums]:
        print(f"{path.relative_to(ROOT)} ({path.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
