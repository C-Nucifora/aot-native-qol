#!/bin/sh
# Existing synthetic host checks only. No game inputs or filesystem installation.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/checks
compiler=${CC:-clang}
for name in repeat camera ui-clock settings native-state commit; do
 case "$name" in
 repeat) units=src/repeat_adapter.c;;
 camera) units=src/camera_rate.c;;
 ui-clock) units=src/ui_clock.c;;
 settings) units='src/settings.c src/settings_io.c src/sidecar.c';;
 native-state) units='src/native_fps_state.c src/settings.c';;
 commit) units=src/fps_commit.c;;
 esac
 "$compiler" -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-ignored-attributes \
  -UNDEBUG -DAOT_API= -fsanitize=address,undefined -Iinclude \
  "checks/$name-check.c" $units -lm -o "build/checks/$name"
 "build/checks/$name"
done
printf '%s\n' 'PASS six existing portable host checks; no game data or process access'
