#!/usr/bin/env bash
# Linux x86-64 build, run inside WSL by build-all.sh.
#
# This is the fast-iteration target: it links against the host distro's glibc,
# which is newer than SteamOS's. Use the steamos target for anything you intend
# to run on a Deck.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

command -v cmake >/dev/null || { echo "cmake missing: apt-get install -y cmake ninja-build build-essential libsdl2-dev" >&2; exit 1; }
pkg-config --exists sdl2   || { echo "SDL2 dev missing: apt-get install -y libsdl2-dev" >&2; exit 1; }

cmake --preset linux
cmake --build --preset linux

echo "[build-linux] $(readlink -f build-linux/ISSDNative)"
file build-linux/ISSDNative | cut -c1-120
