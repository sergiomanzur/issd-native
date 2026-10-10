# Independent stadium examples

Meadow (ID8,1728×576) and Harbor (ID31,1664×576) use original layout0's
bounded engine allocations with different pitch lengths and original artwork.
They contain no cartridge file, extracted character pixels, palette, or saved
game. `tools/mod_studio/examples.py` generates every map, glyph and color
without reading a cartridge. The simplified pitch artwork illustrates the
allocation contract; the original game still supplies players, goals and HUD.

Open `mod.json` in Mod Studio (or `mods/independent_stadium_example.json` in a
platform release bundle), select either stadium and use **Test in game**.
Choose your cartridge and the built game executable when prompted. The editor
creates an isolated test session. Use **Stop test** to close its child process.
**Export portable pack** copies the validated dependencies to a new directory.

The picker includes32slots to demonstrate ID31. Unnamed intermediate entries
retain the original fallback behavior. This is why the validator reports
warnings about unnamed slots9–30. The sample does not add teams.
