# Mod Studio in this beta bundle

## Try the included stadiums

On Windows, open `ISSDModStudio.exe`. Choose **Open** and select
`mods/independent_stadium_example.json`. Select MEADOW (ID 8) or HARBOR (ID 31),
then **Test in game**. Choose the compatible `ISSDNative.exe` and your own
retail USA cartridge when prompted. **Stop test** closes only that test session.
The standalone editor download requires a separately downloaded game executable.

To use the pack directly in the game, enable **Independent stadium examples**
in the mod list and start a fresh match. Keep the manifest, `meadow/` and
`harbor/` in the same `mods/` directory. On Android, extract the Android ZIP
and select its `mods/` folder with the app's mod-folder importer. Physical
Android gameplay remains outside this release's automated acceptance.

## Edit a profile

Use **Geometry** to change pitch length. Values are engine units, separate
from the display-yard fields. Version 1 retains template width/scenery and
shortens lengths in 32-unit steps. Invalid drafts disable Test.

Use **Artwork** to create editable resources or import an 8px tile atlas.
Image scales 2, 4 and 8 create local HD replacements and a native fallback.
**Preview compiled stadium** shows quantized native pixels and final maps;
the initial-camera region corresponds to the dashed window in Geometry.

Changing geometry makes generated maps stale. Use **Recompile pitch maps**
to preserve imported artwork/placements while rebuilding those maps.
Compiler capacity errors must be resolved before testing or exporting.
Use **Export portable pack** to save a validated, shareable dependency set.

## Editor source

The bundled desktop editor source needs Python 3.10+, Pillow and Tcl/Tk.
From the platform archive's root, run:

```text
python mod-studio-source/tools/mod_studio_launch.py
```

The source bundle includes its baked facts and images; it does not require
the game repository. Android has no native Mod Studio GUI in this release.
See `docs/INDEPENDENT_STADIUM_ACCEPTANCE.md` for the measured engine/art limits.
