# Upstream adaptations

The pre-game ROM validation and configuration flow in
`ISSDNative/issd_launcher_model.c`, and the AppDir/writable-state packaging
approach in `tools/make_appimage.sh` and `packaging/appimage/AppRun`, adapt
ideas/source from [Mabec-games/iss-deluxed](https://github.com/Mabec-games/iss-deluxed),
commit `54344cbddf6a7f3e02cee2000b60750d4f663be0`, distributed under GPL-3.0.
Credit to the Mabec-games contributors and the upstream ISSD Native authors.
The Matchday launcher graphics, layout and icon are original code-native artwork.
No fork logo, announcer audio, online implementation or ROM is included.

The runtime also includes SNESRecomp and its separately licensed components;
see `deps/snesrecomp/LICENSE` and `deps/snesrecomp/THIRD_PARTY_ATTRIBUTION.md`.
