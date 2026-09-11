#!/usr/bin/env bash
# Fetch the SDL2 source tree the Android build needs, into deps/SDL2.
#
# Desktop links a prebuilt SDL2. Android cannot: SDL supplies the Java
# SDLActivity that owns the Android window, input and lifecycle, so the source
# tree has to be present and compiled as part of the app.
#
# Pinned to the same version the desktop build uses, so a bug fixed on one
# platform is not still present on the other. Not vendored into git because it
# is ~40MB of third-party source; this script makes it reproducible instead.
set -euo pipefail

SDL_VERSION="${SDL_VERSION:-2.32.10}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$REPO/deps/SDL2"
URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz"

if [ -f "$DEST/.version" ] && [ "$(cat "$DEST/.version")" = "$SDL_VERSION" ]; then
    echo "[fetch-sdl2] deps/SDL2 already at $SDL_VERSION"
    exit 0
fi

echo "[fetch-sdl2] fetching SDL2 $SDL_VERSION"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

curl -fsSL --retry 3 -o "$TMP/sdl2.tar.gz" "$URL" \
    || { echo "[fetch-sdl2] download failed: $URL" >&2; exit 1; }

tar -xzf "$TMP/sdl2.tar.gz" -C "$TMP"
SRC="$TMP/SDL2-${SDL_VERSION}"
[ -d "$SRC/android-project" ] \
    || { echo "[fetch-sdl2] archive has no android-project - wrong tarball?" >&2; exit 1; }

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$SRC" "$DEST"
printf '%s' "$SDL_VERSION" > "$DEST/.version"

echo "[fetch-sdl2] deps/SDL2 ready ($SDL_VERSION)"
echo "[fetch-sdl2]   java glue: $(find "$DEST/android-project" -name '*.java' | wc -l) files"
