#!/usr/bin/env bash
# SteamOS build, run inside the Steam Linux Runtime 3.0 "sniper" SDK container.
#
# Why a container: SteamOS 3.x ships roughly glibc 2.37, while a typical desktop
# distro is newer. A binary linked against a newer glibc will not start on a
# Deck. Sniper is Debian 12 (glibc 2.36), which is Valve's supported base for
# native Linux Steam titles, so what it produces runs on the Deck and on any
# distro that can run Steam.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

echo "[build-steamos] $( . /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}" ) / glibc $(ldd --version | head -1 | awk '{print $NF}')"

if ! pkg-config --exists sdl2; then
    echo "[build-steamos] ERROR: libsdl2-dev is not present in this image." >&2
    echo "                The sniper SDK normally ships it; check the image tag." >&2
    exit 1
fi

cmake --preset steamos
cmake --build --preset steamos

BIN=build-steamos/ISSDNative
echo
echo "[build-steamos] $(file "$BIN" | cut -c1-120)"

# The whole point of building here is the glibc floor. Report it so a
# regression is visible in the build log rather than on the device.
echo "[build-steamos] highest glibc symbol required:"
objdump -T "$BIN" 2>/dev/null \
  | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1 | sed 's/^/                /'
echo "[build-steamos] shared library dependencies:"
objdump -p "$BIN" 2>/dev/null | awk '/NEEDED/ {print "                " $2}'
