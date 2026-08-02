#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

catalog_dir="$(mktemp -d)"
trap 'rm -rf "${catalog_dir}"' EXIT

podir="${catalog_dir}" ./Messages.sh
msgcmp --use-fuzzy po/x-test/kastword.po "${catalog_dir}/kastword.pot"
if msgattrib --untranslated po/x-test/kastword.po | grep -q '^msgid '; then
    echo "po/x-test/kastword.po contains untranslated messages" >&2
    exit 1
fi
