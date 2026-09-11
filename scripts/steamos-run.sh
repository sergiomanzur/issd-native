#!/usr/bin/env bash
# Launcher for the SteamOS build. Add this file (not the ELF) as a Non-Steam
# Game so Steam Input and the Deck's controls are bound before the game starts.
#
# The ROM is never shipped. Put your own dump next to this script, pass it as
# an argument, or let the picker find one (needs zenity or kdialog, which are
# present in SteamOS Desktop Mode but not in Gaming Mode -- so on a Deck,
# prefer setting rom_path in the config or passing it here).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
chmod +x ./ISSDNative 2>/dev/null || true
exec ./ISSDNative "$@"
