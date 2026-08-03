#!/bin/sh
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

prefix=${1:?installation prefix is required}
desktop_file=${2:?desktop file is required}

case "$prefix" in
  "" | /)
    echo "Refusing unsafe installation prefix: $prefix" >&2
    exit 1
    ;;
esac

desktop-file-validate "$desktop_file"
test "$(sed -n 's/^Exec=//p' "$desktop_file")" = "kastword"
resolved_binary=$(PATH="$prefix/bin:$PATH" command -v kastword)
test "$resolved_binary" = "$prefix/bin/kastword"
QT_QPA_PLATFORM=offscreen "$resolved_binary" --smoke-test
