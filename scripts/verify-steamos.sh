#!/usr/bin/env bash
# Verify the SteamOS artifact inside the same runtime it was built for.
#
# Checks, in order of what would actually bite on a Deck:
#   1. the glibc floor is at or below what SteamOS 3.x provides
#   2. every shared library it needs resolves
#   3. it runs a real match headless against a user-supplied ROM
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"
BIN=build-steamos/ISSDNative
ROM="International Superstar Soccer Deluxe (USA).sfc"

# SteamOS 3.x is glibc 2.37. Anything at or below that loads.
STEAMOS_GLIBC_MAJOR=2
STEAMOS_GLIBC_MINOR=37

[ -x "$BIN" ] || { echo "missing $BIN - run build-all.sh --only steamos first" >&2; exit 1; }

echo "=== 1. glibc floor ==="
HIGHEST=$(objdump -T "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
echo "    requires at most: $HIGHEST"
V=${HIGHEST#GLIBC_}
MAJ=${V%%.*}; REST=${V#*.}; MIN=${REST%%.*}
if [ "$MAJ" -gt "$STEAMOS_GLIBC_MAJOR" ] || \
   { [ "$MAJ" -eq "$STEAMOS_GLIBC_MAJOR" ] && [ "$MIN" -gt "$STEAMOS_GLIBC_MINOR" ]; }; then
    echo "    FAIL: newer than SteamOS ${STEAMOS_GLIBC_MAJOR}.${STEAMOS_GLIBC_MINOR}" >&2
    exit 1
fi
echo "    OK (SteamOS provides ${STEAMOS_GLIBC_MAJOR}.${STEAMOS_GLIBC_MINOR})"

echo
echo "=== 2. shared libraries resolve ==="
if ldd "$BIN" | grep -q 'not found'; then
    ldd "$BIN" | grep 'not found' >&2
    echo "    FAIL: unresolved libraries" >&2
    exit 1
fi
ldd "$BIN" | awk '{print "    " $0}' | head -8
echo "    OK"

echo
echo "=== 3. headless match ==="
if [ ! -f "$ROM" ]; then
    echo "    SKIPPED: no ROM present (never shipped; supply your own dump)"
    exit 0
fi
WORK=$(mktemp -d)
cp recomp/aot_boot_deny.txt "$WORK/"
( cd "$WORK" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    "$REPO/$BIN" --headless 900 --auto-start 180 "$REPO/$ROM" > run.log 2>&1 )
STATUS=$?
grep -E '^\[Frame (600|900)\]' "$WORK/run.log" | sed 's/^/    /'
tail -2 "$WORK/run.log" | sed 's/^/    /'
if [ $STATUS -ne 0 ] || ! grep -q 'Mode2: 0x08' "$WORK/run.log"; then
    echo "    FAIL: did not reach live match play" >&2
    rm -rf "$WORK"; exit 1
fi
rm -rf "$WORK"
echo "    OK: reached live match play"
