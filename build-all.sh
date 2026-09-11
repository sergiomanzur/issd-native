#!/usr/bin/env bash
#
# Build every ISSD Native port with one command.
#
#   ./build-all.sh                  Windows + Linux + SteamOS
#   ./build-all.sh --only linux     just one target
#   ./build-all.sh --skip steamos   everything except one target
#   ./build-all.sh --list           show what each target needs
#
# Targets and where they run:
#
#   windows  native on this Windows host        -> dist/windows
#   linux    WSL (fast iteration, host glibc)   -> dist/linux
#   steamos  Docker, Steam Linux Runtime 3.0    -> dist/steamos
#            "sniper" SDK (Debian 12, glibc 2.36)
#   android  Gradle + NDK, arm64-v8a + x86_64   -> dist/android
#
# A build that cannot run its target FAILS. It never quietly produces fewer
# artifacts than asked for: silently skipping a target is how a stale binary
# gets shipped.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WSL_DISTRO_DEFAULT="Ubuntu-24.04"
WSL_DISTRO="${ISSD_WSL_DISTRO:-$WSL_DISTRO_DEFAULT}"
SNIPER_IMAGE="${ISSD_SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest}"

ALL_TARGETS=(windows linux steamos android)
TARGETS=("${ALL_TARGETS[@]}")

die()  { printf '\n[build-all] ERROR: %s\n' "$*" >&2; exit 1; }
note() { printf '[build-all] %s\n' "$*"; }
rule() { printf '\n======== %s ========\n' "$*"; }

usage() {
    sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --only)  [ $# -ge 2 ] || die "--only needs a target"; TARGETS=("$2"); shift 2 ;;
        --skip)  [ $# -ge 2 ] || die "--skip needs a target"
                 NEW=(); for t in "${TARGETS[@]}"; do [ "$t" = "$2" ] || NEW+=("$t"); done
                 TARGETS=("${NEW[@]}"); shift 2 ;;
        --list|-h|--help) usage ;;
        *) die "unknown argument '$1' (try --help)" ;;
    esac
done

for t in "${TARGETS[@]}"; do
    case "$t" in
        windows|linux|steamos|android) ;;
        *) die "unknown target '$t' (expected: ${ALL_TARGETS[*]})" ;;
    esac
done

# `wsl.exe <cmd>` inherits this shell's variable expansion under Git Bash and
# MSYS rewrites /mnt/... into a Windows path, so every Linux-side step is
# handed over as a script file rather than an inline command string.
wsl_run() {
    local script="$1"
    MSYS_NO_PATHCONV=1 wsl.exe -d "$WSL_DISTRO" -- bash "$script"
}

stage() {   # stage <target> <file>...
    local target="$1"; shift
    mkdir -p "$REPO/dist/$target"
    for f in "$@"; do
        [ -e "$f" ] && cp -f "$f" "$REPO/dist/$target/"
    done
}

build_windows() {
    rule "windows"
    command -v cmake >/dev/null || die "cmake not on PATH"
    cmake --preset windows    >/dev/null || die "windows: configure failed"
    cmake --build --preset windows        || die "windows: build failed"
    stage windows "$REPO/build/ISSDNative.exe" "$REPO/build/SDL2.dll" \
                  "$REPO/recomp/aot_boot_deny.txt"
    note "windows -> dist/windows"
}

build_linux() {
    rule "linux"
    command -v wsl.exe >/dev/null || die "linux: wsl.exe not found. Install WSL, or --skip linux"
    MSYS_NO_PATHCONV=1 wsl.exe -d "$WSL_DISTRO" -- true 2>/dev/null \
        || die "linux: WSL distro '$WSL_DISTRO' unavailable (set ISSD_WSL_DISTRO)"
    wsl_run "$(wslpath_of "$REPO/scripts/build-linux.sh")" || die "linux: build failed"
    stage linux "$REPO/build-linux/ISSDNative" "$REPO/recomp/aot_boot_deny.txt"
    note "linux -> dist/linux"
}

build_steamos() {
    rule "steamos"
    command -v docker >/dev/null || die "steamos: docker not found. Start Docker Desktop, or --skip steamos"
    docker info >/dev/null 2>&1  || die "steamos: Docker daemon is not running. Start Docker Desktop, or --skip steamos"
    note "using $SNIPER_IMAGE"
    MSYS_NO_PATHCONV=1 docker run --rm -v "$(cygpath_of "$REPO")":/src -w /src "$SNIPER_IMAGE" \
        bash /src/scripts/build-steamos.sh || die "steamos: container build failed"
    stage steamos "$REPO/build-steamos/ISSDNative" "$REPO/recomp/aot_boot_deny.txt" \
                  "$REPO/scripts/steamos-run.sh"
    note "steamos -> dist/steamos"
}

build_android() {
    rule "android"
    local sdk="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$LOCALAPPDATA/Android/Sdk}}"
    # LOCALAPPDATA is a Windows path; cygpath turns it into one bash can stat.
    command -v cygpath >/dev/null && sdk="$(cygpath -u "$sdk" 2>/dev/null || printf %s "$sdk")"
    [ -d "$sdk" ] || die "android: Android SDK not found at '$sdk'. Set ANDROID_SDK_ROOT, or --skip android"
    bash "$REPO/scripts/build-android.sh" || die "android: build failed"
    local apk
    apk=$(find "$REPO/android/app/build/outputs/apk" -name '*.apk' 2>/dev/null | head -1)
    [ -n "$apk" ] || die "android: no APK produced"
    stage android "$apk"
    note "android -> dist/android"
}

# MSYS_NO_PATHCONV stops Git Bash rewriting container-side absolute paths
# (-w /src and the script argument) into Windows paths before docker sees them.
# /c/foo -> C:/foo for Docker volume mounts; identity elsewhere.
cygpath_of() {
    if command -v cygpath >/dev/null; then cygpath -w "$1" | tr '\\' '/'; else printf '%s' "$1"; fi
}
# C:/foo -> /mnt/c/foo for handing a script path to WSL.
wslpath_of() {
    printf '%s' "$1" | sed -E 's#^/([a-zA-Z])/#/mnt/\1/#; s#^([a-zA-Z]):/#/mnt/\L\1/#'
}

FAILED=()
for t in "${TARGETS[@]}"; do
    case "$t" in
        windows) build_windows || FAILED+=(windows) ;;
        linux)   build_linux   || FAILED+=(linux) ;;
        steamos) build_steamos || FAILED+=(steamos) ;;
        android) build_android || FAILED+=(android) ;;
    esac
done

rule "summary"
for t in "${TARGETS[@]}"; do
    if [ -d "$REPO/dist/$t" ] && [ -n "$(ls -A "$REPO/dist/$t" 2>/dev/null)" ]; then
        printf '  %-8s OK   dist/%s\n' "$t" "$t"
    else
        printf '  %-8s FAIL\n' "$t"
    fi
done
[ ${#FAILED[@]} -eq 0 ] || die "targets failed: ${FAILED[*]}"
note "all requested targets built"
