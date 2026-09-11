#!/usr/bin/env bash
# Android APK build, driven by build-all.sh.
#
# Produces arm64-v8a (every current Android handheld, including the Retroid
# Pockets) and x86_64 (so the app can run in the emulator, which is x86_64 only
# on an x86 host). No ROM is bundled: the user copies their own cartridge dump
# into the app's external directory over USB.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$LOCALAPPDATA/Android/Sdk}}"
# LOCALAPPDATA is a Windows path; cygpath turns it into one bash can stat.
command -v cygpath >/dev/null && SDK="$(cygpath -u "$SDK" 2>/dev/null || printf %s "$SDK")"
[ -d "$SDK" ] || { echo "[build-android] Android SDK not found at '$SDK' (set ANDROID_SDK_ROOT)" >&2; exit 1; }

"$REPO/scripts/fetch-sdl2.sh"

# local.properties is machine-specific, so it is generated rather than tracked.
printf 'sdk.dir=%s\n' "$SDK" > "$REPO/android/local.properties"

cd "$REPO/android"
GRADLE="./gradlew"
[ -x "$GRADLE" ] || GRADLE="gradle"
command -v "${GRADLE#./}" >/dev/null 2>&1 || [ -x "$GRADLE" ] \
    || { echo "[build-android] no gradle wrapper and no gradle on PATH" >&2; exit 1; }

"$GRADLE" --no-daemon assembleRelease

APK=$(find "$REPO/android/app/build/outputs/apk" -name '*.apk' | head -1)
[ -n "$APK" ] || { echo "[build-android] gradle produced no APK" >&2; exit 1; }
echo "[build-android] $APK"

# The ABIs are the whole point of the build; report what actually landed
# rather than trusting the config.
if command -v unzip >/dev/null; then
    echo "[build-android] native libraries in the APK:"
    unzip -l "$APK" | awk '/lib\/.*\.so/ {print "                " $4}'
fi
