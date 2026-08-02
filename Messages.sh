#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

output_dir="${podir:-po}"
read -r -a xgettext_command <<< "${XGETTEXT:-xgettext}"
mapfile -t sources < <(find src -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.qml' \) -print | sort)

mkdir -p "${output_dir}"
"${xgettext_command[@]}" \
    --from-code=UTF-8 \
    --language=C++ \
    --keyword=i18n:1 \
    --keyword=i18nc:1c,2 \
    --keyword=i18np:1,2 \
    --keyword=i18ncp:1c,2,3 \
    --package-name=kastword \
    --output="${output_dir}/kastword.pot" \
    "${sources[@]}"
