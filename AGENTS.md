<!-- SPDX-FileCopyrightText: 2026 Sri Rang -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Development guidelines

- Keep the UI minimal. Prefer native KDE and Qt patterns and follow the KDE Human Interface
  Guidelines instead of introducing custom UI conventions.
- Run `make lint` and `make test` while developing, and run `make validate` before handing off a
  change. New tests must be deterministic, non-flaky, and protect meaningful behavior or prevent a
  regression. Do not add tests merely to increase the coverage number.
