#!/bin/sh
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then
    echo "Usage: $0 SCREENSHOT_GENERATOR" >&2
    exit 2
fi

fixture_root=$(mktemp -d)
trap 'cmake -E remove_directory "$fixture_root"' EXIT HUP INT TERM
output="$fixture_root/output"
cmake -E make_directory "$output"

if QT_QPA_PLATFORM=offscreen "$1" "$output" > "$fixture_root/log" 2>&1; then
    echo "Headless screenshot capture unexpectedly succeeded." >&2
    exit 1
fi

grep -F "requires an active graphical Plasma session" "$fixture_root/log"
test -z "$(find "$output" -mindepth 1 -maxdepth 1 -print -quit)"
