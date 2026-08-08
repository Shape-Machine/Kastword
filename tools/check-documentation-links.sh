#!/bin/sh
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
    echo "Usage: $0 DOCUMENTATION_ROOT" >&2
    exit 2
fi

documentation_root=$(CDPATH= cd -- "$1" && pwd -P)
documents=$(mktemp)
targets=$(mktemp)
trap 'cmake -E rm -f "$documents" "$targets"' EXIT HUP INT TERM

if [ -f "$documentation_root/README.md" ]; then
    printf '%s\n' "$documentation_root/README.md" > "$documents"
fi
if [ -d "$documentation_root/docs" ]; then
    find "$documentation_root/docs" -type f -name '*.md' -print >> "$documents"
fi

while IFS= read -r document; do
    awk -v document="$document" '
        { text = text " " $0 }
        END {
            markdown = text
            html = text
            while (match(markdown, /\]\([^)]*\)/)) {
                print document "\t" substr(markdown, RSTART + 2, RLENGTH - 3)
                markdown = substr(markdown, RSTART + RLENGTH)
            }
            while (match(html, /src="[^"]+"/)) {
                print document "\t" substr(html, RSTART + 5, RLENGTH - 6)
                html = substr(html, RSTART + RLENGTH)
            }
        }
    ' "$document"
done < "$documents" > "$targets"

while IFS="	" read -r document destination; do
    case "$destination" in
        ''|'#'*|http://*|https://*|mailto:*) continue ;;
    esac

    relative_path=${destination%%#*}
    fragment=
    if [ "$relative_path" != "$destination" ]; then
        fragment=${destination#*#}
    fi
    resolved_directory=$(CDPATH= cd -- "$(dirname -- "$document")" && pwd -P)
    resolved_path="$resolved_directory/$relative_path"
    if [ ! -f "$resolved_path" ]; then
        echo "$document: broken relative link: $destination" >&2
        exit 1
    fi

    if [ -n "$fragment" ] && ! awk -v wanted="$fragment" '
        /^#{1,6}[[:space:]]/ {
            heading = $0
            sub(/^#+[[:space:]]+/, "", heading)
            heading = tolower(heading)
            gsub(/[^a-z0-9 _-]/, "", heading)
            gsub(/[ _]+/, "-", heading)
            if (heading == wanted)
                found = 1
        }
        END { exit !found }
    ' "$resolved_path"; then
        echo "$document: missing heading for link: $destination" >&2
        exit 1
    fi
done < "$targets"
