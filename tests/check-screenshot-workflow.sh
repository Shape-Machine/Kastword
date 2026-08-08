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
    'for file in offline-dictation.png speech-models.png audio-input.png settings.png; do' \
    '    printf fixture > "$1/$file"' \
    'done' > "$fixture_generator"
chmod +x "$fixture_generator"

"$fixture_capture" "$fixture_generator"

for screenshot in \
    offline-dictation.png \
    speech-models.png \
    audio-input.png \
    settings.png
do
    test -f "$fixture_root/screenshots/$screenshot"
done
test "$(find "$fixture_root/screenshots" -mindepth 1 -maxdepth 1 | wc -l)" -eq 4

cmake -E touch "$fixture_root/screenshots/stale.png"
"$fixture_capture" "$fixture_generator"
test ! -e "$fixture_root/screenshots/stale.png"

before=$(sha256sum "$fixture_root/screenshots"/*.png)
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

cmake -E remove_directory "$fixture_root/screenshots"
outside_directory="$fixture_root/outside"
cmake -E make_directory "$outside_directory"
cmake -E touch "$outside_directory/sentinel"
ln -s "$outside_directory" "$fixture_root/screenshots"
"$fixture_capture" "$fixture_generator"
test -f "$outside_directory/sentinel"
test ! -L "$fixture_root/screenshots"
test -f "$fixture_root/screenshots/offline-dictation.png"
