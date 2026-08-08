#!/bin/sh
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 CAPTURE_SCRIPT" >&2
    exit 2
fi

capture_script=$1
fixture_root=$(mktemp -d)
trap 'cmake -E remove_directory "$fixture_root"' EXIT HUP INT TERM
cmake -E make_directory "$fixture_root/tools"
cmake -E copy "$capture_script" "$fixture_root/tools/capture-screenshots.sh"
chmod +x "$fixture_root/tools/capture-screenshots.sh"
fixture_capture="$fixture_root/tools/capture-screenshots.sh"
fixture_generator="$fixture_root/success-generator"
printf '%s\n' \
    '#!/bin/sh' \
    'for file in 01-offline-dictation.png 02-speech-models.png 03-audio-input.png 04-settings.png; do' \
    '    printf fixture > "$1/$file"' \
    'done' > "$fixture_generator"
chmod +x "$fixture_generator"

real_cmake=$(command -v cmake)
cmake_state="$fixture_root/cmake-interrupted"
cmake_wrapper_directory="$fixture_root/cmake-wrapper"
cmake -E make_directory "$cmake_wrapper_directory"
printf '%s\n' \
    '#!/bin/sh' \
    'case "${3-}" in' \
    '    */.screenshots-transaction.*/stage)' \
    '        if [ "${1-}" = -E ] && [ "${2-}" = make_directory ] && [ ! -e "$CMAKE_STATE" ]; then' \
    '            "$REAL_CMAKE" -E touch "$CMAKE_STATE"' \
    '            kill -TERM "$PPID"' \
    '            exit 143' \
    '        fi' \
    '        ;;' \
    'esac' \
    'exec "$REAL_CMAKE" "$@"' > "$cmake_wrapper_directory/cmake"
chmod +x "$cmake_wrapper_directory/cmake"
if PATH="$cmake_wrapper_directory:$PATH" REAL_CMAKE="$real_cmake" CMAKE_STATE="$cmake_state" \
    "$fixture_capture" "$fixture_generator"; then
    echo "An interrupted transaction setup unexpectedly succeeded." >&2
    exit 1
fi
test ! -e "$fixture_root/.screenshots-transaction."*

"$fixture_capture" "$fixture_generator"

for screenshot in \
    01-offline-dictation.png \
    02-speech-models.png \
    03-audio-input.png \
    04-settings.png
do
    test -f "$fixture_root/screenshots/$screenshot"
done
test "$(find "$fixture_root/screenshots" -mindepth 1 -maxdepth 1 | wc -l)" -eq 4

serial_generator="$fixture_root/serial-generator"
printf '%s\n' \
    '#!/bin/sh' \
    'if ! mkdir "$SERIAL_ROOT/active"; then exit 1; fi' \
    'trap '\''rmdir "$SERIAL_ROOT/active"'\'' EXIT HUP INT TERM' \
    'cmake -E touch "$SERIAL_ROOT/entered"' \
    'sleep 0.2' \
    'for file in 01-offline-dictation.png 02-speech-models.png 03-audio-input.png 04-settings.png; do' \
    '    printf serialized > "$1/$file"' \
    'done' > "$serial_generator"
chmod +x "$serial_generator"
SERIAL_ROOT="$fixture_root" "$fixture_capture" "$serial_generator" &
first_capture=$!
attempt=0
while [ ! -e "$fixture_root/entered" ] && [ "$attempt" -lt 100 ]; do
    sleep 0.01
    attempt=$((attempt + 1))
done
test -e "$fixture_root/entered"
SERIAL_ROOT="$fixture_root" "$fixture_capture" "$serial_generator" &
second_capture=$!
wait "$first_capture"
wait "$second_capture"
test "$(find "$fixture_root/screenshots" -mindepth 1 -maxdepth 1 | wc -l)" -eq 4

cmake -E touch "$fixture_root/screenshots/stale.png"
"$fixture_capture" "$fixture_generator"
test ! -e "$fixture_root/screenshots/stale.png"

before=$(sha256sum "$fixture_root/screenshots"/*.png)
stale_backup="$fixture_root/.screenshots-backup.$$"
cmake -E make_directory "$stale_backup"
cmake -E touch "$stale_backup/sentinel"
failure_generator="$fixture_root/failure-generator"
printf '%s\n' '#!/bin/sh' 'cmake -E touch "$1/partial.png"' 'exit 1' > "$failure_generator"
chmod +x "$failure_generator"
if "$fixture_capture" "$failure_generator"; then
    echo "A failed capture unexpectedly succeeded." >&2
    exit 1
fi
after=$(sha256sum "$fixture_root/screenshots"/*.png)
test "$before" = "$after"
test ! -e "$fixture_root/screenshots/partial.png"
test -f "$stale_backup/sentinel"

real_mv=$(command -v mv)
cmake -E make_directory "$fixture_root/fake-bin"
interrupting_mv="$fixture_root/fake-bin/mv"
printf '%s\n' \
    '#!/bin/sh' \
    '"$REAL_MV" "$@"' \
    'if [ ! -e "$MV_STATE" ]; then' \
    '    cmake -E touch "$MV_STATE"' \
    '    kill -TERM "$PPID"' \
    'fi' > "$interrupting_mv"
chmod +x "$interrupting_mv"
before=$(sha256sum "$fixture_root/screenshots"/*.png)
if PATH="$fixture_root/fake-bin:$PATH" REAL_MV="$real_mv" \
    MV_STATE="$fixture_root/mv-interrupted" "$fixture_capture" "$fixture_generator"; then
    echo "An interrupted replacement unexpectedly succeeded." >&2
    exit 1
fi
after=$(sha256sum "$fixture_root/screenshots"/*.png)
test "$before" = "$after"
test ! -e "$fixture_root/.screenshots-transaction."*

cmake -E remove_directory "$fixture_root/screenshots"
outside_directory="$fixture_root/outside"
cmake -E make_directory "$outside_directory"
cmake -E touch "$outside_directory/sentinel"
ln -s "$outside_directory" "$fixture_root/screenshots"
"$fixture_capture" "$fixture_generator"
test -f "$outside_directory/sentinel"
test ! -L "$fixture_root/screenshots"
test -f "$fixture_root/screenshots/01-offline-dictation.png"
