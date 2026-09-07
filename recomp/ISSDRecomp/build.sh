#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" --config Release --parallel
printf '
%s
' 'Built the generated-code static library.'
printf '%s
' 'No playable executable was produced; see README.md under "Continue the port".'
