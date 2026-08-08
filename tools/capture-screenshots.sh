#!/bin/sh
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then
    echo "Usage: $0 SCREENSHOT_GENERATOR" >&2
    exit 2
fi

generator=$1
repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
output_directory="$repository_root/screenshots"

case "$output_directory" in
    "$repository_root/screenshots") ;;
    *) echo "Refusing unsafe screenshot output path: $output_directory" >&2; exit 2 ;;
esac

stage_directory=$(mktemp -d "$repository_root/.screenshots-stage.XXXXXX")
backup_directory="$repository_root/.screenshots-backup.$$"
stage_exists=true
backup_exists=false

cleanup() {
    if [ "$stage_exists" = true ]; then
        cmake -E remove_directory "$stage_directory"
    fi
    if [ "$backup_exists" = true ]; then
        cmake -E remove_directory "$backup_directory"
    fi
}
trap cleanup EXIT HUP INT TERM

"$generator" "$stage_directory"

for screenshot in \
    offline-dictation.png \
    speech-models.png \
    audio-input.png \
    settings.png
do
    test -f "$stage_directory/$screenshot"
done
test "$(find "$stage_directory" -mindepth 1 -maxdepth 1 | wc -l)" -eq 4

if [ -e "$backup_directory" ] || [ -L "$backup_directory" ]; then
    echo "Refusing to overwrite screenshot backup path: $backup_directory" >&2
    exit 2
fi

if [ -e "$output_directory" ] || [ -L "$output_directory" ]; then
    mv -- "$output_directory" "$backup_directory"
    backup_exists=true
fi

if mv -- "$stage_directory" "$output_directory"; then
    stage_exists=false
    if [ "$backup_exists" = true ]; then
        cmake -E remove_directory "$backup_directory"
        backup_exists=false
    fi
else
    if [ "$backup_exists" = true ]; then
        mv -- "$backup_directory" "$output_directory"
        backup_exists=false
    fi
    exit 1
fi

echo "Wrote screenshots to $output_directory"
