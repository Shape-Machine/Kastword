#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

build_dir="${1:-build-coverage}"
report_dir="${2:-${build_dir}/coverage}"
minimum_line="${COVERAGE_MIN_LINE:-68}"
minimum_branch="${COVERAGE_MIN_BRANCH:-55}"

if command -v gcovr >/dev/null 2>&1; then
    gcovr_command=(gcovr)
elif command -v uvx >/dev/null 2>&1; then
    gcovr_command=(uvx --from gcovr gcovr)
else
    echo "Coverage requires gcovr or uvx." >&2
    exit 1
fi

mkdir -p "${report_dir}"
"${gcovr_command[@]}" \
    "${build_dir}" \
    --root . \
    --filter src/ \
    --gcov-exclude '.*(qrc_|qmlcache|qmltyperegistrations|mocs_compilation|Main_qml).*' \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --html-details "${report_dir}/index.html" \
    --xml "${report_dir}/coverage.xml" \
    --txt "${report_dir}/summary.txt" \
    --print-summary \
    --fail-under-line "${minimum_line}" \
    --fail-under-branch "${minimum_branch}"

echo "Coverage report: ${report_dir}/index.html"
