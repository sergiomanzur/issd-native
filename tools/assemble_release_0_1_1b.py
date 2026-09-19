import os
import shutil
import zipfile

def main():
    rel_dir = os.path.join('dist', 'releases', 'v0.1.1b')
    win_dir = os.path.join(rel_dir, 'ISSDNative-v0.1.1b-windows-x64')
    android_dir = os.path.join(rel_dir, 'ISSDNative-v0.1.1b-android')
    
    os.makedirs(win_dir, exist_ok=True)
    os.makedirs(os.path.join(win_dir, 'mods'), exist_ok=True)
    os.makedirs(os.path.join(win_dir, 'saves'), exist_ok=True)
    os.makedirs(android_dir, exist_ok=True)
    
    # 1. Windows files
    shutil.copy2('ISSDNativeLauncher.exe', os.path.join(win_dir, 'ISSDNative.exe'))
    shutil.copy2(os.path.join('build', 'ISSDNative.exe'), os.path.join(win_dir, 'ISSDNativeCore.exe'))
    shutil.copy2('SDL2.dll', os.path.join(win_dir, 'SDL2.dll'))
    shutil.copy2('README.md', os.path.join(win_dir, 'README.md'))
    shutil.copy2('CHANGELOG.md', os.path.join(win_dir, 'CHANGELOG.md'))
    shutil.copy2('VERSION', os.path.join(win_dir, 'VERSION'))
    shutil.copy2(os.path.join('recomp', 'aot_boot_deny.txt'), os.path.join(win_dir, 'aot_boot_deny.txt'))
    
    rel_notes = (
        "ISSD Native v0.1.1b\n\n"
        "No ROM, cartridge data, copyrighted game assets, BIOS files, or commercial media are included.\n"
        "Place your own legally dumped International Superstar Soccer Deluxe (USA) ROM beside ISSDNative.exe or select it with the Windows picker on first run.\n"
        "Use the bundled empty mods folder or select another mods folder when prompted.\n"
    )
    with open(os.path.join(win_dir, 'RELEASE_NOTES.txt'), 'w', encoding='utf-8') as f:
        f.write(rel_notes)
        
    # Zip Windows
    win_zip = os.path.join(rel_dir, 'ISSDNative-v0.1.1b-windows-x64.zip')
    with zipfile.ZipFile(win_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(win_dir):
            for d in dirs:
                full_d = os.path.join(root, d)
                rel_d = os.path.relpath(full_d, rel_dir)
                # Ensure directory entries exist in zip
                zf.write(full_d, arcname=rel_d)
            for f in files:
                full_f = os.path.join(root, f)
                rel_f = os.path.relpath(full_f, rel_dir)
                zf.write(full_f, arcname=rel_f)
    print(f"Created {win_zip} ({os.path.getsize(win_zip):,} bytes)")

    # 2. Android APK
    apk_source = os.path.join('android', 'app', 'build', 'outputs', 'apk', 'release', 'app-release.apk')
    apk_dest = os.path.join(rel_dir, 'ISSDNative-v0.1.1b-android.apk')
    shutil.copy2(apk_source, apk_dest)
    print(f"Copied APK to {apk_dest} ({os.path.getsize(apk_dest):,} bytes)")
    
    # 3. Android ZIP
    shutil.copy2(apk_dest, os.path.join(android_dir, 'ISSDNative-v0.1.1b-android.apk'))
    shutil.copy2('README.md', os.path.join(android_dir, 'README.md'))
    shutil.copy2('CHANGELOG.md', os.path.join(android_dir, 'CHANGELOG.md'))
    shutil.copy2('VERSION', os.path.join(android_dir, 'VERSION'))
    
    android_zip = os.path.join(rel_dir, 'ISSDNative-v0.1.1b-android.zip')
    with zipfile.ZipFile(android_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(android_dir):
            for f in files:
                full_f = os.path.join(root, f)
                rel_f = os.path.relpath(full_f, android_dir)
                zf.write(full_f, arcname=rel_f)
    print(f"Created {android_zip} ({os.path.getsize(android_zip):,} bytes)")

    # 4. Mod Packs
    def zip_mod(json_file, assets_dir, out_zip):
        with zipfile.ZipFile(out_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
            zf.write(os.path.join('mods', json_file), arcname=json_file)
            full_assets = os.path.join('mods', assets_dir)
            for root, dirs, files in os.walk(full_assets):
                for f in files:
                    full_f = os.path.join(root, f)
                    rel_f = os.path.relpath(full_f, 'mods')
                    zf.write(full_f, arcname=rel_f)
        print(f"Created {out_zip} ({os.path.getsize(out_zip):,} bytes)")

    zip_mod('liga_mx_expansion.json', 'liga_mx', os.path.join(rel_dir, 'liga_mx_expansion.zip'))
    zip_mod('world_cup_2026.json', 'world_cup_2026', os.path.join(rel_dir, 'world_cup_2026.zip'))

if __name__ == '__main__':
    main()
