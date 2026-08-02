#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

if command -v reuse >/dev/null 2>&1; then
    exec reuse lint
fi
if command -v uvx >/dev/null 2>&1; then
    exec uvx --from reuse reuse lint
fi

echo "License validation requires reuse or uvx." >&2
exit 1
