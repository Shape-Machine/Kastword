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

exec 9< "$repository_root"
flock 9

transaction_directory=
stage_directory=
backup_directory=
replacement_complete=false
preserve_transaction=false

cleanup() {
    status=$?
    trap - EXIT HUP INT TERM

    if [ -n "$transaction_directory" ] && [ "$replacement_complete" != true ]; then
        if [ -e "$backup_directory" ] || [ -L "$backup_directory" ]; then
            if [ -e "$output_directory" ] || [ -L "$output_directory" ]; then
                cmake -E remove_directory "$output_directory"
            fi
            if ! mv -- "$backup_directory" "$output_directory"; then
                preserve_transaction=true
                status=1
            fi
        elif [ ! -e "$stage_directory" ] \
            && { [ -e "$output_directory" ] || [ -L "$output_directory" ]; }; then
            cmake -E remove_directory "$output_directory"
        fi
    fi

    if [ -n "$transaction_directory" ] && [ "$preserve_transaction" != true ]; then
        cmake -E remove_directory "$transaction_directory"
    fi

    exit "$status"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

transaction_directory=$(mktemp -d "$repository_root/.screenshots-transaction.XXXXXX")
stage_directory="$transaction_directory/stage"
backup_directory="$transaction_directory/backup"
cmake -E make_directory "$stage_directory"

"$generator" "$stage_directory"

for screenshot in \
    01-offline-dictation.png \
    02-history.png \
    03-speech-models.png \
    04-audio-input.png \
    05-settings.png
do
    test -f "$stage_directory/$screenshot"
done
test "$(find "$stage_directory" -mindepth 1 -maxdepth 1 | wc -l)" -eq 5

if [ -e "$output_directory" ] || [ -L "$output_directory" ]; then
    mv -- "$output_directory" "$backup_directory"
fi

if ! mv -- "$stage_directory" "$output_directory"; then
    exit 1
fi
replacement_complete=true

echo "Wrote screenshots to $output_directory"
