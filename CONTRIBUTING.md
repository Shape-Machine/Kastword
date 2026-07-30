<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Contributing to Kastword

Kastword is an early-stage KDE Plasma application. Bug reports, focused fixes, documentation,
and testing across Plasma Wayland and X11 are welcome.

## Before opening an issue

- Search existing issues.
- Confirm the problem still occurs on the current `main` branch.
- Include Plasma, Qt, KDE Frameworks, session type, and Kastword versions.
- Never attach private recordings or dictated text without reviewing them first.
- Use the security process in `SECURITY.md` for vulnerabilities.

## Development setup

Install the dependencies listed in `README.md`, then build without the large default model when
working on UI or infrastructure code:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DKASTWORD_FETCH_DEFAULT_MODEL=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Run `make format` before submitting C++ changes and `make validate` when all validation tools are
installed. Keep changes narrowly scoped and explain user-visible behavior in the pull request.

Contributions are accepted under `GPL-3.0-or-later`. Add your own
`SPDX-FileCopyrightText` line to substantially changed or newly created files.

