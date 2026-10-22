#!/usr/bin/env bash
# Build a ROM-free AppImage from an already-built compatible Linux executable.
# BUILD_DIR=build-steamos APPIMAGE_TOOL=/path/to/appimagetool ./tools/make_appimage.sh
# Use --stage-only to inspect/test the AppDir without creating a SquashFS image.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-steamos}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT/dist/appimage}"
APPIMAGE_TOOL="${APPIMAGE_TOOL:-appimagetool}"
MAX_GLIBC="${MAX_GLIBC:-2.36}"
STAGE_ONLY=false
if [[ "${1:-}" == --stage-only ]]; then STAGE_ONLY=true; shift; fi
[[ $# == 0 ]] || { echo 'Usage: tools/make_appimage.sh [--stage-only]' >&2; exit 2; }
[[ -x "$BUILD_DIR/ISSDNative" ]] || { echo "Missing $BUILD_DIR/ISSDNative; build the steamos preset first." >&2; exit 1; }
mkdir -p -- "$OUTPUT_DIR"
OUTPUT_DIR="$(cd -- "$OUTPUT_DIR" && pwd -P)"
# Fresh, unique staging path; never delete a caller-computed directory.
APPDIR="$(mktemp -d "$OUTPUT_DIR/ISSDNative.AppDir.XXXXXX")"
fail() { echo "[AppImage] $* (staging retained at $APPDIR)" >&2; exit 1; }
check_glibc() {
    local highest
    highest=$(objdump -T "$1" | sed -n 's/.*(GLIBC_\([0-9][0-9.]*\)).*/\1/p' | sort -Vu | tail -1)
    [[ -n "$highest" ]] || fail "Cannot establish glibc floor for $1"
    [[ "$(printf '%s\n%s\n' "$MAX_GLIBC" "$highest" | sort -V | tail -1)" == "$MAX_GLIBC" ]] || fail "$1 requires glibc $highest; maximum is $MAX_GLIBC"
    echo "[AppImage] $(basename "$1"): glibc <= $highest"
}
check_glibc "$BUILD_DIR/ISSDNative"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/issd-native/mods" \
    "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/scalable/apps" "$APPDIR/usr/share/licenses/issd-native"
cp -- "$BUILD_DIR/ISSDNative" "$APPDIR/usr/bin/"
cp -- "$ROOT/recomp/aot_boot_deny.txt" "$APPDIR/usr/bin/"
dependencies=$(ldd "$BUILD_DIR/ISSDNative")
[[ "$dependencies" != *'not found'* ]] || fail 'Unresolved shared libraries'
# Host graphics/audio drivers, glibc, libstdc++, X11/Wayland and D-Bus stay
# supplied by SteamOS. Bundle SDL and its optional helper dependencies only.
while read -r name arrow path rest; do
    case "$name" in
        libSDL2-2.0.so.*|libdecor-0.so.*|libsamplerate.so.*|libXss.so.*)
            if [[ -n "${SDL_LIBRARY_DIR:-}" && -f "$SDL_LIBRARY_DIR/$name" ]]; then path="$SDL_LIBRARY_DIR/$name"; fi
            [[ "$arrow" == '=>' && -f "$path" ]] || fail "Cannot resolve $name"
            check_glibc "$path"
            cp -L -- "$path" "$APPDIR/usr/lib/$name" ;;
    esac
done <<< "$dependencies"
[[ -f "$APPDIR/usr/lib/libSDL2-2.0.so.0" ]] || fail 'No shared SDL2 library found'
(
    cd "$ROOT"
    # Explicit asset whitelist prevents ROMs, save states and build products
    # from accidentally entering a release when present in a local mods tree.
    find mods -type f \( -iname '*.json' -o -iname '*.bmp' -o -iname '*.png' -o -iname '*.md' -o -iname '*.txt' \) \
        -exec cp --parents -- '{}' "$APPDIR/usr/share/issd-native/" \;
)
cp -- "$ROOT/LICENSE" "$APPDIR/usr/share/licenses/issd-native/GPL-3.0.txt"
cp -- "$ROOT/deps/snesrecomp/LICENSE" "$APPDIR/usr/share/licenses/issd-native/SNESRecomp-LICENSE.txt"
cp -- "$ROOT/deps/snesrecomp/THIRD_PARTY_ATTRIBUTION.md" "$APPDIR/usr/share/licenses/issd-native/"
cp -- "$ROOT/docs/UPSTREAM_ADAPTATIONS.md" "$APPDIR/usr/share/licenses/issd-native/"
# Distribution-provided SDL copyright covers its bundled build and helpers.
if [[ -f /usr/share/doc/libsdl2-2.0-0/copyright ]]; then
    cp /usr/share/doc/libsdl2-2.0-0/copyright "$APPDIR/usr/share/licenses/issd-native/SDL2-copyright.txt"
fi
cp -- "$ROOT/packaging/appimage/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun" "$APPDIR/usr/bin/ISSDNative"
cp -- "$ROOT/packaging/appimage/issd-native.desktop" "$APPDIR/"
cp -- "$ROOT/packaging/appimage/issd-native.desktop" "$APPDIR/usr/share/applications/"
cp -- "$ROOT/packaging/appimage/issd-native.svg" "$APPDIR/"
cp -- "$ROOT/packaging/appimage/issd-native.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/"
ln -s issd-native.svg "$APPDIR/.DirIcon"
if command -v desktop-file-validate >/dev/null; then desktop-file-validate "$APPDIR/issd-native.desktop"; fi
echo "[AppImage] staged $APPDIR"
if $STAGE_ONLY; then exit 0; fi
command -v "$APPIMAGE_TOOL" >/dev/null || [[ -x "$APPIMAGE_TOOL" ]] || fail 'Set APPIMAGE_TOOL to appimagetool (see docs/APPIMAGE.md)'
runtime=()
if [[ -n "${APPIMAGE_RUNTIME:-}" ]]; then runtime=(--runtime-file "$APPIMAGE_RUNTIME"); fi
OUT="$OUTPUT_DIR/ISSDNative-x86_64.AppImage"
ARCH=x86_64 "$APPIMAGE_TOOL" "${runtime[@]}" "$APPDIR" "$OUT.new"
chmod +x "$OUT.new"
mv -f -- "$OUT.new" "$OUT"
echo "[AppImage] ready: $OUT"
