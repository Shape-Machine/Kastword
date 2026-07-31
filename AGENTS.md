<!-- SPDX-FileCopyrightText: 2026 Sri Rang -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Development guidelines

- Keep the UI minimal. Prefer native KDE and Qt patterns, preserve behavior already supplied by the
  platform, and follow the KDE Human Interface Guidelines instead of introducing custom UI
  conventions.

## Git workflow

- Make all changes on a focused feature or fix branch; never commit directly to `main`.
- Use concise, imperative commit messages.
- After merging a pull request, switch to `main`, pull with `--ff-only`, and delete its merged local
  and remote feature branches.

## Validation

- Run `make lint` and `make test` while developing.
- New tests must be deterministic, non-flaky, and protect meaningful behavior or prevent a
  regression. Do not add tests merely to increase the coverage number.
- After the final changes, run the complete `make validate` target before opening a pull request or
  handing off the work. If `reuse` is unavailable but `uvx` exists, locate its temporary executable
  with `uvx --from reuse which reuse`, prepend that executable's directory to `PATH`, and then run
  `make validate`. Report validation as passing only when every required check succeeds.

## Repository conventions

- Keep both the root `LICENSE` file and the `LICENSES/` directory: the root file supports GitHub
  license detection, while `LICENSES/` supports REUSE compliance.
- Search for references in source, build files, documentation, packaging, and CI before renaming or
  deleting files.

## Pull requests

- Before opening a pull request, ensure the working tree is clean, use a concise imperative title,
  and include a short summary and the exact validation commands run in its description.
- Before recommending or performing a merge, confirm that all required GitHub Actions checks passed
  and GitHub reports the pull request as mergeable.
